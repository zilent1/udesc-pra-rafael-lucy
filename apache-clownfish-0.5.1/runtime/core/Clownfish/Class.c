/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_CFISH_CLASS
#define C_CFISH_OBJ
#define C_CFISH_STRING
#define C_CFISH_METHOD
#define CFISH_USE_SHORT_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "Clownfish/Class.h"
#include "Clownfish/String.h"
#include "Clownfish/Boolean.h"
#include "Clownfish/CharBuf.h"
#include "Clownfish/Err.h"
#include "Clownfish/Hash.h"
#include "Clownfish/LockFreeRegistry.h"
#include "Clownfish/Method.h"
#include "Clownfish/Vector.h"
#include "Clownfish/Util/Atomic.h"
#include "Clownfish/Util/Memory.h"

#define NovelMethSpec            cfish_NovelMethSpec
#define OverriddenMethSpec       cfish_OverriddenMethSpec
#define InheritedMethSpec        cfish_InheritedMethSpec
#define ClassSpec                cfish_ClassSpec

uint32_t Class_offset_of_parent = offsetof(Class, parent);

static void
S_set_name(Class *self, const char *utf8, size_t size);

static Method*
S_find_method(Class *self, const char *meth_name);

static int32_t
S_claim_parcel_id(void);

static LockFreeRegistry *Class_registry;
cfish_Class_bootstrap_hook1_t cfish_Class_bootstrap_hook1;

void
Class_bootstrap(const cfish_ClassSpec *specs, size_t num_specs,
                const cfish_NovelMethSpec *novel_specs,
                const cfish_OverriddenMethSpec *overridden_specs,
                const cfish_InheritedMethSpec *inherited_specs) {
    int32_t parcel_id = S_claim_parcel_id();

    /* Pass 1:
     * - Allocate memory.
     * - Initialize global Class pointers.
     */
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *parent = spec->parent ? *spec->parent : NULL;

        uint32_t novel_offset = parent
                                ? parent->class_alloc_size
                                : offsetof(Class, vtable);
        uint32_t class_alloc_size = novel_offset
                                    + spec->num_novel_meths
                                      * sizeof(cfish_method_t);

        Class *klass = (Class*)Memory_wrapped_calloc(class_alloc_size, 1);

        // Needed to calculate size of subclasses.
        klass->class_alloc_size = class_alloc_size;

        if (spec->klass == &CLASS) {
            // `obj_alloc_size` is used by Init_Obj to zero the object. In the
            // next pass, this method is called to initialize the Class
            // objects. The Class struct is zeroed already, so this isn't
            // crucial, but let's set the correct value here.
            klass->obj_alloc_size = offsetof(Class, vtable);
        }

        // Initialize the global pointer to the Class.
        *spec->klass = klass;
    }

    /* Pass 2:
     * - Initialize IVARS_OFFSET.
     * - Initialize 'klass' ivar and refcount by calling Init_Obj.
     * - Initialize parent, flags, obj_alloc_size, class_alloc_size.
     * - Assign parcel_id.
     * - Initialize method pointers and offsets.
     */
    uint32_t num_novel      = 0;
    uint32_t num_overridden = 0;
    uint32_t num_inherited  = 0;
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *klass  = *spec->klass;
        Class *parent = spec->parent ? *spec->parent : NULL;

        uint32_t ivars_offset = 0;
        if (spec->ivars_offset_ptr != NULL) {
            if (parent) {
                Class *ancestor = parent;
                while (ancestor && ancestor->parcel_id == parcel_id) {
                    ancestor = ancestor->parent;
                }
                ivars_offset = ancestor ? ancestor->obj_alloc_size : 0;
                *spec->ivars_offset_ptr = ivars_offset;
            }
            else {
                *spec->ivars_offset_ptr = 0;
            }
        }

        // Init_Obj clears all klass ivars, so `class_alloc_size` must be
        // recalculated.
        Class_Init_Obj_IMP(CLASS, klass);

        uint32_t novel_offset = parent
                                ? parent->class_alloc_size
                                : offsetof(Class, vtable);
        uint32_t class_alloc_size = novel_offset
                                    + spec->num_novel_meths
                                      * sizeof(cfish_method_t);

        klass->parent           = parent;
        klass->parcel_id        = parcel_id;
        klass->class_alloc_size = class_alloc_size;

        if (klass == CLASS) {
            // Don't account for vtable array.
            klass->obj_alloc_size = offsetof(Class, vtable);
        }
        else {
            klass->obj_alloc_size = ivars_offset + spec->ivars_size;
        }
        if (cfish_Class_bootstrap_hook1 != NULL) {
            cfish_Class_bootstrap_hook1(klass);
        }

        klass->flags = 0;
        if (klass == CLASS
            || klass == METHOD
            || klass == BOOLEAN
            || klass == STRING
           ) {
            klass->flags |= CFISH_fREFCOUNTSPECIAL;
        }
        if (spec->flags & cfish_ClassSpec_FINAL) {
            klass->flags |= CFISH_fFINAL;
        }

        if (parent) {
            // Copy parent vtable.
            uint32_t parent_vt_size = parent->class_alloc_size
                                      - offsetof(Class, vtable);
            memcpy(klass->vtable, parent->vtable, parent_vt_size);
        }

        for (size_t i = 0; i < spec->num_inherited_meths; ++i) {
            const InheritedMethSpec *mspec = &inherited_specs[num_inherited++];
            *mspec->offset = *mspec->parent_offset;
        }

        for (size_t i = 0; i < spec->num_overridden_meths; ++i) {
            const OverriddenMethSpec *mspec
                = &overridden_specs[num_overridden++];
            *mspec->offset = *mspec->parent_offset;
            Class_Override_IMP(klass, mspec->func, *mspec->offset);
        }

        for (size_t i = 0; i < spec->num_novel_meths; ++i) {
            const NovelMethSpec *mspec = &novel_specs[num_novel++];
            *mspec->offset = novel_offset;
            novel_offset += sizeof(cfish_method_t);
            Class_Override_IMP(klass, mspec->func, *mspec->offset);
        }
    }

    /* Now it's safe to call methods.
     *
     * Pass 3:
     * - Inititalize name and method array.
     * - Register class.
     */
    num_novel      = 0;
    num_overridden = 0;
    num_inherited  = 0;
    for (size_t i = 0; i < num_specs; ++i) {
        const ClassSpec *spec = &specs[i];
        Class *klass = *spec->klass;

        S_set_name(klass, spec->name, strlen(spec->name));
        klass->methods = (Method**)MALLOCATE((spec->num_novel_meths + 1)
                                             * sizeof(Method*));

        // Only store novel methods for now.
        for (size_t i = 0; i < spec->num_novel_meths; ++i) {
            const NovelMethSpec *mspec = &novel_specs[num_novel++];
            String *name = SSTR_WRAP_C(mspec->name);
            Method *method = Method_new(name, mspec->callback_func,
                                        *mspec->offset);
            klass->methods[i] = method;
        }

        klass->methods[spec->num_novel_meths] = NULL;

        Class_add_to_registry(klass);
    }
}

void
Class_Destroy_IMP(Class *self) {
    THROW(ERR, "Insane attempt to destroy Class for class '%o'", self->name);
}

void
Class_Override_IMP(Class *self, cfish_method_t method, uint32_t offset) {
    union { char *char_ptr; cfish_method_t *func_ptr; } pointer;
    pointer.char_ptr = ((char*)self) + offset;
    pointer.func_ptr[0] = method;
}

String*
Class_Get_Name_IMP(Class *self) {
    return self->name;
}

Class*
Class_Get_Parent_IMP(Class *self) {
    return self->parent;
}

uint32_t
Class_Get_Obj_Alloc_Size_IMP(Class *self) {
    return self->obj_alloc_size;
}

Vector*
Class_Get_Methods_IMP(Class *self) {
    Vector *retval = Vec_new(0);

    for (size_t i = 0; self->methods[i]; ++i) {
        Vec_Push(retval, INCREF(self->methods[i]));
    }

    return retval;
}

void
Class_init_registry() {
    LockFreeRegistry *reg = LFReg_new(256);
    if (Atomic_cas_ptr((void*volatile*)&Class_registry, NULL, reg)) {
        return;
    }
    else {
        DECREF(reg);
    }
}

static Class*
S_simple_subclass(Class *parent, String *name) {
    if (parent->flags & CFISH_fFINAL) {
        THROW(ERR, "Can't subclass final class %o", Class_Get_Name(parent));
    }

    Class *subclass
        = (Class*)Memory_wrapped_calloc(parent->class_alloc_size, 1);
    Class_Init_Obj(parent->klass, subclass);

    subclass->parent           = parent;
    subclass->flags            = parent->flags;
    subclass->parcel_id        = parent->parcel_id;
    subclass->obj_alloc_size   = parent->obj_alloc_size;
    subclass->class_alloc_size = parent->class_alloc_size;
    subclass->methods          = (Method**)CALLOCATE(1, sizeof(Method*));

    S_set_name(subclass, Str_Get_Ptr8(name), Str_Get_Size(name));

    memcpy(subclass->vtable, parent->vtable,
           parent->class_alloc_size - offsetof(Class, vtable));

    return subclass;
}

Class*
Class_singleton(String *class_name, Class *parent) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }

    Class *singleton = (Class*)LFReg_fetch(Class_registry, class_name);
    if (singleton == NULL) {
        Vector *fresh_host_methods;
        size_t num_fresh;

        if (parent == NULL) {
            String *parent_class = Class_find_parent_class(class_name);
            if (parent_class == NULL) {
                THROW(ERR, "Class '%o' doesn't descend from %o", class_name,
                      OBJ->name);
            }
            else {
                parent = Class_singleton(parent_class, NULL);
                DECREF(parent_class);
            }
        }

        singleton = S_simple_subclass(parent, class_name);

        // Allow host methods to override.
        fresh_host_methods = Class_fresh_host_methods(class_name);
        num_fresh = Vec_Get_Size(fresh_host_methods);
        if (num_fresh) {
            Hash *meths = Hash_new(num_fresh);
            for (size_t i = 0; i < num_fresh; i++) {
                String *meth = (String*)Vec_Fetch(fresh_host_methods, i);
                Hash_Store(meths, meth, (Obj*)CFISH_TRUE);
            }
            for (Class *klass = parent; klass; klass = klass->parent) {
                for (size_t i = 0; klass->methods[i]; i++) {
                    Method *method = klass->methods[i];
                    if (method->callback_func) {
                        String *name = Method_Host_Name(method);
                        if (Hash_Fetch(meths, name)) {
                            Class_Override(singleton, method->callback_func,
                                            method->offset);
                        }
                        DECREF(name);
                    }
                }
            }
            DECREF(meths);
        }
        DECREF(fresh_host_methods);

        // Register the new class, both locally and with host.
        if (Class_add_to_registry(singleton)) {
            // Doing this after registering is racy, but hard to fix. :(
            Class_register_with_host(singleton, parent);
        }
        else {
            DECREF(singleton);
            singleton = (Class*)LFReg_fetch(Class_registry, class_name);
            if (!singleton) {
                THROW(ERR, "Failed to either insert or fetch Class for '%o'",
                      class_name);
            }
        }
    }

    return singleton;
}

bool
Class_add_to_registry(Class *klass) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }
    if (LFReg_fetch(Class_registry, klass->name)) {
        return false;
    }
    else {
        return LFReg_register(Class_registry, klass->name, (Obj*)klass);
    }
}

bool
Class_add_alias_to_registry(Class *klass, const char *alias_ptr,
                            size_t alias_len) {
    if (Class_registry == NULL) {
        Class_init_registry();
    }
    String *alias = SSTR_WRAP_UTF8(alias_ptr, alias_len);
    if (LFReg_fetch(Class_registry, alias)) {
        return false;
    }
    else {
        return LFReg_register(Class_registry, alias, (Obj*)klass);
    }
}

Class*
Class_fetch_class(String *class_name) {
    Class *klass = NULL;
    if (Class_registry != NULL) {
        klass = (Class*)LFReg_fetch(Class_registry, class_name);
    }
    return klass;
}

void
Class_Add_Host_Method_Alias_IMP(Class *self, const char *alias,
                             const char *meth_name) {
    Method *method = S_find_method(self, meth_name);
    if (!method) {
        fprintf(stderr, "Method %s not found\n", meth_name);
        abort();
    }
    String *string = SSTR_WRAP_C(alias);
    Method_Set_Host_Alias(method, string);
}

void
Class_Exclude_Host_Method_IMP(Class *self, const char *meth_name) {
    Method *method = S_find_method(self, meth_name);
    if (!method) {
        fprintf(stderr, "Method %s not found\n", meth_name);
        abort();
    }
    method->is_excluded = true;
}

static void
S_set_name(Class *self, const char *utf8, size_t size) {
    /*
     * We use a "wrapped" String for `name` because it's effectively
     * threadsafe: the sole reference is owned by an immortal object and any
     * INCREF spawns a copy.
     */
    self->name_internal = Str_new_from_trusted_utf8(utf8, size);
    self->name = Str_new_wrap_trusted_utf8(Str_Get_Ptr8(self->name_internal),
                                           Str_Get_Size(self->name_internal));
}

static Method*
S_find_method(Class *self, const char *name) {
    size_t name_len = strlen(name);

    for (size_t i = 0; self->methods[i]; i++) {
        Method *method = self->methods[i];
        if (Str_Equals_Utf8(method->name, name, name_len)) {
            return method;
        }
    }

    return NULL;
}

static size_t parcel_count;

static int32_t
S_claim_parcel_id(void) {
    // TODO: use ordinary cas rather than cas_ptr.
    union { size_t num; void *ptr; } old_value;
    union { size_t num; void *ptr; } new_value;

    bool succeeded = false;
    do {
        old_value.num = parcel_count;
        new_value.num = old_value.num + 1;
        succeeded = Atomic_cas_ptr((void*volatile*)&parcel_count,
                                   old_value.ptr, new_value.ptr);
    } while (!succeeded);

    return (int32_t)new_value.num;
}


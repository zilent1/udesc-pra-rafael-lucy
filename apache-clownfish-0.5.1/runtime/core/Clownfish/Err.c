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

#define C_CFISH_ERR
#define C_CFISH_OBJ
#define C_CFISH_CLASS
#define CFISH_USE_SHORT_NAMES

#include "charmony.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "Clownfish/Err.h"

#include "Clownfish/CharBuf.h"
#include "Clownfish/String.h"
#include "Clownfish/Class.h"
#include "Clownfish/Util/Memory.h"

Err*
Err_new(String *mess) {
    Err *self = (Err*)Class_Make_Obj(ERR);
    return Err_init(self, mess);
}

Err*
Err_init(Err *self, String *mess) {
    self->mess = mess;
    return self;
}

void
Err_Destroy_IMP(Err *self) {
    DECREF(self->mess);
    SUPER_DESTROY(self, ERR);
}

String*
Err_To_String_IMP(Err *self) {
    return (String*)INCREF(self->mess);
}

void
Err_Cat_Mess_IMP(Err *self, String *mess) {
    String *new_mess = Str_Cat(self->mess, mess);
    DECREF(self->mess);
    self->mess = new_mess;
}

// Fallbacks in case variadic macros aren't available.
#ifndef CHY_HAS_VARIADIC_MACROS
static String*
S_str_vnewf(char *pattern, va_list args) {
    CharBuf *buf = CB_new(strlen(pattern) + 10);
    CB_VCatF(buf, pattern, args);
    String *message = CB_Yield_String(buf);
    DECREF(buf);
    return message;
}
void
THROW(Class *klass, char *pattern, ...) {
    va_list args;

    va_start(args, pattern);
    String *message = S_str_vnewf(pattern, args);
    va_end(args);

    Err *err = (Err*)Class_Make_Obj(klass);
    err = Err_init(err, message);
    Err_do_throw(err);
}
void
CFISH_WARN(char *pattern, ...) {
    va_list args;

    va_start(args, pattern);
    String *message = S_str_vnewf(pattern, args);
    va_end(args);

    Err_warn_mess(message);
}
String*
CFISH_MAKE_MESS(char *pattern, ...) {
    va_list args;

    va_start(args, pattern);
    String *message = S_str_vnewf(pattern, args);
    va_end(args);

    return message;
}
#endif


static String*
S_vmake_mess(const char *file, int line, const char *func,
             const char *pattern, va_list args) {
    size_t guess_len = strlen(file)
                       + (func ? strlen(func) : 0)
                       + strlen(pattern)
                       + 30;
    CharBuf *buf = CB_new(guess_len);
    CB_VCatF(buf, pattern, args);
    if (func != NULL) {
        CB_catf(buf, "\n\t%s at %s line %i32\n", func, file, (int32_t)line);
    }
    else {
        CB_catf(buf, "\n\t%s line %i32\n", file, (int32_t)line);
    }
    String *message = CB_Yield_String(buf);
    DECREF(buf);
    return message;
}

String*
Err_make_mess(const char *file, int line, const char *func,
              const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    String *message = S_vmake_mess(file, line, func, pattern, args);
    va_end(args);
    return message;
}

void
Err_warn_at(const char *file, int line, const char *func,
            const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    String *message = S_vmake_mess(file, line, func, pattern, args);
    va_end(args);
    Err_warn_mess(message);
}

String*
Err_Get_Mess_IMP(Err *self) {
    return self->mess;
}

void
Err_Add_Frame_IMP(Err *self, const char *file, int line, const char *func) {
    CharBuf *buf = CB_new(0);
    CB_Cat(buf, self->mess);

    if (!Str_Ends_With_Utf8(self->mess, "\n", 1)) {
        CB_Cat_Char(buf, '\n');
    }

    if (func != NULL) {
        CB_catf(buf, "\t%s at %s line %i32\n", func, file, (int32_t)line);
    }
    else {
        CB_catf(buf, "\tat %s line %i32\n", file, (int32_t)line);
    }

    DECREF(self->mess);
    self->mess = CB_Yield_String(buf);
    DECREF(buf);
}

void
Err_rethrow(Err *self, const char *file, int line, const char *func) {
    Err_Add_Frame_IMP(self, file, line, func);
    Err_do_throw(self);
}

void
Err_throw_at(Class *klass, const char *file, int line,
             const char *func, const char *pattern, ...) {
    va_list args;

    va_start(args, pattern);
    String *message = S_vmake_mess(file, line, func, pattern, args);
    va_end(args);

    Err *err = (Err*)Class_Make_Obj(klass);
    err = Err_init(err, message);
    Err_do_throw(err);
}

// Inlined, slightly optimized version of Obj_is_a.
static CFISH_INLINE bool
SI_obj_is_a(Obj *obj, Class *target_class) {
    Class *klass = obj->klass;

    while (klass != NULL) {
        if (klass == target_class) {
            return true;
        }
        klass = klass->parent;
    }

    return false;
}

Obj*
Err_downcast(Obj *obj, Class *klass, const char *file, int line,
             const char *func) {
    if (obj && !SI_obj_is_a(obj, klass)) {
        Err_throw_at(ERR, file, line, func, "Can't downcast from %o to %o",
                     Obj_get_class_name(obj), Class_Get_Name(klass));
    }
    return obj;
}

Obj*
Err_certify(Obj *obj, Class *klass, const char *file, int line,
            const char *func) {
    if (!obj) {
        Err_throw_at(ERR, file, line, func, "Object isn't a %o, it's NULL",
                     Class_Get_Name(klass));
    }
    else if (!SI_obj_is_a(obj, klass)) {
        Err_throw_at(ERR, file, line, func, "Can't downcast from %o to %o",
                     Obj_get_class_name(obj), Class_Get_Name(klass));
    }
    return obj;
}

void
Err_abstract_method_call(Obj *obj, Class *klass, const char *method_name) {
    String *class_name = obj ? Obj_get_class_name(obj) : Class_Get_Name(klass);
    THROW(ERR, "Abstract method '%s' not defined by %o", method_name,
          class_name);
}

void
Err_invalid_callback(const char *method_name) {
    THROW(ERR, "Can't override %s via binding", method_name);
}

#ifdef CHY_HAS_WINDOWS_H

#include <windows.h>

char*
Err_win_error() {
    DWORD buf_size = 256;
    char *buf = (char*)MALLOCATE(buf_size);
    DWORD message_len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                                      NULL,       // message source table
                                      GetLastError(),
                                      0,          // language id
                                      buf,
                                      buf_size,
                                      NULL        // empty va_list
                                     );
    if (message_len == 0) {
        char unknown[] = "Unknown error";
        size_t len = sizeof(unknown);
        strncpy(buf, unknown, len);
    }
    else if (message_len > 1) {
        // Kill stupid newline.
        buf[message_len - 2] = '\0';
    }
    return buf;
}

#else

char*
Err_win_error() {
    return NULL; // Never called.
}

#endif // CHY_HAS_WINDOWS_H



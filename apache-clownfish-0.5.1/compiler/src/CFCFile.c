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

#include "charmony.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifndef true
    #define true 1
    #define false 0
#endif

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCFile.h"
#include "CFCFileSpec.h"
#include "CFCParcel.h"
#include "CFCUtil.h"
#include "CFCClass.h"

struct CFCFile {
    CFCBase base;
    CFCParcel *parcel;
    CFCBase **blocks;
    CFCClass **classes;
    CFCFileSpec *spec;
    int modified;
    char *guard_name;
    char *guard_start;
    char *guard_close;
};

static const CFCMeta CFCFILE_META = {
    "Clownfish::CFC::Model::File",
    sizeof(CFCFile),
    (CFCBase_destroy_t)CFCFile_destroy
};

CFCFile*
CFCFile_new(CFCParcel *parcel, CFCFileSpec *spec) {

    CFCFile *self = (CFCFile*)CFCBase_allocate(&CFCFILE_META);
    return CFCFile_init(self, parcel, spec);
}

CFCFile*
CFCFile_init(CFCFile *self, CFCParcel *parcel, CFCFileSpec *spec) {
    CFCUTIL_NULL_CHECK(parcel);
    CFCUTIL_NULL_CHECK(spec);
    self->parcel     = (CFCParcel*)CFCBase_incref((CFCBase*)parcel);
    self->modified   = false;
    self->spec       = (CFCFileSpec*)CFCBase_incref((CFCBase*)spec);
    self->blocks     = (CFCBase**)CALLOCATE(1, sizeof(CFCBase*));
    self->classes    = (CFCClass**)CALLOCATE(1, sizeof(CFCBase*));

    // Derive include guard name, plus C code for opening and closing the
    // guard.
    const char *path_part = CFCFileSpec_get_path_part(self->spec);
    size_t len = strlen(path_part);
    self->guard_name = (char*)MALLOCATE(len + sizeof("H_") + 1);
    memcpy(self->guard_name, "H_", 2);
    size_t i, j;
    for (i = 0, j = 2; i < len; i++) {
        char c = path_part[i];
        if (c == CHY_DIR_SEP_CHAR) {
            self->guard_name[j++] = '_';
        }
        else if (isalnum(c)) {
            self->guard_name[j++] = toupper(c);
        }
    }
    self->guard_name[j] = '\0';
    self->guard_start = CFCUtil_sprintf("#ifndef %s\n#define %s 1\n",
                                        self->guard_name, self->guard_name);
    self->guard_close = CFCUtil_sprintf("#endif /* %s */\n", self->guard_name);

    return self;
}

void
CFCFile_destroy(CFCFile *self) {
    CFCBase_decref((CFCBase*)self->parcel);
    for (size_t i = 0; self->blocks[i] != NULL; i++) {
        CFCBase_decref(self->blocks[i]);
    }
    FREEMEM(self->blocks);
    for (size_t i = 0; self->classes[i] != NULL; i++) {
        CFCBase_decref((CFCBase*)self->classes[i]);
    }
    FREEMEM(self->classes);
    FREEMEM(self->guard_name);
    FREEMEM(self->guard_start);
    FREEMEM(self->guard_close);
    CFCBase_decref((CFCBase*)self->spec);
    CFCBase_destroy((CFCBase*)self);
}

void
CFCFile_add_block(CFCFile *self, CFCBase *block) {
    CFCUTIL_NULL_CHECK(block);
    const char *cfc_class = CFCBase_get_cfc_class(block);

    // Add to classes array if the block is a CFCClass.
    if (strcmp(cfc_class, "Clownfish::CFC::Model::Class") == 0) {
        size_t num_class_blocks = 0;
        while (self->classes[num_class_blocks] != NULL) {
            num_class_blocks++;
        }
        num_class_blocks++;
        size_t size = (num_class_blocks + 1) * sizeof(CFCClass*);
        self->classes = (CFCClass**)REALLOCATE(self->classes, size);
        self->classes[num_class_blocks - 1]
            = (CFCClass*)CFCBase_incref(block);
        self->classes[num_class_blocks] = NULL;
    }

    // Add to blocks array.
    if (strcmp(cfc_class, "Clownfish::CFC::Model::Class") == 0
        || strcmp(cfc_class, "Clownfish::CFC::Model::Parcel") == 0
        || strcmp(cfc_class, "Clownfish::CFC::Model::CBlock") == 0
       ) {
        size_t num_blocks = 0;
        while (self->blocks[num_blocks] != NULL) {
            num_blocks++;
        }
        num_blocks++;
        size_t size = (num_blocks + 1) * sizeof(CFCBase*);
        self->blocks = (CFCBase**)REALLOCATE(self->blocks, size);
        self->blocks[num_blocks - 1] = CFCBase_incref(block);
        self->blocks[num_blocks] = NULL;
    }
    else {
        CFCUtil_die("Wrong kind of object: '%s'", cfc_class);
    }
}

static char*
S_some_path(CFCFile *self, const char *base_dir, const char *ext) {
    const char *path_part = CFCFileSpec_get_path_part(self->spec);
    char *buf;
    if (base_dir) {
        buf = CFCUtil_sprintf("%s" CHY_DIR_SEP "%s%s", base_dir,
                              path_part, ext);
    }
    else {
        buf = CFCUtil_sprintf("%s%s", path_part, ext);
    }
    for (size_t i = 0; buf[i] != '\0'; i++) {
        #ifdef _WIN32
        if (buf[i] == '/') { buf[i] = '\\'; }
        #else
        if (buf[i] == '\\') { buf[i] = '/'; }
        #endif
    }
    return buf;
}

char*
CFCFile_c_path(CFCFile *self, const char *base_dir) {
    return S_some_path(self, base_dir, ".c");
}

char*
CFCFile_h_path(CFCFile *self, const char *base_dir) {
    return S_some_path(self, base_dir, ".h");
}

char*
CFCFile_cfh_path(CFCFile *self, const char *base_dir) {
    return S_some_path(self, base_dir, ".cfh");
}

CFCParcel*
CFCFile_get_parcel(CFCFile *self) {
    return self->parcel;
}

CFCBase**
CFCFile_blocks(CFCFile *self) {
    return self->blocks;
}

CFCClass**
CFCFile_classes(CFCFile *self) {
    return self->classes;
}

void
CFCFile_set_modified(CFCFile *self, int modified) {
    self->modified = !!modified;
}

int
CFCFile_get_modified(CFCFile *self) {
    return self->modified;
}

const char*
CFCFile_get_source_dir(CFCFile *self) {
    return CFCFileSpec_get_source_dir(self->spec);
}

const char*
CFCFile_get_path_part(CFCFile *self) {
    return CFCFileSpec_get_path_part(self->spec);
}

int
CFCFile_included(CFCFile *self) {
    return CFCFileSpec_included(self->spec);
}

const char*
CFCFile_guard_name(CFCFile *self) {
    return self->guard_name;
}

const char*
CFCFile_guard_start(CFCFile *self) {
    return self->guard_start;
}

const char*
CFCFile_guard_close(CFCFile *self) {
    return self->guard_close;
}


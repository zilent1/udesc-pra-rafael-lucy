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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCParser.h"
#include "CFCParcel.h"
#include "CFCFile.h"
#include "CFCFileSpec.h"
#include "CFCUtil.h"
#include "CFCMemPool.h"
#include "CFCLexHeader.h"
#include "CFCParseHeader.h"

#ifndef true
  #define true 1
  #define false 0
#endif

struct CFCParser {
    CFCBase base;
    void *header_parser;
    struct CFCBase *result;
    int errors;
    int lineno;
    char *class_name;
    int class_is_final;
    CFCFileSpec *file_spec;
    CFCMemPool *pool;
    CFCParcel  *parcel;
};

static const CFCMeta CFCPARSER_META = {
    "Clownfish::CFC::Parser",
    sizeof(CFCParser),
    (CFCBase_destroy_t)CFCParser_destroy
};

CFCParser*
CFCParser_new(void) {
    CFCParser *self = (CFCParser*)CFCBase_allocate(&CFCPARSER_META);
    return CFCParser_init(self);
}

CFCParser*
CFCParser_init(CFCParser *self) {
    self->header_parser = CFCParseHeaderAlloc(malloc);
    if (self->header_parser == NULL) {
        CFCUtil_die("Failed to allocate header parser");
    }
    self->result         = NULL;
    self->errors         = false;
    self->lineno         = 0;
    self->class_name     = NULL;
    self->file_spec      = NULL;
    self->pool           = NULL;
    self->parcel         = NULL;
    return self;
}

void
CFCParser_destroy(CFCParser *self) {
    CFCParseHeaderFree(self->header_parser, free);
    FREEMEM(self->class_name);
    CFCBase_decref((CFCBase*)self->file_spec);
    CFCBase_decref((CFCBase*)self->pool);
    CFCBase_decref(self->result);
    CFCBase_decref((CFCBase*)self->parcel);
    CFCBase_destroy((CFCBase*)self);
}

CFCParser *CFCParser_current_state  = NULL;
void      *CFCParser_current_parser = NULL;

CFCBase*
CFCParser_parse(CFCParser *self, const char *string) {
    self->pool = CFCMemPool_new(0);

    // Make Lemon-based parser and parser state available from Flex-based scanner.
    CFCParser_current_state  = self;
    CFCParser_current_parser = self->header_parser;

    // Zero out, then parse.
    self->errors = false;
    self->lineno = 0;
    YY_BUFFER_STATE buffer = yy_scan_bytes(string, (int)strlen(string));
    yylex();
    yy_delete_buffer(buffer);

    // Finish up.
    CFCParseHeader(CFCParser_current_parser, 0, NULL, self);
    CFCBase_decref((CFCBase*)self->pool);
    self->pool = NULL;
    CFCBase *result = self->result;
    self->result = NULL;
    if (self->errors) {
        CFCBase_decref((CFCBase*)result);
        result = NULL;
    }

    yylex_destroy();
    return result;
}

CFCFile*
CFCParser_parse_file(CFCParser *self, const char *string,
                     CFCFileSpec *file_spec) {
    CFCParser_set_parcel(self, NULL);
    CFCParser_set_file_spec(self, file_spec);
    CFCParseHeader(self->header_parser, CFC_TOKENTYPE_FILE_START, NULL, self);
    CFCFile *result = (CFCFile*)CFCParser_parse(self, string);
    CFCParser_set_file_spec(self, NULL);
    return result;
}

char*
CFCParser_dupe(CFCParser *self, const char *string) {
    size_t len = strlen(string);
    char *dupe = (char*)CFCMemPool_allocate(self->pool, len + 1);
    memcpy(dupe, string, len + 1);
    return dupe;
}

void*
CFCParser_allocate(CFCParser *self, size_t size) {
    return CFCMemPool_allocate(self->pool, size);
}

void
CFCParser_set_result(CFCParser *self, CFCBase *result) {
    CFCBase_decref(self->result);
    self->result = CFCBase_incref(result);
}

void
CFCParser_set_errors(CFCParser *self, int errors) {
    self->errors = errors;
}

void
CFCParser_set_lineno(CFCParser *self, int lineno) {
    self->lineno = lineno;
}

int
CFCParser_get_lineno(CFCParser *self) {
    return self->lineno;
}

void
CFCParser_set_parcel(CFCParser *self, CFCParcel *parcel) {
    CFCBase_incref((CFCBase*)parcel);
    CFCBase_decref((CFCBase*)self->parcel);
    self->parcel = parcel;
}

CFCParcel*
CFCParser_get_parcel(CFCParser *self) {
    return self->parcel;
}

void
CFCParser_set_class_name(CFCParser *self, const char *class_name) {
    FREEMEM(self->class_name);
    if (class_name) {
        self->class_name = CFCUtil_strdup(class_name);
    }
    else {
        self->class_name = NULL;
    }
}

const char*
CFCParser_get_class_name(CFCParser *self) {
    return self->class_name;
}

void
CFCParser_set_class_final(CFCParser *self, int is_final) {
    self->class_is_final = is_final;
}

int
CFCParser_get_class_final(CFCParser *self) {
    return self->class_is_final;
}

void
CFCParser_set_file_spec(CFCParser *self, CFCFileSpec *file_spec) {
    CFCBase_decref((CFCBase*)self->file_spec);
    self->file_spec = (CFCFileSpec*)CFCBase_incref((CFCBase*)file_spec);
}

CFCFileSpec*
CFCParser_get_file_spec(CFCParser *self) {
    return self->file_spec;
}


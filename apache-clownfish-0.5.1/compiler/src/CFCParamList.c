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

#include <string.h>

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCParamList.h"
#include "CFCVariable.h"
#include "CFCType.h"
#include "CFCSymbol.h"
#include "CFCUtil.h"

struct CFCParamList {
    CFCBase       base;
    CFCVariable **variables;
    char        **values;
    int           variadic;
    size_t        num_vars;
    char         *c_string;
    char         *name_list;
};

//
static void
S_generate_c_strings(CFCParamList *self);

static const CFCMeta CFCPARAMLIST_META = {
    "Clownfish::CFC::Model::ParamList",
    sizeof(CFCParamList),
    (CFCBase_destroy_t)CFCParamList_destroy
};

CFCParamList*
CFCParamList_new(int variadic) {
    CFCParamList *self = (CFCParamList*)CFCBase_allocate(&CFCPARAMLIST_META);
    return CFCParamList_init(self, variadic);
}

CFCParamList*
CFCParamList_init(CFCParamList *self, int variadic) {
    self->variadic  = variadic;
    self->num_vars  = 0;
    self->variables = (CFCVariable**)CALLOCATE(1, sizeof(void*));
    self->values    = (char**)CALLOCATE(1, sizeof(char*));
    self->c_string  = NULL;
    self->name_list = NULL;
    return self;
}

void
CFCParamList_resolve_types(CFCParamList *self) {
    for (size_t i = 0; self->variables[i]; ++i) {
        CFCVariable_resolve_type(self->variables[i]);
    }
}

void
CFCParamList_add_param(CFCParamList *self, CFCVariable *variable,
                       const char *value) {
    CFCUTIL_NULL_CHECK(variable);
    // It might be better to enforce that object parameters with a NULL
    // default are also nullable.
    if (value && strcmp(value, "NULL") == 0) {
        CFCType *type = CFCVariable_get_type(variable);
        CFCType_set_nullable(type, 1);
    }
    self->num_vars++;
    size_t amount = (self->num_vars + 1) * sizeof(void*);
    self->variables = (CFCVariable**)REALLOCATE(self->variables, amount);
    self->values    = (char**)REALLOCATE(self->values, amount);
    self->variables[self->num_vars - 1]
        = (CFCVariable*)CFCBase_incref((CFCBase*)variable);
    self->values[self->num_vars - 1] = value ? CFCUtil_strdup(value) : NULL;
    self->variables[self->num_vars] = NULL;
    self->values[self->num_vars] = NULL;
}

void
CFCParamList_destroy(CFCParamList *self) {
    for (size_t i = 0; i < self->num_vars; i++) {
        CFCBase_decref((CFCBase*)self->variables[i]);
        FREEMEM(self->values[i]);
    }
    FREEMEM(self->variables);
    FREEMEM(self->values);
    FREEMEM(self->c_string);
    FREEMEM(self->name_list);
    CFCBase_destroy((CFCBase*)self);
}

static void
S_generate_c_strings(CFCParamList *self) {
    size_t c_string_size = 1;
    size_t name_list_size = 1;

    // Calc space requirements and allocate memory.
    for (size_t i = 0; i < self->num_vars; i++) {
        CFCVariable *var = self->variables[i];
        c_string_size += sizeof(", ");
        c_string_size += strlen(CFCVariable_local_c(var));
        name_list_size += sizeof(", ");
        name_list_size += strlen(CFCVariable_get_name(var));
    }
    if (self->variadic) {
        c_string_size += sizeof(", ...");
    }
    if (self->num_vars == 0) {
        c_string_size += sizeof("void");
    }
    self->c_string  = (char*)MALLOCATE(c_string_size);
    self->name_list = (char*)MALLOCATE(name_list_size);
    self->c_string[0] = '\0';
    self->name_list[0] = '\0';

    // Build the strings.
    for (size_t i = 0; i < self->num_vars; i++) {
        CFCVariable *var = self->variables[i];
        strcat(self->c_string, CFCVariable_local_c(var));
        strcat(self->name_list, CFCVariable_get_name(var));
        if (i == self->num_vars - 1) {
            if (self->variadic) {
                strcat(self->c_string, ", ...");
            }
        }
        else {
            strcat(self->c_string, ", ");
            strcat(self->name_list, ", ");
        }
    }
    if (self->num_vars == 0) {
        strcat(self->c_string, "void");
    }
}

CFCVariable**
CFCParamList_get_variables(CFCParamList *self) {
    return self->variables;
}

const char**
CFCParamList_get_initial_values(CFCParamList *self) {
    return (const char**)self->values;
}

size_t
CFCParamList_num_vars(CFCParamList *self) {
    return self->num_vars;
}

void
CFCParamList_set_variadic(CFCParamList *self, int variadic) {
    self->variadic = !!variadic;
}

int
CFCParamList_variadic(CFCParamList *self) {
    return self->variadic;
}

const char*
CFCParamList_to_c(CFCParamList *self) {
    if (!self->c_string) { S_generate_c_strings(self); }
    return self->c_string;
}

const char*
CFCParamList_name_list(CFCParamList *self) {
    if (!self->name_list) { S_generate_c_strings(self); }
    return self->name_list;
}

const char*
CFCParamList_param_name(CFCParamList *self, int tick) {
    if ((size_t)tick >= self->num_vars) {
        CFCUtil_die("No var at position %d for ParamList (%s)", tick,
                    CFCParamList_to_c(self));
    }
    return CFCVariable_get_name(self->variables[tick]);
}

CFCType*
CFCParamList_param_type(CFCParamList *self, int tick) {
    if ((size_t)tick >= self->num_vars) {
        CFCUtil_die("No var at position %d for ParamList (%s)", tick,
                    CFCParamList_to_c(self));
    }
    return CFCVariable_get_type(self->variables[tick]);
}


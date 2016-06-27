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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifndef true
  #define true 1
  #define false 0
#endif

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCParcel.h"
#include "CFCFileSpec.h"
#include "CFCVersion.h"
#include "CFCUtil.h"

struct CFCParcel {
    CFCBase base;
    char *name;
    char *nickname;
    CFCVersion *version;
    CFCFileSpec *file_spec;
    char *prefix;
    char *Prefix;
    char *PREFIX;
    char *privacy_sym;
    int is_required;
    char **inherited_parcels;
    size_t num_inherited_parcels;
    char **struct_syms;
    size_t num_struct_syms;
    CFCPrereq **prereqs;
    size_t num_prereqs;
};

#define JSON_STRING 1
#define JSON_HASH   2
#define JSON_NULL   3

typedef struct JSONNode {
    int type;
    char *string;
    struct JSONNode **kids;
    size_t num_kids;
} JSONNode;

static void
S_set_prereqs(CFCParcel *self, JSONNode *node, const char *path);

static JSONNode*
S_parse_json_for_parcel(const char *json);

static JSONNode*
S_parse_json_hash(const char **json);

static JSONNode*
S_parse_json_string(const char **json);

static JSONNode*
S_parse_json_null(const char **json);

static void
S_skip_whitespace(const char **json);

static void
S_destroy_json(JSONNode *node);

static CFCParcel **registry = NULL;
static size_t num_registered = 0;

CFCParcel*
CFCParcel_fetch(const char *name) {
    for (size_t i = 0; i < num_registered ; i++) {
        CFCParcel *existing = registry[i];
        if (strcmp(existing->name, name) == 0) {
            return existing;
        }
    }

    return NULL;
}

void
CFCParcel_register(CFCParcel *self) {
    const char *name     = self->name;
    const char *nickname = self->nickname;

    for (size_t i = 0; i < num_registered ; i++) {
        CFCParcel *other = registry[i];

        if (strcmp(other->name, name) == 0) {
            CFCUtil_die("Parcel '%s' already registered", name);
        }
        if (strcmp(other->nickname, nickname) == 0) {
            CFCUtil_die("Parcel with nickname '%s' already registered",
                        nickname);
        }
    }

    size_t size = (num_registered + 2) * sizeof(CFCParcel*);
    registry = (CFCParcel**)REALLOCATE(registry, size);
    registry[num_registered++] = (CFCParcel*)CFCBase_incref((CFCBase*)self);
    registry[num_registered]   = NULL;
}

CFCParcel**
CFCParcel_all_parcels(void) {
    if (registry == NULL) {
        registry = (CFCParcel**)MALLOCATE(sizeof(CFCParcel*));
        registry[0] = NULL;
    }

    return registry;
}

void
CFCParcel_reap_singletons(void) {
    for (size_t i = 0; i < num_registered; i++) {
        CFCBase_decref((CFCBase*)registry[i]);
    }
    FREEMEM(registry);
    num_registered = 0;
    registry = NULL;
}

static int
S_validate_name_or_nickname(const char *orig) {
    const char *ptr = orig;
    for (; *ptr != 0; ptr++) {
        if (!isalpha(*ptr)) { return false; }
    }
    return true;
}

static const CFCMeta CFCPARCEL_META = {
    "Clownfish::CFC::Model::Parcel",
    sizeof(CFCParcel),
    (CFCBase_destroy_t)CFCParcel_destroy
};

CFCParcel*
CFCParcel_new(const char *name, const char *nickname, CFCVersion *version,
              CFCFileSpec *file_spec) {
    CFCParcel *self = (CFCParcel*)CFCBase_allocate(&CFCPARCEL_META);
    return CFCParcel_init(self, name, nickname, version, file_spec);
}

CFCParcel*
CFCParcel_init(CFCParcel *self, const char *name, const char *nickname,
               CFCVersion *version, CFCFileSpec *file_spec) {
    // Validate name.
    if (!name || !S_validate_name_or_nickname(name)) {
        CFCUtil_die("Invalid name: '%s'", name ? name : "[NULL]");
    }
    self->name = CFCUtil_strdup(name);

    // Validate or derive nickname.
    if (nickname) {
        if (!S_validate_name_or_nickname(nickname)) {
            CFCUtil_die("Invalid nickname: '%s'", nickname);
        }
        self->nickname = CFCUtil_strdup(nickname);
    }
    else {
        // Default nickname to name.
        self->nickname = CFCUtil_strdup(name);
    }

    // Default to version v0.
    if (version) {
        self->version = (CFCVersion*)CFCBase_incref((CFCBase*)version);
    }
    else {
        self->version = CFCVersion_new("v0");
    }

    // Set file_spec.
    self->file_spec = (CFCFileSpec*)CFCBase_incref((CFCBase*)file_spec);

    // Derive prefix, Prefix, PREFIX.
    size_t nickname_len  = strlen(self->nickname);
    size_t prefix_len = nickname_len ? nickname_len + 1 : 0;
    size_t amount     = prefix_len + 1;
    self->prefix = (char*)MALLOCATE(amount);
    self->Prefix = (char*)MALLOCATE(amount);
    self->PREFIX = (char*)MALLOCATE(amount);
    memcpy(self->Prefix, self->nickname, nickname_len);
    if (nickname_len) {
        self->Prefix[nickname_len]  = '_';
        self->Prefix[nickname_len + 1]  = '\0';
    }
    else {
        self->Prefix[nickname_len] = '\0';
    }
    for (size_t i = 0; i < amount; i++) {
        self->prefix[i] = tolower(self->Prefix[i]);
        self->PREFIX[i] = toupper(self->Prefix[i]);
    }
    self->prefix[prefix_len] = '\0';
    self->Prefix[prefix_len] = '\0';
    self->PREFIX[prefix_len] = '\0';

    // Derive privacy symbol.
    size_t privacy_sym_len = nickname_len + 4;
    self->privacy_sym = (char*)MALLOCATE(privacy_sym_len + 1);
    memcpy(self->privacy_sym, "CFP_", 4);
    for (size_t i = 0; i < nickname_len; i++) {
        self->privacy_sym[i+4] = toupper(self->nickname[i]);
    }
    self->privacy_sym[privacy_sym_len] = '\0';

    // Initialize flags.
    self->is_required = false;

    // Initialize arrays.
    self->inherited_parcels = (char**)CALLOCATE(1, sizeof(char*));
    self->num_inherited_parcels = 0;
    self->struct_syms = (char**)CALLOCATE(1, sizeof(char*));
    self->num_struct_syms = 0;
    self->prereqs = (CFCPrereq**)CALLOCATE(1, sizeof(CFCPrereq*));
    self->num_prereqs = 0;

    return self;
}

static CFCParcel*
S_new_from_json(const char *json, const char *path, CFCFileSpec *file_spec) {
    JSONNode *parsed = S_parse_json_for_parcel(json);
    if (!parsed) {
        CFCUtil_die("Invalid JSON parcel definition in '%s'", path);
    }
    if (parsed->type != JSON_HASH) {
        CFCUtil_die("Parcel definition must be a hash in '%s'", path);
    }
    const char *name     = NULL;
    const char *nickname = NULL;
    CFCVersion *version  = NULL;
    JSONNode   *prereqs  = NULL;
    for (size_t i = 0, max = parsed->num_kids; i < max; i += 2) {
        JSONNode *key   = parsed->kids[i];
        JSONNode *value = parsed->kids[i + 1];
        if (key->type != JSON_STRING) {
            CFCUtil_die("JSON parsing error (filepath '%s')", path);
        }
        if (strcmp(key->string, "name") == 0) {
            if (value->type != JSON_STRING) {
                CFCUtil_die("'name' must be a string (filepath %s)", path);
            }
            name = value->string;
        }
        else if (strcmp(key->string, "nickname") == 0) {
            if (value->type != JSON_STRING) {
                CFCUtil_die("'nickname' must be a string (filepath %s)",
                            path);
            }
            nickname = value->string;
        }
        else if (strcmp(key->string, "version") == 0) {
            if (value->type != JSON_STRING) {
                CFCUtil_die("'version' must be a string (filepath %s)",
                            path);
            }
            version = CFCVersion_new(value->string);
        }
        else if (strcmp(key->string, "prerequisites") == 0) {
            if (value->type != JSON_HASH) {
                CFCUtil_die("'prerequisites' must be a hash (filepath %s)",
                            path);
            }
            prereqs = value;
        }
        else {
            CFCUtil_die("Unrecognized key: '%s' (filepath '%s')",
                        key->string, path);
        }
    }
    if (!name) {
        CFCUtil_die("Missing required key 'name' (filepath '%s')", path);
    }
    if (!version) {
        CFCUtil_die("Missing required key 'version' (filepath '%s')", path);
    }
    CFCParcel *self = CFCParcel_new(name, nickname, version, file_spec);
    if (prereqs) {
        S_set_prereqs(self, prereqs, path);
    }
    CFCBase_decref((CFCBase*)version);

    S_destroy_json(parsed);
    return self;
}

static void
S_set_prereqs(CFCParcel *self, JSONNode *node, const char *path) {
    size_t num_prereqs = node->num_kids / 2;
    CFCPrereq **prereqs
        = (CFCPrereq**)MALLOCATE((num_prereqs + 1) * sizeof(CFCPrereq*));

    for (size_t i = 0; i < num_prereqs; ++i) {
        JSONNode *key = node->kids[2*i];
        if (key->type != JSON_STRING) {
            CFCUtil_die("Prereq key must be a string (filepath '%s')", path);
        }
        const char *name = key->string;

        JSONNode *value = node->kids[2*i+1];
        CFCVersion *version = NULL;
        if (value->type == JSON_STRING) {
            version = CFCVersion_new(value->string);
        }
        else if (value->type != JSON_NULL) {
            CFCUtil_die("Invalid prereq value (filepath '%s')", path);
        }

        prereqs[i] = CFCPrereq_new(name, version);

        CFCBase_decref((CFCBase*)version);
    }
    prereqs[num_prereqs] = NULL;

    // Assume that prereqs are empty.
    FREEMEM(self->prereqs);
    self->prereqs     = prereqs;
    self->num_prereqs = num_prereqs;
}

CFCParcel*
CFCParcel_new_from_json(const char *json, CFCFileSpec *file_spec) {
    return S_new_from_json(json, "[NULL]", file_spec);
}

CFCParcel*
CFCParcel_new_from_file(const char *path, CFCFileSpec *file_spec) {
    size_t len;
    char *json = CFCUtil_slurp_text(path, &len);
    CFCParcel *self = S_new_from_json(json, path, file_spec);
    FREEMEM(json);
    return self;
}

void
CFCParcel_destroy(CFCParcel *self) {
    FREEMEM(self->name);
    FREEMEM(self->nickname);
    CFCBase_decref((CFCBase*)self->version);
    CFCBase_decref((CFCBase*)self->file_spec);
    FREEMEM(self->prefix);
    FREEMEM(self->Prefix);
    FREEMEM(self->PREFIX);
    FREEMEM(self->privacy_sym);
    CFCUtil_free_string_array(self->inherited_parcels);
    CFCUtil_free_string_array(self->struct_syms);
    for (size_t i = 0; self->prereqs[i]; ++i) {
        CFCBase_decref((CFCBase*)self->prereqs[i]);
    }
    FREEMEM(self->prereqs);
    CFCBase_destroy((CFCBase*)self);
}

int
CFCParcel_equals(CFCParcel *self, CFCParcel *other) {
    if (strcmp(self->name, other->name)) { return false; }
    if (strcmp(self->nickname, other->nickname)) { return false; }
    if (CFCVersion_compare_to(self->version, other->version) != 0) {
        return false;
    }
    if (CFCParcel_included(self) != CFCParcel_included(other)) {
        return false;
    }
    return true;
}

const char*
CFCParcel_get_name(CFCParcel *self) {
    return self->name;
}

const char*
CFCParcel_get_nickname(CFCParcel *self) {
    return self->nickname;
}

CFCVersion*
CFCParcel_get_version(CFCParcel *self) {
    return self->version;
}

const char*
CFCParcel_get_prefix(CFCParcel *self) {
    return self->prefix;
}

const char*
CFCParcel_get_Prefix(CFCParcel *self) {
    return self->Prefix;
}

const char*
CFCParcel_get_PREFIX(CFCParcel *self) {
    return self->PREFIX;
}

const char*
CFCParcel_get_privacy_sym(CFCParcel *self) {
    return self->privacy_sym;
}

const char*
CFCParcel_get_cfp_path(CFCParcel *self) {
    return self->file_spec
           ? CFCFileSpec_get_path(self->file_spec)
           : NULL;
}

const char*
CFCParcel_get_source_dir(CFCParcel *self) {
    return self->file_spec
           ? CFCFileSpec_get_source_dir(self->file_spec)
           : NULL;
}

int
CFCParcel_included(CFCParcel *self) {
    return self->file_spec ? CFCFileSpec_included(self->file_spec) : false;
}

int
CFCParcel_required(CFCParcel *self) {
    return self->is_required;
}

void
CFCParcel_add_inherited_parcel(CFCParcel *self, CFCParcel *inherited) {
    const char *name     = CFCParcel_get_name(self);
    const char *inh_name = CFCParcel_get_name(inherited);

    if (strcmp(name, inh_name) == 0) { return; }

    for (size_t i = 0; self->inherited_parcels[i]; ++i) {
        const char *other_name = self->inherited_parcels[i];
        if (strcmp(other_name, inh_name) == 0) { return; }
    }

    size_t num_parcels = self->num_inherited_parcels;
    self->inherited_parcels
        = (char**)REALLOCATE(self->inherited_parcels,
                             (num_parcels + 2) * sizeof(char*));
    self->inherited_parcels[num_parcels]   = CFCUtil_strdup(inh_name);
    self->inherited_parcels[num_parcels+1] = NULL;
    self->num_inherited_parcels = num_parcels + 1;
}

CFCParcel**
CFCParcel_inherited_parcels(CFCParcel *self) {
    CFCParcel **parcels
        = (CFCParcel**)CALLOCATE(self->num_inherited_parcels + 1,
                                 sizeof(CFCParcel*));

    for (size_t i = 0; self->inherited_parcels[i]; ++i) {
        parcels[i] = CFCParcel_fetch(self->inherited_parcels[i]);
    }

    return parcels;
}

CFCPrereq**
CFCParcel_get_prereqs(CFCParcel *self) {
    return self->prereqs;
}

CFCParcel**
CFCParcel_prereq_parcels(CFCParcel *self) {
    CFCParcel **parcels
        = (CFCParcel**)CALLOCATE(self->num_prereqs + 1, sizeof(CFCParcel*));

    for (size_t i = 0; self->prereqs[i]; ++i) {
        const char *name = CFCPrereq_get_name(self->prereqs[i]);
        parcels[i] = CFCParcel_fetch(name);
    }

    return parcels;
}

void
CFCParcel_check_prereqs(CFCParcel *self) {
    // This is essentially a depth-first search of the dependency graph.
    // It might be possible to skip indirect dependencies, at least if
    // they're not part of the inheritance chain. But for now, all
    // dependencies are marked recursively.

    if (self->is_required) { return; }
    self->is_required = true;

    const char *name = CFCParcel_get_name(self);

    for (int i = 0; self->prereqs[i]; ++i) {
        CFCPrereq *prereq = self->prereqs[i];

        const char *req_name   = CFCPrereq_get_name(prereq);
        CFCParcel  *req_parcel = CFCParcel_fetch(req_name);
        if (!req_parcel) {
            // TODO: Add include path to error message.
            CFCUtil_die("Parcel '%s' required by '%s' not found", req_name,
                        name);
        }

        CFCVersion *version     = req_parcel->version;
        CFCVersion *req_version = CFCPrereq_get_version(prereq);
        if (CFCVersion_compare_to(version, req_version) < 0) {
            const char *vstring     = CFCVersion_get_vstring(version);
            const char *req_vstring = CFCVersion_get_vstring(req_version);
            CFCUtil_die("Version %s of parcel '%s' required by '%s' is lower"
                        " than required version %s",
                        vstring, req_name, name, req_vstring);
        }

        CFCParcel_check_prereqs(req_parcel);
    }
}

int
CFCParcel_has_prereq(CFCParcel *self, CFCParcel *parcel) {
    const char *name = CFCParcel_get_name(parcel);

    if (strcmp(CFCParcel_get_name(self), name) == 0) {
        return true;
    }

    for (int i = 0; self->prereqs[i]; ++i) {
        const char *prereq_name = CFCPrereq_get_name(self->prereqs[i]);
        if (strcmp(prereq_name, name) == 0) {
            return true;
        }
    }

    return false;
}

void
CFCParcel_add_struct_sym(CFCParcel *self, const char *struct_sym) {
    size_t num_struct_syms = self->num_struct_syms + 1;
    size_t size = (num_struct_syms + 1) * sizeof(char*);
    char **struct_syms = (char**)REALLOCATE(self->struct_syms, size);
    struct_syms[num_struct_syms-1] = CFCUtil_strdup(struct_sym);
    struct_syms[num_struct_syms]   = NULL;
    self->struct_syms     = struct_syms;
    self->num_struct_syms = num_struct_syms;
}

static CFCParcel*
S_lookup_struct_sym(CFCParcel *self, const char *struct_sym) {
    for (size_t i = 0; self->struct_syms[i]; ++i) {
        if (strcmp(self->struct_syms[i], struct_sym) == 0) {
            return self;
        }
    }

    return NULL;
}

CFCParcel*
CFCParcel_lookup_struct_sym(CFCParcel *self, const char *struct_sym) {
    CFCParcel *parcel = S_lookup_struct_sym(self, struct_sym);

    for (size_t i = 0; self->prereqs[i]; ++i) {
        const char *prereq_name   = CFCPrereq_get_name(self->prereqs[i]);
        CFCParcel  *prereq_parcel = CFCParcel_fetch(prereq_name);
        CFCParcel *maybe_parcel
            = S_lookup_struct_sym(prereq_parcel, struct_sym);

        if (maybe_parcel) {
            if (parcel) {
                CFCUtil_die("Type '%s' is ambigious", struct_sym);
            }
            parcel = maybe_parcel;
        }
    }

    return parcel;
}

int
CFCParcel_is_cfish(CFCParcel *self) {
    return !strcmp(self->prefix, "cfish_");
}

/**************************************************************************/

struct CFCPrereq {
    CFCBase base;
    char *name;
    CFCVersion *version;
};

static const CFCMeta CFCPREREQ_META = {
    "Clownfish::CFC::Model::Prereq",
    sizeof(CFCPrereq),
    (CFCBase_destroy_t)CFCPrereq_destroy
};

CFCPrereq*
CFCPrereq_new(const char *name, CFCVersion *version) {
    CFCPrereq *self = (CFCPrereq*)CFCBase_allocate(&CFCPREREQ_META);
    return CFCPrereq_init(self, name, version);
}

CFCPrereq*
CFCPrereq_init(CFCPrereq *self, const char *name, CFCVersion *version) {
    // Validate name.
    if (!name || !S_validate_name_or_nickname(name)) {
        CFCUtil_die("Invalid name: '%s'", name ? name : "[NULL]");
    }
    self->name = CFCUtil_strdup(name);

    // Default to version v0.
    if (version) {
        self->version = (CFCVersion*)CFCBase_incref((CFCBase*)version);
    }
    else {
        self->version = CFCVersion_new("v0");
    }

    return self;
}

void
CFCPrereq_destroy(CFCPrereq *self) {
    FREEMEM(self->name);
    CFCBase_decref((CFCBase*)self->version);
    CFCBase_destroy((CFCBase*)self);
}

const char*
CFCPrereq_get_name(CFCPrereq *self) {
    return self->name;
}

CFCVersion*
CFCPrereq_get_version(CFCPrereq *self) {
    return self->version;
}

/*****************************************************************************
 * The hack JSON parser coded up below is only meant to parse Clownfish parcel
 * file content.  It is limited in its capabilities because so little is legal
 * in a .cfp file.
 */

static JSONNode*
S_parse_json_for_parcel(const char *json) {
    if (!json) {
        return NULL;
    }
    S_skip_whitespace(&json);
    if (*json != '{') {
        return NULL;
    }
    JSONNode *parsed = S_parse_json_hash(&json);
    S_skip_whitespace(&json);
    if (*json != '\0') {
        S_destroy_json(parsed);
        parsed = NULL;
    }
    return parsed;
}

static void
S_append_kid(JSONNode *node, JSONNode *child) {
    size_t size = (node->num_kids + 2) * sizeof(JSONNode*);
    node->kids = (JSONNode**)realloc(node->kids, size);
    node->kids[node->num_kids++] = child;
    node->kids[node->num_kids]   = NULL;
}

static JSONNode*
S_parse_json_hash(const char **json) {
    const char *text = *json;
    S_skip_whitespace(&text);
    if (*text != '{') {
        return NULL;
    }
    text++;
    JSONNode *node = (JSONNode*)calloc(1, sizeof(JSONNode));
    node->type = JSON_HASH;
    while (1) {
        // Parse key.
        S_skip_whitespace(&text);
        if (*text == '}') {
            text++;
            break;
        }
        else if (*text == '"') {
            JSONNode *key = S_parse_json_string(&text);
            S_skip_whitespace(&text);
            if (!key || *text != ':') {
                S_destroy_json(node);
                return NULL;
            }
            text++;
            S_append_kid(node, key);
        }
        else {
            S_destroy_json(node);
            return NULL;
        }

        // Parse value.
        S_skip_whitespace(&text);
        JSONNode *value = NULL;
        if (*text == '"') {
            value = S_parse_json_string(&text);
        }
        else if (*text == '{') {
            value = S_parse_json_hash(&text);
        }
        else if (*text == 'n') {
            value = S_parse_json_null(&text);
        }
        if (!value) {
            S_destroy_json(node);
            return NULL;
        }
        S_append_kid(node, value);

        // Parse comma.
        S_skip_whitespace(&text);
        if (*text == ',') {
            text++;
        }
        else if (*text == '}') {
            text++;
            break;
        }
        else {
            S_destroy_json(node);
            return NULL;
        }
    }

    // Move pointer.
    *json = text;

    return node;
}

// Parse a double quoted string.  Don't allow escapes.
static JSONNode*
S_parse_json_string(const char **json) {
    const char *text = *json; 
    if (*text != '\"') {
        return NULL;
    }
    text++;
    const char *start = text;
    while (*text != '"') {
        if (*text == '\\' || *text == '\0') {
            return NULL;
        }
        text++;
    }
    JSONNode *node = (JSONNode*)calloc(1, sizeof(JSONNode));
    node->type = JSON_STRING;
    node->string = CFCUtil_strndup(start, text - start);

    // Move pointer.
    text++;
    *json = text;

    return node;
}

// Parse a JSON null value.
static JSONNode*
S_parse_json_null(const char **json) {
    static const char null_str[] = "null";

    if (strncmp(*json, null_str, sizeof(null_str) - 1) != 0) {
        return NULL;
    }

    JSONNode *node = (JSONNode*)calloc(1, sizeof(JSONNode));
    node->type = JSON_NULL;

    // Move pointer.
    *json += sizeof(null_str) - 1;

    return node;
}

static void
S_skip_whitespace(const char **json) {
    while (isspace(json[0][0])) { *json = *json + 1; }
}

static void
S_destroy_json(JSONNode *node) {
    if (!node) {
        return;
    }
    if (node->kids) {
        for (size_t i = 0; node->kids[i] != NULL; i++) {
            S_destroy_json(node->kids[i]);
        }
    }
    free(node->string);
    free(node->kids);
    free(node);
}


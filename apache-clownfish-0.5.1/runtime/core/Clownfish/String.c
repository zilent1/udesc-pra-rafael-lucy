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

#define C_CFISH_STRING
#define C_CFISH_STRINGITERATOR
#define CFISH_USE_SHORT_NAMES

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Clownfish/Class.h"
#include "Clownfish/String.h"

#include "Clownfish/ByteBuf.h"
#include "Clownfish/CharBuf.h"
#include "Clownfish/Err.h"
#include "Clownfish/Util/Memory.h"
#include "Clownfish/Util/StringHelper.h"

#define STACK_ITER(string, byte_offset) \
    S_new_stack_iter(alloca(sizeof(StringIterator)), string, byte_offset)

// Helper function for throwing invalid UTF-8 error. Since THROW uses
// a String internally, calling THROW with invalid UTF-8 would create an
// infinite loop -- so we fwrite some of the bogus text to stderr and
// invoke THROW with a generic message.
#define DIE_INVALID_UTF8(text, size) \
    S_die_invalid_utf8(text, size, __FILE__, __LINE__, CFISH_ERR_FUNC_MACRO)
static void
S_die_invalid_utf8(const char *text, size_t size, const char *file, int line,
                   const char *func);

static const char*
S_memmem(String *self, const char *substring, size_t size);

static StringIterator*
S_new_stack_iter(void *allocation, String *string, size_t byte_offset);

String*
Str_new_from_utf8(const char *utf8, size_t size) {
    if (!StrHelp_utf8_valid(utf8, size)) {
        DIE_INVALID_UTF8(utf8, size);
    }
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_from_trusted_utf8(self, utf8, size);
}

String*
Str_new_from_trusted_utf8(const char *utf8, size_t size) {
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_from_trusted_utf8(self, utf8, size);
}

String*
Str_init_from_trusted_utf8(String *self, const char *utf8, size_t size) {
    // Allocate.
    char *ptr = (char*)MALLOCATE(size + 1);

    // Copy.
    memcpy(ptr, utf8, size);
    ptr[size] = '\0'; // Null terminate.

    // Assign.
    self->ptr    = ptr;
    self->size   = size;
    self->origin = self;

    return self;
}

String*
Str_new_steal_utf8(char *utf8, size_t size) {
    if (!StrHelp_utf8_valid(utf8, size)) {
        DIE_INVALID_UTF8(utf8, size);
    }
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_steal_trusted_utf8(self, utf8, size);
}

String*
Str_new_steal_trusted_utf8(char *utf8, size_t size) {
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_steal_trusted_utf8(self, utf8, size);
}

String*
Str_init_steal_trusted_utf8(String *self, char *utf8, size_t size) {
    self->ptr    = utf8;
    self->size   = size;
    self->origin = self;
    return self;
}

String*
Str_new_wrap_utf8(const char *utf8, size_t size) {
    if (!StrHelp_utf8_valid(utf8, size)) {
        DIE_INVALID_UTF8(utf8, size);
    }
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_wrap_trusted_utf8(self, utf8, size);
}

String*
Str_new_wrap_trusted_utf8(const char *utf8, size_t size) {
    String *self = (String*)Class_Make_Obj(STRING);
    return Str_init_wrap_trusted_utf8(self, utf8, size);
}

String*
Str_init_stack_string(void *allocation, const char *utf8, size_t size) {
    String *self = (String*)Class_Init_Obj(STRING, allocation);
    return Str_init_wrap_trusted_utf8(self, utf8, size);
}

String*
Str_init_wrap_trusted_utf8(String *self, const char *ptr, size_t size) {
    self->ptr    = ptr;
    self->size   = size;
    self->origin = NULL;
    return self;
}

String*
Str_new_from_char(int32_t code_point) {
    const size_t MAX_UTF8_BYTES = 4;
    char   *ptr  = (char*)MALLOCATE(MAX_UTF8_BYTES + 1);
    size_t  size = StrHelp_encode_utf8_char(code_point, (uint8_t*)ptr);
    ptr[size] = '\0';

    String *self = (String*)Class_Make_Obj(STRING);
    self->ptr    = ptr;
    self->size   = size;
    self->origin = self;
    return self;
}

String*
Str_newf(const char *pattern, ...) {
    CharBuf *buf = CB_new(strlen(pattern));
    va_list args;
    va_start(args, pattern);
    CB_VCatF(buf, pattern, args);
    va_end(args);
    String *self = CB_Yield_String(buf);
    DECREF(buf);
    return self;
}

static String*
S_new_substring(String *string, size_t byte_offset, size_t size) {
    String *self = (String*)Class_Make_Obj(STRING);

    if (string->origin == NULL) {
        // Copy substring of wrapped strings.
        Str_init_from_trusted_utf8(self, string->ptr + byte_offset, size);
    }
    else {
        self->ptr    = string->ptr + byte_offset;
        self->size   = size;
        self->origin = (String*)INCREF(string->origin);
    }

    return self;
}

bool
Str_Is_Copy_On_IncRef_IMP(String *self) {
    return self->origin == NULL;
}

void
Str_Destroy_IMP(String *self) {
    if (self->origin == self) {
        FREEMEM((char*)self->ptr);
    }
    else {
        DECREF(self->origin);
    }
    SUPER_DESTROY(self, STRING);
}

size_t
Str_Hash_Sum_IMP(String *self) {
    size_t hashvalue = 5381;
    StringIterator *iter = STACK_ITER(self, 0);

    const StrIter_Next_t next = METHOD_PTR(STRINGITERATOR, CFISH_StrIter_Next);
    int32_t code_point;
    while (STR_OOB != (code_point = next(iter))) {
        hashvalue = ((hashvalue << 5) + hashvalue) ^ code_point;
    }

    return hashvalue;
}

static void
S_die_invalid_utf8(const char *text, size_t size, const char *file, int line,
                   const char *func) {
    fprintf(stderr, "Invalid UTF-8, aborting: '");
    fwrite(text, sizeof(char), size < 200 ? size : 200, stderr);
    if (size > 200) { fwrite("[...]", sizeof(char), 5, stderr); }
    fprintf(stderr, "' (length %lu)\n", (unsigned long)size);
    Err_throw_at(ERR, file, line, func, "Invalid UTF-8");
}

String*
Str_To_String_IMP(String *self) {
    return (String*)INCREF(self);
}

int64_t
Str_To_I64_IMP(String *self) {
    return Str_BaseX_To_I64(self, 10);
}

int64_t
Str_BaseX_To_I64_IMP(String *self, uint32_t base) {
    StringIterator *iter = STACK_ITER(self, 0);
    int64_t retval = 0;
    bool is_negative = false;
    int32_t code_point = StrIter_Next(iter);

    // Advance past minus sign.
    if (code_point == '-') {
        code_point = StrIter_Next(iter);
        is_negative = true;
    }

    // Accumulate.
    while (code_point != STR_OOB) {
        if (isalnum(code_point)) {
            int32_t addend = isdigit(code_point)
                             ? code_point - '0'
                             : tolower(code_point) - 'a' + 10;
            if (addend >= (int32_t)base) { break; }
            retval *= base;
            retval += addend;
        }
        else {
            break;
        }
        code_point = StrIter_Next(iter);
    }

    // Apply minus sign.
    if (is_negative) { retval = 0 - retval; }

    return retval;
}

double
Str_To_F64_IMP(String *self) {
    size_t amount = self->size < 511 ? self->size : 511;
    char buf[512];
    memcpy(buf, self->ptr, amount);
    buf[amount] = 0; // NULL-terminate.
    return strtod(buf, NULL);
}

char*
Str_To_Utf8_IMP(String *self) {
    char *buf = (char*)malloc(self->size + 1);
    memcpy(buf, self->ptr, self->size);
    buf[self->size] = '\0'; // NULL-terminate.
    return buf;
}

ByteBuf*
Str_To_ByteBuf_IMP(String *self) {
    return BB_new_bytes(self->ptr, self->size);
}

String*
Str_Clone_IMP(String *self) {
    return (String*)INCREF(self);
}

String*
Str_Cat_IMP(String *self, String *other) {
    return Str_Cat_Trusted_Utf8(self, other->ptr, other->size);
}

String*
Str_Cat_Utf8_IMP(String *self, const char* ptr, size_t size) {
    if (!StrHelp_utf8_valid(ptr, size)) {
        DIE_INVALID_UTF8(ptr, size);
    }
    return Str_Cat_Trusted_Utf8(self, ptr, size);
}

String*
Str_Cat_Trusted_Utf8_IMP(String *self, const char* ptr, size_t size) {
    size_t  result_size = self->size + size;
    char   *result_ptr  = (char*)MALLOCATE(result_size + 1);
    memcpy(result_ptr, self->ptr, self->size);
    memcpy(result_ptr + self->size, ptr, size);
    result_ptr[result_size] = '\0';
    String *result = (String*)Class_Make_Obj(STRING);
    return Str_init_steal_trusted_utf8(result, result_ptr, result_size);
}

bool
Str_Starts_With_IMP(String *self, String *prefix) {
    return Str_Starts_With_Utf8_IMP(self, prefix->ptr, prefix->size);
}

bool
Str_Starts_With_Utf8_IMP(String *self, const char *prefix, size_t size) {
    if (size <= self->size
        && (memcmp(self->ptr, prefix, size) == 0)
       ) {
        return true;
    }
    else {
        return false;
    }
}

bool
Str_Equals_IMP(String *self, Obj *other) {
    String *const twin = (String*)other;
    if (twin == self)              { return true; }
    if (!Obj_is_a(other, STRING)) { return false; }
    return Str_Equals_Utf8_IMP(self, twin->ptr, twin->size);
}

int32_t
Str_Compare_To_IMP(String *self, Obj *other) {
    String  *twin = (String*)CERTIFY(other, STRING);
    size_t   min_size;
    int32_t  tie;

    if (self->size <= twin->size) {
        min_size = self->size;
        tie      = self->size < twin->size ? -1 : 0;
    }
    else {
        min_size = twin->size;
        tie      = 1;
    }

    int comparison = memcmp(self->ptr, twin->ptr, min_size);
    if (comparison < 0) { return -1; }
    if (comparison > 0) { return 1; }

    return tie;
}

bool
Str_Equals_Utf8_IMP(String *self, const char *ptr, size_t size) {
    if (self->size != size) {
        return false;
    }
    return (memcmp(self->ptr, ptr, self->size) == 0);
}

bool
Str_Ends_With_IMP(String *self, String *suffix) {
    return Str_Ends_With_Utf8_IMP(self, suffix->ptr, suffix->size);
}

bool
Str_Ends_With_Utf8_IMP(String *self, const char *suffix, size_t suffix_len) {
    if (suffix_len <= self->size) {
        const char *start = self->ptr + self->size - suffix_len;
        if (memcmp(start, suffix, suffix_len) == 0) {
            return true;
        }
    }

    return false;
}

bool
Str_Contains_IMP(String *self, String *substring) {
    return !!S_memmem(self, substring->ptr, substring->size);
}

bool
Str_Contains_Utf8_IMP(String *self, const char *substring, size_t size) {
    return !!S_memmem(self, substring, size);
}

StringIterator*
Str_Find_IMP(String *self, String *substring) {
    return Str_Find_Utf8(self, substring->ptr, substring->size);
}

StringIterator*
Str_Find_Utf8_IMP(String *self, const char *substring, size_t size) {
    const char *ptr = S_memmem(self, substring, size);
    return ptr ? StrIter_new(self, ptr - self->ptr) : NULL;
}

static const char*
S_memmem(String *self, const char *substring, size_t size) {
    if (size == 0)         { return self->ptr; }
    if (size > self->size) { return NULL;      }

    const char *ptr = self->ptr;
    const char *end = ptr + self->size - size + 1;
    char first_char = substring[0];

    // Naive string search.
    while (NULL != (ptr = (const char*)memchr(ptr, first_char, end - ptr))) {
        if (memcmp(ptr, substring, size) == 0) { break; }
        ptr++;
    }

    return ptr;
}

String*
Str_Trim_IMP(String *self) {
    StringIterator *top = STACK_ITER(self, 0);
    StrIter_Skip_Whitespace(top);

    StringIterator *tail = NULL;
    if (top->byte_offset < self->size) {
        tail = STACK_ITER(self, self->size);
        StrIter_Skip_Whitespace_Back(tail);
    }

    return StrIter_crop((StringIterator*)top, (StringIterator*)tail);
}

String*
Str_Trim_Top_IMP(String *self) {
    StringIterator *top = STACK_ITER(self, 0);
    StrIter_Skip_Whitespace(top);
    return StrIter_crop((StringIterator*)top, NULL);
}

String*
Str_Trim_Tail_IMP(String *self) {
    StringIterator *tail = STACK_ITER(self, self->size);
    StrIter_Skip_Whitespace_Back(tail);
    return StrIter_crop(NULL, (StringIterator*)tail);
}

size_t
Str_Length_IMP(String *self) {
    StringIterator *iter = STACK_ITER(self, 0);
    return StrIter_Advance(iter, SIZE_MAX);
}

int32_t
Str_Code_Point_At_IMP(String *self, size_t tick) {
    StringIterator *iter = STACK_ITER(self, 0);
    StrIter_Advance(iter, tick);
    return StrIter_Next(iter);
}

int32_t
Str_Code_Point_From_IMP(String *self, size_t tick) {
    if (tick == 0) { return STR_OOB; }
    StringIterator *iter = STACK_ITER(self, self->size);
    StrIter_Recede(iter, tick - 1);
    return StrIter_Prev(iter);
}

String*
Str_SubString_IMP(String *self, size_t offset, size_t len) {
    StringIterator *iter = STACK_ITER(self, 0);

    StrIter_Advance(iter, offset);
    size_t start_offset = iter->byte_offset;

    StrIter_Advance(iter, len);
    size_t size = iter->byte_offset - start_offset;

    return S_new_substring(self, start_offset, size);
}

size_t
Str_Get_Size_IMP(String *self) {
    return self->size;
}

const char*
Str_Get_Ptr8_IMP(String *self) {
    return self->ptr;
}

StringIterator*
Str_Top_IMP(String *self) {
    return StrIter_new(self, 0);
}

StringIterator*
Str_Tail_IMP(String *self) {
    return StrIter_new(self, self->size);
}

/*****************************************************************/

StringIterator*
StrIter_new(String *string, size_t byte_offset) {
    StringIterator *self = (StringIterator*)Class_Make_Obj(STRINGITERATOR);
    self->string      = (String*)INCREF(string);
    self->byte_offset = byte_offset;
    return self;
}

static StringIterator*
S_new_stack_iter(void *allocation, String *string, size_t byte_offset) {
    StringIterator *self
        = (StringIterator*)Class_Init_Obj(STRINGITERATOR, allocation);
    // Assume that the string will be available for the lifetime of the
    // iterator and don't increase its refcount.
    self->string      = string;
    self->byte_offset = byte_offset;
    return self;
}

String*
StrIter_crop(StringIterator *top, StringIterator *tail) {
    String *string;
    size_t  top_offset;
    size_t  tail_offset;

    if (tail == NULL) {
        if (top == NULL) {
            THROW(ERR, "StrIter_crop: Both top and tail are NULL");
            UNREACHABLE_RETURN(String*);
        }
        string      = top->string;
        tail_offset = string->size;
    }
    else {
        string = tail->string;
        if (top != NULL && string != top->string) {
            THROW(ERR, "StrIter_crop: strings don't match");
            UNREACHABLE_RETURN(String*);
        }

        tail_offset = tail->byte_offset;
    }

    if (top == NULL) {
        top_offset = 0;
    }
    else {
        top_offset = top->byte_offset;
        if (top_offset > tail_offset) {
            THROW(ERR, "StrIter_crop: top is behind tail");
            UNREACHABLE_RETURN(String*);
        }
    }

    return S_new_substring(string, top_offset, tail_offset - top_offset);
}

StringIterator*
StrIter_Clone_IMP(StringIterator *self) {
    return StrIter_new(self->string, self->byte_offset);
}

void
StrIter_Assign_IMP(StringIterator *self, StringIterator *other) {
    if (self->string != other->string) {
        DECREF(self->string);
        self->string = (String*)INCREF(other->string);
    }
    self->byte_offset = other->byte_offset;
}

bool
StrIter_Equals_IMP(StringIterator *self, Obj *other) {
    StringIterator *const twin = (StringIterator*)other;
    if (twin == self)                     { return true; }
    if (!Obj_is_a(other, STRINGITERATOR)) { return false; }
    return self->string == twin->string
           && self->byte_offset == twin->byte_offset;
}

int32_t
StrIter_Compare_To_IMP(StringIterator *self, Obj *other) {
    StringIterator *twin = (StringIterator*)CERTIFY(other, STRINGITERATOR);
    if (self->string != twin->string) {
        THROW(ERR, "Can't compare iterators of different strings");
        UNREACHABLE_RETURN(int32_t);
    }
    if (self->byte_offset < twin->byte_offset) { return -1; }
    if (self->byte_offset > twin->byte_offset) { return 1; }
    return 0;
}

bool
StrIter_Has_Next_IMP(StringIterator *self) {
    return self->byte_offset < self->string->size;
}

bool
StrIter_Has_Prev_IMP(StringIterator *self) {
    return self->byte_offset != 0;
}

int32_t
StrIter_Next_IMP(StringIterator *self) {
    String *string      = self->string;
    size_t  byte_offset = self->byte_offset;
    size_t  size        = string->size;

    if (byte_offset >= size) { return STR_OOB; }

    const uint8_t *const ptr = (const uint8_t*)string->ptr;
    int32_t retval = ptr[byte_offset++];

    if (retval >= 0x80) {
        /*
         * The 'mask' bit is tricky. In each iteration, 'retval' is
         * left-shifted by 6 and 'mask' by 5 bits. So relative to the first
         * byte of the sequence, 'mask' moves one bit to the right.
         *
         * The possible outcomes after the loop are:
         *
         * Two byte sequence
         * retval: 110aaaaa bbbbbb
         * mask:   00100000 000000
         *
         * Three byte sequence
         * retval: 1110aaaa bbbbbb cccccc
         * mask:   00010000 000000 000000
         *
         * Four byte sequence
         * retval: 11110aaa bbbbbb cccccc dddddd
         * mask:   00001000 000000 000000 000000
         *
         * This also illustrates why the exit condition (retval & mask)
         * works. After the first iteration, the third most significant bit
         * is tested. After the second iteration, the fourth, and so on.
         */

        int32_t mask = 1 << 6;

        do {
            if (byte_offset >= size) {
                THROW(ERR, "StrIter_Next: Invalid UTF-8");
                UNREACHABLE_RETURN(int32_t);
            }

            retval = (retval << 6) | (ptr[byte_offset++] & 0x3F);
            mask <<= 5;
        } while (retval & mask);

        retval &= mask - 1;
    }

    self->byte_offset = byte_offset;
    return retval;
}

int32_t
StrIter_Prev_IMP(StringIterator *self) {
    size_t byte_offset = self->byte_offset;

    if (byte_offset == 0) { return STR_OOB; }

    const uint8_t *const ptr = (const uint8_t*)self->string->ptr;
    int32_t retval = ptr[--byte_offset];

    if (retval >= 0x80) {
        // Construct the result from right to left.

        if (byte_offset == 0) {
            THROW(ERR, "StrIter_Prev: Invalid UTF-8");
            UNREACHABLE_RETURN(int32_t);
        }

        retval &= 0x3F;
        int shift = 6;
        int32_t first_byte_mask = 0x1F;
        int32_t byte = ptr[--byte_offset];

        while ((byte & 0xC0) == 0x80) {
            if (byte_offset == 0) {
                THROW(ERR, "StrIter_Prev: Invalid UTF-8");
                UNREACHABLE_RETURN(int32_t);
            }

            retval |= (byte & 0x3F) << shift;
            shift += 6;
            first_byte_mask >>= 1;
            byte = ptr[--byte_offset];
        }

        retval |= (byte & first_byte_mask) << shift;
    }

    self->byte_offset = byte_offset;
    return retval;
}

size_t
StrIter_Advance_IMP(StringIterator *self, size_t num) {
    size_t num_skipped = 0;
    size_t byte_offset = self->byte_offset;
    size_t size        = self->string->size;
    const uint8_t *const ptr = (const uint8_t*)self->string->ptr;

    while (num_skipped < num) {
        if (byte_offset >= size) {
            break;
        }
        uint8_t first_byte = ptr[byte_offset];
        byte_offset += StrHelp_UTF8_COUNT[first_byte];
        ++num_skipped;
    }

    if (byte_offset > size) {
        THROW(ERR, "StrIter_Advance: Invalid UTF-8");
        UNREACHABLE_RETURN(size_t);
    }

    self->byte_offset = byte_offset;
    return num_skipped;
}

size_t
StrIter_Recede_IMP(StringIterator *self, size_t num) {
    size_t num_skipped = 0;
    size_t byte_offset = self->byte_offset;
    const uint8_t *const ptr = (const uint8_t*)self->string->ptr;

    while (num_skipped < num) {
        if (byte_offset == 0) {
            break;
        }

        uint8_t byte;
        do {
            if (byte_offset == 0) {
                THROW(ERR, "StrIter_Recede: Invalid UTF-8");
                UNREACHABLE_RETURN(size_t);
            }

            byte = ptr[--byte_offset];
        } while ((byte & 0xC0) == 0x80);
        ++num_skipped;
    }

    self->byte_offset = byte_offset;
    return num_skipped;
}

size_t
StrIter_Skip_Whitespace_IMP(StringIterator *self) {
    size_t  num_skipped = 0;
    size_t  byte_offset = self->byte_offset;
    int32_t code_point;

    while (STR_OOB != (code_point = StrIter_Next(self))) {
        if (!StrHelp_is_whitespace(code_point)) { break; }
        byte_offset = self->byte_offset;
        ++num_skipped;
    }

    self->byte_offset = byte_offset;
    return num_skipped;
}

size_t
StrIter_Skip_Whitespace_Back_IMP(StringIterator *self) {
    size_t  num_skipped = 0;
    size_t  byte_offset = self->byte_offset;
    int32_t code_point;

    while (STR_OOB != (code_point = StrIter_Prev(self))) {
        if (!StrHelp_is_whitespace(code_point)) { break; }
        byte_offset = self->byte_offset;
        ++num_skipped;
    }

    self->byte_offset = byte_offset;
    return num_skipped;
}

bool
StrIter_Starts_With_IMP(StringIterator *self, String *prefix) {
    return StrIter_Starts_With_Utf8_IMP(self, prefix->ptr, prefix->size);
}

bool
StrIter_Starts_With_Utf8_IMP(StringIterator *self, const char *prefix,
                             size_t size) {
    String *string      = self->string;
    size_t  byte_offset = self->byte_offset;

    if (string->size - byte_offset < size) { return false; }

    return memcmp(string->ptr + byte_offset, prefix, size) == 0;
}

bool
StrIter_Ends_With_IMP(StringIterator *self, String *suffix) {
    return StrIter_Ends_With_Utf8_IMP(self, suffix->ptr, suffix->size);
}

bool
StrIter_Ends_With_Utf8_IMP(StringIterator *self, const char *suffix,
                           size_t size) {
    String *string      = self->string;
    size_t  byte_offset = self->byte_offset;

    if (byte_offset < size) { return false; }

    return memcmp(string->ptr + byte_offset - size, suffix, size) == 0;
}

void
StrIter_Destroy_IMP(StringIterator *self) {
    DECREF(self->string);
    SUPER_DESTROY(self, STRINGITERATOR);
}



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

#define C_TESTLUCY_TESTUTILS
#define TESTLUCY_USE_SHORT_NAMES
#include "Lucy/Util/ToolSet.h"
#include <string.h>

#include "Lucy/Test/TestUtils.h"
#include "Lucy/Test.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Clownfish/TestHarness/TestUtils.h"
#include "Lucy/Analysis/Analyzer.h"
#include "Lucy/Analysis/Inversion.h"
#include "Lucy/Analysis/Token.h"
#include "Lucy/Search/TermQuery.h"
#include "Lucy/Search/PhraseQuery.h"
#include "Lucy/Search/LeafQuery.h"
#include "Lucy/Search/ANDQuery.h"
#include "Lucy/Search/NOTQuery.h"
#include "Lucy/Search/ORQuery.h"
#include "Lucy/Search/RangeQuery.h"
#include "Lucy/Store/FSFolder.h"
#include "Lucy/Store/InStream.h"
#include "Lucy/Store/OutStream.h"
#include "Lucy/Store/RAMFile.h"
#include "Lucy/Util/Freezer.h"

Vector*
TestUtils_doc_set() {
    Vector *docs = Vec_new(10);

    Vec_Push(docs, (Obj*)TestUtils_get_str("x"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("y"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("z"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("x a"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("x a b"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("x a b c"));
    Vec_Push(docs, (Obj*)TestUtils_get_str("x foo a b c d"));

    return docs;
}

PolyQuery*
TestUtils_make_poly_query(uint32_t boolop, ...) {
    va_list args;
    Query *child;
    PolyQuery *retval;
    Vector *children = Vec_new(0);

    va_start(args, boolop);
    while (NULL != (child = va_arg(args, Query*))) {
        Vec_Push(children, (Obj*)child);
    }
    va_end(args);

    retval = boolop == BOOLOP_OR
             ? (PolyQuery*)ORQuery_new(children)
             : (PolyQuery*)ANDQuery_new(children);
    DECREF(children);
    return retval;
}

TermQuery*
TestUtils_make_term_query(const char *field, const char *term) {
    String *field_str = SSTR_WRAP_C(field);
    String *term_str  = SSTR_WRAP_C(term);
    return TermQuery_new((String*)field_str, (Obj*)term_str);
}

PhraseQuery*
TestUtils_make_phrase_query(const char *field, ...) {
    String *field_str = SSTR_WRAP_C(field);
    va_list args;
    Vector *terms = Vec_new(0);
    PhraseQuery *query;
    char *term_str;

    va_start(args, field);
    while (NULL != (term_str = va_arg(args, char*))) {
        Vec_Push(terms, (Obj*)TestUtils_get_str(term_str));
    }
    va_end(args);

    query = PhraseQuery_new(field_str, terms);
    DECREF(terms);
    return query;
}

LeafQuery*
TestUtils_make_leaf_query(const char *field, const char *term) {
    String *term_str  = SSTR_WRAP_C(term);
    String *field_str = field ? SSTR_WRAP_C(field) : NULL;
    return LeafQuery_new(field_str, term_str);
}

NOTQuery*
TestUtils_make_not_query(Query* negated_query) {
    NOTQuery *not_query = NOTQuery_new(negated_query);
    DECREF(negated_query);
    return not_query;
}

RangeQuery*
TestUtils_make_range_query(const char *field, const char *lower_term,
                           const char *upper_term, bool include_lower,
                           bool include_upper) {
    String *f     = SSTR_WRAP_C(field);
    String *lterm = SSTR_WRAP_C(lower_term);
    String *uterm = SSTR_WRAP_C(upper_term);
    return RangeQuery_new(f, (Obj*)lterm, (Obj*)uterm, include_lower,
                          include_upper);
}

void
TestUtils_test_analyzer(TestBatchRunner *runner, Analyzer *analyzer,
                        String *source, Vector *expected,
                        const char *message) {
    Token *seed = Token_new(Str_Get_Ptr8(source), Str_Get_Size(source),
                            0, 0, 1.0f, 1);
    Inversion *starter = Inversion_new(seed);
    Inversion *transformed = Analyzer_Transform(analyzer, starter);
    Vector *got = Vec_new(1);
    Token *token;
    while (NULL != (token = Inversion_Next(transformed))) {
        String *token_text
            = Str_new_from_utf8(Token_Get_Text(token), Token_Get_Len(token));
        Vec_Push(got, (Obj*)token_text);
    }
    TEST_TRUE(runner, Vec_Equals(expected, (Obj*)got),
              "Transform(): %s", message);
    DECREF(transformed);

    transformed = Analyzer_Transform_Text(analyzer, source);
    Vec_Clear(got);
    while (NULL != (token = Inversion_Next(transformed))) {
        String *token_text
            = Str_new_from_utf8(Token_Get_Text(token), Token_Get_Len(token));
        Vec_Push(got, (Obj*)token_text);
    }
    TEST_TRUE(runner, Vec_Equals(expected, (Obj*)got),
              "Transform_Text(): %s", message);
    DECREF(transformed);

    DECREF(got);
    got = Analyzer_Split(analyzer, source);
    TEST_TRUE(runner, Vec_Equals(expected, (Obj*)got), "Split(): %s", message);

    DECREF(got);
    DECREF(starter);
    DECREF(seed);
}

FSFolder*
TestUtils_modules_folder() {
    static const char *const paths[] = { "modules", "../modules" };

    for (size_t i = 0; i < sizeof(paths) / sizeof(char*); i++) {
        String *path = Str_newf(paths[i]);
        FSFolder *modules_folder = FSFolder_new(path);
        DECREF(path);
        if (FSFolder_Check(modules_folder)) {
            return modules_folder;
        }
        DECREF(modules_folder);
    }

    return NULL;
}


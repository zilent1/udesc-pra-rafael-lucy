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

#define C_LUCY_ANDQUERY
#define C_LUCY_ANDCOMPILER
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Search/ANDQuery.h"

#include "Clownfish/CharBuf.h"
#include "Lucy/Index/DocVector.h"
#include "Lucy/Index/SegReader.h"
#include "Lucy/Index/Similarity.h"
#include "Lucy/Plan/Schema.h"
#include "Lucy/Search/ANDMatcher.h"
#include "Lucy/Search/Searcher.h"
#include "Lucy/Search/Span.h"
#include "Lucy/Store/InStream.h"
#include "Lucy/Store/OutStream.h"
#include "Lucy/Util/Freezer.h"

ANDQuery*
ANDQuery_new(Vector *children) {
    ANDQuery *self = (ANDQuery*)Class_Make_Obj(ANDQUERY);
    return ANDQuery_init(self, children);
}

ANDQuery*
ANDQuery_init(ANDQuery *self, Vector *children) {
    return (ANDQuery*)PolyQuery_init((PolyQuery*)self, children);
}

String*
ANDQuery_To_String_IMP(ANDQuery *self) {
    ANDQueryIVARS *const ivars = ANDQuery_IVARS(self);
    uint32_t num_kids = Vec_Get_Size(ivars->children);
    if (!num_kids) { return Str_new_from_trusted_utf8("()", 2); }
    else {
        CharBuf *buf = CB_new(0);
        CB_Cat_Trusted_Utf8(buf, "(", 1);
        for (uint32_t i = 0; i < num_kids; i++) {
            String *kid_string = Obj_To_String(Vec_Fetch(ivars->children, i));
            CB_Cat(buf, kid_string);
            DECREF(kid_string);
            if (i == num_kids - 1) {
                CB_Cat_Trusted_Utf8(buf, ")", 1);
            }
            else {
                CB_Cat_Trusted_Utf8(buf, " AND ", 5);
            }
        }
        String *retval = CB_Yield_String(buf);
        DECREF(buf);
        return retval;
    }
}


bool
ANDQuery_Equals_IMP(ANDQuery *self, Obj *other) {
    if ((ANDQuery*)other == self)   { return true; }
    if (!Obj_is_a(other, ANDQUERY)) { return false; }
    ANDQuery_Equals_t super_equals
        = (ANDQuery_Equals_t)SUPER_METHOD_PTR(ANDQUERY, LUCY_ANDQuery_Equals);
    return super_equals(self, other);
}

Compiler*
ANDQuery_Make_Compiler_IMP(ANDQuery *self, Searcher *searcher, float boost,
                           bool subordinate) {
    ANDCompiler *compiler = ANDCompiler_new(self, searcher, boost);
    if (!subordinate) {
        ANDCompiler_Normalize(compiler);
    }
    return (Compiler*)compiler;
}

/**********************************************************************/

ANDCompiler*
ANDCompiler_new(ANDQuery *parent, Searcher *searcher, float boost) {
    ANDCompiler *self = (ANDCompiler*)Class_Make_Obj(ANDCOMPILER);
    return ANDCompiler_init(self, parent, searcher, boost);
}

ANDCompiler*
ANDCompiler_init(ANDCompiler *self, ANDQuery *parent, Searcher *searcher,
                 float boost) {
    PolyCompiler_init((PolyCompiler*)self, (PolyQuery*)parent, searcher,
                      boost);
    return self;
}

Matcher*
ANDCompiler_Make_Matcher_IMP(ANDCompiler *self, SegReader *reader,
                             bool need_score) {
    ANDCompilerIVARS *const ivars = ANDCompiler_IVARS(self);
    uint32_t num_kids = Vec_Get_Size(ivars->children);

    if (num_kids == 1) {
        Compiler *only_child = (Compiler*)Vec_Fetch(ivars->children, 0);
        return Compiler_Make_Matcher(only_child, reader, need_score);
    }
    else {
        Vector *child_matchers = Vec_new(num_kids);

        // Add child matchers one by one.
        for (uint32_t i = 0; i < num_kids; i++) {
            Compiler *child = (Compiler*)Vec_Fetch(ivars->children, i);
            Matcher *child_matcher
                = Compiler_Make_Matcher(child, reader, need_score);

            // If any required clause fails, the whole thing fails.
            if (child_matcher == NULL) {
                DECREF(child_matchers);
                return NULL;
            }
            else {
                Vec_Push(child_matchers, (Obj*)child_matcher);
            }
        }

        Matcher *retval
            = (Matcher*)ANDMatcher_new(child_matchers,
                                       ANDCompiler_Get_Similarity(self));
        DECREF(child_matchers);
        return retval;

    }
}



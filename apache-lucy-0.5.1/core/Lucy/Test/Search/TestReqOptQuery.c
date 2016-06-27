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

#define C_TESTLUCY_TESTREQOPTQUERY
#define TESTLUCY_USE_SHORT_NAMES
#include "Lucy/Util/ToolSet.h"
#include <math.h>

#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Lucy/Test.h"
#include "Lucy/Test/TestUtils.h"
#include "Lucy/Test/Search/TestReqOptQuery.h"
#include "Lucy/Search/RequiredOptionalQuery.h"
#include "Lucy/Search/LeafQuery.h"
#include "Lucy/Util/Freezer.h"

TestReqOptQuery*
TestReqOptQuery_new() {
    return (TestReqOptQuery*)Class_Make_Obj(TESTREQOPTQUERY);
}

static void
test_Dump_Load_and_Equals(TestBatchRunner *runner) {
    Query *a_leaf  = (Query*)TestUtils_make_leaf_query(NULL, "a");
    Query *b_leaf  = (Query*)TestUtils_make_leaf_query(NULL, "b");
    Query *c_leaf  = (Query*)TestUtils_make_leaf_query(NULL, "c");
    RequiredOptionalQuery *query = ReqOptQuery_new(a_leaf, b_leaf);
    RequiredOptionalQuery *kids_differ = ReqOptQuery_new(a_leaf, c_leaf);
    RequiredOptionalQuery *boost_differs = ReqOptQuery_new(a_leaf, b_leaf);
    Obj *dump = (Obj*)ReqOptQuery_Dump(query);
    RequiredOptionalQuery *clone
        = (RequiredOptionalQuery*)Freezer_load(dump);

    TEST_FALSE(runner, ReqOptQuery_Equals(query, (Obj*)kids_differ),
               "Different kids spoil Equals");
    TEST_TRUE(runner, ReqOptQuery_Equals(query, (Obj*)boost_differs),
              "Equals with identical boosts");
    ReqOptQuery_Set_Boost(boost_differs, 1.5);
    TEST_FALSE(runner, ReqOptQuery_Equals(query, (Obj*)boost_differs),
               "Different boost spoils Equals");
    TEST_TRUE(runner, ReqOptQuery_Equals(query, (Obj*)clone),
              "Dump => Load round trip");

    DECREF(a_leaf);
    DECREF(b_leaf);
    DECREF(c_leaf);
    DECREF(query);
    DECREF(kids_differ);
    DECREF(boost_differs);
    DECREF(dump);
    DECREF(clone);
}

void
TestReqOptQuery_Run_IMP(TestReqOptQuery *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 4);
    test_Dump_Load_and_Equals(runner);
}



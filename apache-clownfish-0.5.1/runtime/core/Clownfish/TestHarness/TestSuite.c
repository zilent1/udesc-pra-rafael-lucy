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

#define C_CFISH_TESTSUITE
#define CFISH_USE_SHORT_NAMES
#define TESTCFISH_USE_SHORT_NAMES

#include "Clownfish/TestHarness/TestSuite.h"

#include "Clownfish/String.h"
#include "Clownfish/Err.h"
#include "Clownfish/TestHarness/TestBatch.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Clownfish/TestHarness/TestFormatter.h"
#include "Clownfish/TestHarness/TestSuiteRunner.h"
#include "Clownfish/Vector.h"
#include "Clownfish/Class.h"

static void
S_unbuffer_stdout();

TestSuite*
TestSuite_new() {
    TestSuite *self = (TestSuite*)Class_Make_Obj(TESTSUITE);
    return TestSuite_init(self);
}

TestSuite*
TestSuite_init(TestSuite *self) {
    self->batches = Vec_new(0);
    return self;
}

void
TestSuite_Destroy_IMP(TestSuite *self) {
    DECREF(self->batches);
    SUPER_DESTROY(self, TESTSUITE);
}

void
TestSuite_Add_Batch_IMP(TestSuite *self, TestBatch *batch) {
    Vec_Push(self->batches, (Obj*)batch);
}

bool
TestSuite_Run_Batch_IMP(TestSuite *self, String *class_name,
                    TestFormatter *formatter) {
    S_unbuffer_stdout();

    size_t size = Vec_Get_Size(self->batches);

    for (size_t i = 0; i < size; ++i) {
        TestBatch *batch = (TestBatch*)Vec_Fetch(self->batches, i);

        if (Str_Equals(TestBatch_get_class_name(batch), (Obj*)class_name)) {
            TestBatchRunner *runner = TestBatchRunner_new(formatter);
            bool result = TestBatchRunner_Run_Batch(runner, batch);
            DECREF(runner);
            return result;
        }
    }

    THROW(ERR, "Couldn't find test class '%o'", class_name);
    UNREACHABLE_RETURN(bool);
}

bool
TestSuite_Run_All_Batches_IMP(TestSuite *self, TestFormatter *formatter) {
    S_unbuffer_stdout();

    TestSuiteRunner *runner = TestSuiteRunner_new(formatter);
    size_t size = Vec_Get_Size(self->batches);

    for (size_t i = 0; i < size; ++i) {
        TestBatch *batch = (TestBatch*)Vec_Fetch(self->batches, i);
        TestSuiteRunner_Run_Batch(runner, batch);
    }

    bool result = TestSuiteRunner_Finish(runner);

    DECREF(runner);
    return result;
}

static void
S_unbuffer_stdout() {
    int check_val = setvbuf(stdout, NULL, _IONBF, 0);
    if (check_val != 0) {
        fprintf(stderr, "Failed when trying to unbuffer stdout\n");
    }
}



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

#define C_CFISH_TESTFORMATTER
#define CFISH_USE_SHORT_NAMES

#include "Clownfish/TestHarness/TestFormatter.h"

#include "Clownfish/String.h"
#include "Clownfish/Err.h"
#include "Clownfish/TestHarness/TestBatch.h"
#include "Clownfish/TestHarness/TestSuiteRunner.h"
#include "Clownfish/Util/Memory.h"
#include "Clownfish/Class.h"

TestFormatter*
TestFormatter_init(TestFormatter *self) {
    ABSTRACT_CLASS_CHECK(self, TESTFORMATTER);
    return self;
}

void
TestFormatter_test_result(TestFormatter *self, bool pass, uint32_t test_num,
                          const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TestFormatter_VTest_Result(self, pass, test_num, fmt, args);
    va_end(args);
}

void
TestFormatter_test_comment(TestFormatter *self, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TestFormatter_VTest_Comment(self, fmt, args);
    va_end(args);
}

void
TestFormatter_batch_comment(TestFormatter *self, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TestFormatter_VBatch_Comment(self, fmt, args);
    va_end(args);
}

TestFormatterCF*
TestFormatterCF_new() {
    TestFormatterCF *self
        = (TestFormatterCF*)Class_Make_Obj(TESTFORMATTERCF);
    return TestFormatterCF_init(self);
}

TestFormatterCF*
TestFormatterCF_init(TestFormatterCF *self) {
    return (TestFormatterCF*)TestFormatter_init((TestFormatter*)self);
}

void
TestFormatterCF_Batch_Prologue_IMP(TestFormatterCF *self, TestBatch *batch,
                                   uint32_t num_planned) {
    UNUSED_VAR(self);
    UNUSED_VAR(num_planned);
    String *class_name = TestBatch_get_class_name(batch);
    char *utf8 = Str_To_Utf8(class_name);
    printf("Running %s...\n", utf8);
    FREEMEM(utf8);
}

void
TestFormatterCF_VTest_Result_IMP(TestFormatterCF *self, bool pass,
                                 uint32_t test_num, const char *fmt,
                                 va_list args) {
    UNUSED_VAR(self);
    if (!pass) {
        printf("  Failed test %u: ", test_num);
        vprintf(fmt, args);
        printf("\n");
    }
}

void
TestFormatterCF_VTest_Skip_IMP(TestFormatterCF *self, uint32_t test_num,
                               uint32_t num_skipped, const char *fmt,
                               va_list args) {
    UNUSED_VAR(self);
    UNUSED_VAR(test_num);
    UNUSED_VAR(num_skipped);
    UNUSED_VAR(fmt);
    UNUSED_VAR(args);
}

void
TestFormatterCF_VTest_Comment_IMP(TestFormatterCF *self, const char *fmt,
                                  va_list args) {
    UNUSED_VAR(self);
    printf("    ");
    vprintf(fmt, args);
}

void
TestFormatterCF_VBatch_Comment_IMP(TestFormatterCF *self, const char *fmt,
                                   va_list args) {
    UNUSED_VAR(self);
    printf("  ");
    vprintf(fmt, args);
}

void
TestFormatterCF_Summary_IMP(TestFormatterCF *self, TestSuiteRunner *runner) {
    UNUSED_VAR(self);
    uint32_t num_batches = TestSuiteRunner_Get_Num_Batches(runner);
    uint32_t num_batches_failed
        = TestSuiteRunner_Get_Num_Batches_Failed(runner);
    uint32_t num_tests = TestSuiteRunner_Get_Num_Tests(runner);
    uint32_t num_tests_failed = TestSuiteRunner_Get_Num_Tests_Failed(runner);

    if (num_batches == 0) {
        printf("No tests planned or run.\n");
    }
    else if (num_batches_failed == 0) {
        printf("%u batches passed. %u tests passed.\n", num_batches,
               num_tests);
        printf("Result: PASS\n");
    }
    else {
        printf("%u/%u batches failed. %u/%u tests failed.\n",
               num_batches_failed, num_batches, num_tests_failed, num_tests);
        printf("Result: FAIL\n");
    }
}

TestFormatterTAP*
TestFormatterTAP_new() {
    TestFormatterTAP *self
        = (TestFormatterTAP*)Class_Make_Obj(TESTFORMATTERTAP);
    return TestFormatterTAP_init(self);
}

TestFormatterTAP*
TestFormatterTAP_init(TestFormatterTAP *self) {
    return (TestFormatterTAP*)TestFormatter_init((TestFormatter*)self);
}

void
TestFormatterTAP_Batch_Prologue_IMP(TestFormatterTAP *self, TestBatch *batch,
                                uint32_t num_planned) {
    UNUSED_VAR(self);
    UNUSED_VAR(batch);
    printf("1..%u\n", num_planned);
}

void
TestFormatterTAP_VTest_Result_IMP(TestFormatterTAP *self, bool pass,
                                  uint32_t test_num, const char *fmt,
                                  va_list args) {
    UNUSED_VAR(self);
    const char *result = pass ? "ok" : "not ok";
    printf("%s %u - ", result, test_num);
    vprintf(fmt, args);
    printf("\n");
}

void
TestFormatterTAP_VTest_Skip_IMP(TestFormatterTAP *self, uint32_t test_num,
                                uint32_t num_skipped, const char *fmt,
                                va_list args) {
    UNUSED_VAR(self);
    for (uint32_t i = 0; i < num_skipped; ++i) {
        printf("ok %u # SKIP ", test_num + i);
        vprintf(fmt, args);
        printf("\n");
    }
}

void
TestFormatterTAP_VTest_Comment_IMP(TestFormatterTAP *self, const char *fmt,
                                   va_list args) {
    UNUSED_VAR(self);
    printf("#   ");
    vprintf(fmt, args);
}

void
TestFormatterTAP_VBatch_Comment_IMP(TestFormatterTAP *self, const char *fmt,
                                    va_list args) {
    UNUSED_VAR(self);
    printf("# ");
    vprintf(fmt, args);
}

void
TestFormatterTAP_Summary_IMP(TestFormatterTAP *self, TestSuiteRunner *runner) {
    UNUSED_VAR(self);
    UNUSED_VAR(runner);
}



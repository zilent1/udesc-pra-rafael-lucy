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
#define TESTLUCY_USE_SHORT_NAMES
#include "Lucy/Util/ToolSet.h"

#include "Clownfish/Blob.h"
#include "Clownfish/Boolean.h"
#include "Clownfish/Num.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Clownfish/TestHarness/TestUtils.h"
#include "Lucy/Test/Util/TestFreezer.h"
#include "Lucy/Store/InStream.h"
#include "Lucy/Store/OutStream.h"
#include "Lucy/Store/RAMFile.h"
#include "Lucy/Util/Freezer.h"

TestFreezer*
TestFreezer_new() {
    return (TestFreezer*)Class_Make_Obj(TESTFREEZER);
}

// Return the result of round-tripping the object through FREEZE and THAW.
static Obj*
S_freeze_thaw(Obj *object) {
    if (object) {
        RAMFile *ram_file = RAMFile_new(NULL, false);
        OutStream *outstream = OutStream_open((Obj*)ram_file);
        FREEZE(object, outstream);
        OutStream_Close(outstream);
        DECREF(outstream);

        InStream *instream = InStream_open((Obj*)ram_file);
        Obj *retval = THAW(instream);
        DECREF(instream);
        DECREF(ram_file);
        return retval;
    }
    else {
        return NULL;
    }
}

// Return the result of round-tripping the object through dump() and load().
static Obj*
S_dump_load(Obj *object) {
    if (object) {
        Obj *dump = Freezer_dump(object);
        Obj *loaded = Freezer_load(dump);
        DECREF(dump);
        return loaded;
    }
    else {
        return NULL;
    }
}

static void
test_blob(TestBatchRunner *runner) {
    Blob *wanted = Blob_new("foobar", 6);
    Blob *got    = (Blob*)S_freeze_thaw((Obj*)wanted);
    TEST_TRUE(runner, got && Blob_Equals(wanted, (Obj*)got),
              "Serialization round trip");
    DECREF(wanted);
    DECREF(got);
}

static void
test_string(TestBatchRunner *runner) {
    String *wanted = TestUtils_get_str("foo");
    String *got    = (String*)S_freeze_thaw((Obj*)wanted);
    TEST_TRUE(runner, got && Str_Equals(wanted, (Obj*)got),
              "Round trip through FREEZE/THAW");
    DECREF(got);
    DECREF(wanted);
}

static void
test_hash(TestBatchRunner *runner) {
    Hash  *wanted = Hash_new(0);

    for (uint32_t i = 0; i < 10; i++) {
        String *str = TestUtils_random_string(rand() % 1200);
        Integer *num = Int_new(i);
        Hash_Store(wanted, str, (Obj*)num);
        DECREF(str);
    }

    {
        Hash *got = (Hash*)S_freeze_thaw((Obj*)wanted);
        TEST_TRUE(runner, got && Hash_Equals(wanted, (Obj*)got),
                  "Round trip through serialization.");
        DECREF(got);
    }

    {
        Obj *got = S_dump_load((Obj*)wanted);
        TEST_TRUE(runner, Hash_Equals(wanted, got),
                  "Dump => Load round trip");
        DECREF(got);
    }

    DECREF(wanted);
}

static void
test_num(TestBatchRunner *runner) {
    Float   *f64 = Float_new(1.33);
    Integer *i64 = Int_new(-1);
    Float   *f64_thaw = (Float*)S_freeze_thaw((Obj*)f64);
    Integer *i64_thaw = (Integer*)S_freeze_thaw((Obj*)i64);

    TEST_TRUE(runner, Float_Equals(f64, (Obj*)f64_thaw),
              "Float freeze/thaw");
    TEST_TRUE(runner, Int_Equals(i64, (Obj*)i64_thaw),
              "Integer freeze/thaw");

#ifdef LUCY_VALGRIND
    SKIP(runner, 1, "known leaks");
#else
    Boolean *true_thaw = (Boolean*)S_freeze_thaw((Obj*)CFISH_TRUE);
    TEST_TRUE(runner, Bool_Equals(CFISH_TRUE, (Obj*)true_thaw),
              "Boolean freeze/thaw");
#endif

    DECREF(i64_thaw);
    DECREF(f64_thaw);
    DECREF(i64);
    DECREF(f64);
}

static void
test_varray(TestBatchRunner *runner) {
    Vector *array = Vec_new(0);
    Vec_Store(array, 1, (Obj*)Str_newf("foo"));
    Vec_Store(array, 3, (Obj*)Str_newf("bar"));

    {
        Obj *got = S_freeze_thaw((Obj*)array);
        TEST_TRUE(runner, got && Vec_Equals(array, got),
                  "Round trip through FREEZE/THAW");
        DECREF(got);
    }

    {
        Obj *got = S_dump_load((Obj*)array);
        TEST_TRUE(runner, got && Vec_Equals(array, got),
                  "Dump => Load round trip");
        DECREF(got);
    }

    DECREF(array);
}

void
TestFreezer_Run_IMP(TestFreezer *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 9);
    test_blob(runner);
    test_string(runner);
    test_hash(runner);
    test_num(runner);
    test_varray(runner);
}


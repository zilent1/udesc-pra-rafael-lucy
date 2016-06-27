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

#define CFISH_USE_SHORT_NAMES
#define TESTCFISH_USE_SHORT_NAMES

#include "Clownfish/Test/TestByteBuf.h"

#include "Clownfish/ByteBuf.h"
#include "Clownfish/Test.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Clownfish/TestHarness/TestUtils.h"
#include "Clownfish/Blob.h"
#include "Clownfish/Class.h"
#include "Clownfish/String.h"

TestByteBuf*
TestBB_new() {
    return (TestByteBuf*)Class_Make_Obj(TESTBYTEBUF);
}

static void
test_Equals(TestBatchRunner *runner) {
    ByteBuf *bb = BB_new_bytes("foo", 4); // Include terminating NULL.

    {
        ByteBuf *other = BB_new_bytes("foo", 4);
        TEST_TRUE(runner, BB_Equals(bb, (Obj*)other), "Equals");
        DECREF(other);
    }

    TEST_TRUE(runner, BB_Equals_Bytes(bb, "foo", 4), "Equals_Bytes");
    TEST_FALSE(runner, BB_Equals_Bytes(bb, "foo", 3),
               "Equals_Bytes spoiled by different size");
    TEST_FALSE(runner, BB_Equals_Bytes(bb, "bar", 4),
               "Equals_Bytes spoiled by different content");

    {
        ByteBuf *other = BB_new_bytes("foo", 3);
        TEST_FALSE(runner, BB_Equals(bb, (Obj*)other),
                   "Different size spoils Equals");
        DECREF(other);
    }

    {
        ByteBuf *other = BB_new_bytes("bar", 4);
        TEST_INT_EQ(runner, BB_Get_Size(bb), BB_Get_Size(other),
                    "same length");
        TEST_FALSE(runner, BB_Equals(bb, (Obj*)other),
                   "Different content spoils Equals");
        DECREF(other);
    }

    DECREF(bb);
}

static void
test_Grow(TestBatchRunner *runner) {
    ByteBuf *bb = BB_new(1);
    TEST_INT_EQ(runner, BB_Get_Capacity(bb), 8,
                "Allocate in 8-byte increments");
    BB_Grow(bb, 9);
    TEST_INT_EQ(runner, BB_Get_Capacity(bb), 16,
                "Grow in 8-byte increments");
    DECREF(bb);
}

static void
test_Clone(TestBatchRunner *runner) {
    ByteBuf *bb = BB_new_bytes("foo", 3);
    ByteBuf *twin = BB_Clone(bb);
    TEST_TRUE(runner, BB_Equals(bb, (Obj*)twin), "Clone");
    DECREF(bb);
    DECREF(twin);
}

static void
test_Compare_To(TestBatchRunner *runner) {
    ByteBuf *a = BB_new_bytes("foo\0a", 5);
    ByteBuf *b = BB_new_bytes("foo\0b", 5);

    BB_Set_Size(a, 4);
    BB_Set_Size(b, 4);
    TEST_INT_EQ(runner, BB_Compare_To(a, (Obj*)b), 0,
                "Compare_To returns 0 for equal ByteBufs");

    BB_Set_Size(a, 3);
    TEST_TRUE(runner, BB_Compare_To(a, (Obj*)b) < 0,
              "shorter ByteBuf sorts first");

    BB_Set_Size(a, 5);
    BB_Set_Size(b, 5);
    TEST_TRUE(runner, BB_Compare_To(a, (Obj*)b) < 0,
              "NULL doesn't interfere with Compare_To");

    DECREF(a);
    DECREF(b);
}

static void
test_Cat(TestBatchRunner *runner) {
    ByteBuf *bb = BB_new_bytes("foo", 3);

    {
        Blob *blob = Blob_new("bar", 3);
        BB_Cat(bb, blob);
        TEST_TRUE(runner, BB_Equals_Bytes(bb, "foobar", 6), "Cat");
        DECREF(blob);
    }

    BB_Cat_Bytes(bb, "baz", 3);
    TEST_TRUE(runner, BB_Equals_Bytes(bb, "foobarbaz", 9), "Cat_Bytes");

    DECREF(bb);
}

static void
test_Utf8_To_String(TestBatchRunner *runner) {
    ByteBuf *bb = BB_new_bytes("foo", 3);

    {
        String *string = BB_Utf8_To_String(bb);
        TEST_TRUE(runner, Str_Equals_Utf8(string, "foo", 3), "Utf8_To_String");
        DECREF(string);
    }

    {
        String *string = BB_Trusted_Utf8_To_String(bb);
        TEST_TRUE(runner, Str_Equals_Utf8(string, "foo", 3),
                  "Trusted_Utf8_To_String");
        DECREF(string);
    }

    DECREF(bb);
}

void
TestBB_Run_IMP(TestByteBuf *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 17);
    test_Equals(runner);
    test_Grow(runner);
    test_Clone(runner);
    test_Compare_To(runner);
    test_Utf8_To_String(runner);
    test_Cat(runner);
}



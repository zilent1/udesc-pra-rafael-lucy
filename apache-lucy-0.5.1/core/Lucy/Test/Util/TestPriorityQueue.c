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

#define C_TESTLUCY_TESTPRIORITYQUEUE
#define TESTLUCY_USE_SHORT_NAMES
#include "Lucy/Util/ToolSet.h"

#include "Clownfish/Num.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Lucy/Test.h"
#include "Lucy/Test/Util/TestPriorityQueue.h"
#include "Lucy/Util/PriorityQueue.h"

TestPriorityQueue*
TestPriQ_new() {
    return (TestPriorityQueue*)Class_Make_Obj(TESTPRIORITYQUEUE);
}

NumPriorityQueue*
NumPriQ_new(uint32_t max_size) {
    NumPriorityQueue *self
        = (NumPriorityQueue*)Class_Make_Obj(NUMPRIORITYQUEUE);
    return (NumPriorityQueue*)PriQ_init((PriorityQueue*)self, max_size);
}

bool
NumPriQ_Less_Than_IMP(NumPriorityQueue *self, Obj *a, Obj *b) {
    Float *num_a = (Float*)a;
    Float *num_b = (Float*)b;
    UNUSED_VAR(self);
    return Float_Get_Value(num_a) < Float_Get_Value(num_b) ? true : false;
}

static void
S_insert_num(NumPriorityQueue *pq, int32_t value) {
    NumPriQ_Insert(pq, (Obj*)Float_new((double)value));
}

static int32_t
S_pop_num(NumPriorityQueue *pq) {
    Float *num = (Float*)NumPriQ_Pop(pq);
    int32_t retval;
    if (!num) { THROW(ERR, "Queue is empty"); }
    retval = (int32_t)Float_Get_Value(num);
    DECREF(num);
    return retval;
}

static void
test_Peek_and_Pop_All(TestBatchRunner *runner) {
    NumPriorityQueue *pq = NumPriQ_new(5);
    Float *val;

    S_insert_num(pq, 3);
    S_insert_num(pq, 1);
    S_insert_num(pq, 2);
    S_insert_num(pq, 20);
    S_insert_num(pq, 10);
    val = (Float*)CERTIFY(NumPriQ_Peek(pq), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val), 1,
                "peek at the least item in the queue");

    Vector  *got = NumPriQ_Pop_All(pq);
    val = (Float*)CERTIFY(Vec_Fetch(got, 0), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val), 20, "pop_all");
    val = (Float*)CERTIFY(Vec_Fetch(got, 1), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val), 10, "pop_all");
    val = (Float*)CERTIFY(Vec_Fetch(got, 2), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val),  3, "pop_all");
    val = (Float*)CERTIFY(Vec_Fetch(got, 3), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val),  2, "pop_all");
    val = (Float*)CERTIFY(Vec_Fetch(got, 4), FLOAT);
    TEST_INT_EQ(runner, (long)Float_Get_Value(val),  1, "pop_all");

    DECREF(got);
    DECREF(pq);
}

static void
test_Insert_and_Pop(TestBatchRunner *runner) {
    NumPriorityQueue *pq = NumPriQ_new(5);

    S_insert_num(pq, 3);
    S_insert_num(pq, 1);
    S_insert_num(pq, 2);
    S_insert_num(pq, 20);
    S_insert_num(pq, 10);

    TEST_INT_EQ(runner, S_pop_num(pq), 1, "Pop");
    TEST_INT_EQ(runner, S_pop_num(pq), 2, "Pop");
    TEST_INT_EQ(runner, S_pop_num(pq), 3, "Pop");
    TEST_INT_EQ(runner, S_pop_num(pq), 10, "Pop");

    S_insert_num(pq, 7);
    TEST_INT_EQ(runner, S_pop_num(pq), 7,
                "Insert after Pop still sorts correctly");

    DECREF(pq);
}

static void
test_discard(TestBatchRunner *runner) {
    int32_t i;
    NumPriorityQueue *pq = NumPriQ_new(5);

    for (i = 1; i <= 10; i++) { S_insert_num(pq, i); }
    S_insert_num(pq, -3);
    for (i = 1590; i <= 1600; i++) { S_insert_num(pq, i); }
    S_insert_num(pq, 5);

    TEST_INT_EQ(runner, S_pop_num(pq), 1596, "discard waste");
    TEST_INT_EQ(runner, S_pop_num(pq), 1597, "discard waste");
    TEST_INT_EQ(runner, S_pop_num(pq), 1598, "discard waste");
    TEST_INT_EQ(runner, S_pop_num(pq), 1599, "discard waste");
    TEST_INT_EQ(runner, S_pop_num(pq), 1600, "discard waste");

    DECREF(pq);
}

static void
test_random_insertion(TestBatchRunner *runner) {
    int i;
    int shuffled[64];
    NumPriorityQueue *pq = NumPriQ_new(64);

    for (i = 0; i < 64; i++) { shuffled[i] = i; }
    for (i = 0; i < 64; i++) {
        int shuffle_pos = rand() % 64;
        int temp = shuffled[shuffle_pos];
        shuffled[shuffle_pos] = shuffled[i];
        shuffled[i] = temp;
    }
    for (i = 0; i < 64; i++) { S_insert_num(pq, shuffled[i]); }
    for (i = 0; i < 64; i++) {
        if (S_pop_num(pq) != i) { break; }
    }
    TEST_INT_EQ(runner, i, 64, "random insertion");

    DECREF(pq);
}

void
TestPriQ_Run_IMP(TestPriorityQueue *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 17);
    test_Peek_and_Pop_All(runner);
    test_Insert_and_Pop(runner);
    test_discard(runner);
    test_random_insertion(runner);
}




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

#define C_TESTLUCY_TESTSNOWBALLSTEMMER
#define TESTLUCY_USE_SHORT_NAMES
#include "Lucy/Util/ToolSet.h"

#include "Clownfish/HashIterator.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"
#include "Lucy/Test.h"
#include "Lucy/Test/Analysis/TestSnowballStemmer.h"
#include "Lucy/Analysis/SnowballStemmer.h"
#include "Lucy/Store/FSFolder.h"
#include "Lucy/Test/TestUtils.h"
#include "Lucy/Util/Json.h"

TestSnowballStemmer*
TestSnowStemmer_new() {
    return (TestSnowballStemmer*)Class_Make_Obj(TESTSNOWBALLSTEMMER);
}

static void
test_Dump_Load_and_Equals(TestBatchRunner *runner) {
    String *EN = SSTR_WRAP_C("en");
    String *ES = SSTR_WRAP_C("es");
    SnowballStemmer *stemmer = SnowStemmer_new(EN);
    SnowballStemmer *other   = SnowStemmer_new(ES);
    Obj *dump       = (Obj*)SnowStemmer_Dump(stemmer);
    Obj *other_dump = (Obj*)SnowStemmer_Dump(other);
    SnowballStemmer *clone       = (SnowballStemmer*)SnowStemmer_Load(other, dump);
    SnowballStemmer *other_clone = (SnowballStemmer*)SnowStemmer_Load(other, other_dump);

    TEST_FALSE(runner,
               SnowStemmer_Equals(stemmer, (Obj*)other),
               "Equals() false with different language");
    TEST_TRUE(runner,
              SnowStemmer_Equals(stemmer, (Obj*)clone),
              "Dump => Load round trip");
    TEST_TRUE(runner,
              SnowStemmer_Equals(other, (Obj*)other_clone),
              "Dump => Load round trip");

    DECREF(stemmer);
    DECREF(dump);
    DECREF(clone);
    DECREF(other);
    DECREF(other_dump);
    DECREF(other_clone);
}

static void
test_stemming(TestBatchRunner *runner) {
    FSFolder *modules_folder = TestUtils_modules_folder();
    if (modules_folder == NULL) {
        SKIP(runner, 150, "Can't locate test data");
        return;
    }

    String *path = Str_newf("analysis/snowstem/source/test/tests.json");
    Hash *tests = (Hash*)Json_slurp_json((Folder*)modules_folder, path);
    if (!tests) { RETHROW(Err_get_error()); }

    HashIterator *iter = HashIter_new(tests);
    while (HashIter_Next(iter)) {
        String *iso       = HashIter_Get_Key(iter);
        Hash   *lang_data = (Hash*)HashIter_Get_Value(iter);
        Vector *words = (Vector*)Hash_Fetch_Utf8(lang_data, "words", 5);
        Vector *stems = (Vector*)Hash_Fetch_Utf8(lang_data, "stems", 5);
        SnowballStemmer *stemmer = SnowStemmer_new(iso);
        for (uint32_t i = 0, max = Vec_Get_Size(words); i < max; i++) {
            String *word  = (String*)Vec_Fetch(words, i);
            Vector *got   = SnowStemmer_Split(stemmer, word);
            String *stem  = (String*)Vec_Fetch(got, 0);
            TEST_TRUE(runner,
                      stem
                      && Str_is_a(stem, STRING)
                      && Str_Equals(stem, Vec_Fetch(stems, i)),
                      "Stem %s: %s", Str_Get_Ptr8(iso), Str_Get_Ptr8(word)
                     );
            DECREF(got);
        }
        DECREF(stemmer);
    }
    DECREF(iter);

    DECREF(tests);
    DECREF(modules_folder);
    DECREF(path);
}

void
TestSnowStemmer_Run_IMP(TestSnowballStemmer *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 153);
    test_Dump_Load_and_Equals(runner);
    test_stemming(runner);
}




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

#define C_LUCY_POLYLEXICON
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Index/PolyLexicon.h"
#include "Lucy/Index/LexiconReader.h"
#include "Lucy/Index/PostingList.h"
#include "Lucy/Index/SegLexicon.h"
#include "Lucy/Index/SegReader.h"
#include "Lucy/Util/PriorityQueue.h"

// Empty out, then refill the Queue, seeking all elements to [target].
static void
S_refresh_lex_q(SegLexQueue *lex_q, Vector *seg_lexicons, Obj *target);

PolyLexicon*
PolyLex_new(String *field, Vector *sub_readers) {
    PolyLexicon *self = (PolyLexicon*)Class_Make_Obj(POLYLEXICON);
    return PolyLex_init(self, field, sub_readers);
}

PolyLexicon*
PolyLex_init(PolyLexicon *self, String *field, Vector *sub_readers) {
    uint32_t  num_sub_readers = Vec_Get_Size(sub_readers);
    Vector   *seg_lexicons    = Vec_new(num_sub_readers);

    // Init.
    Lex_init((Lexicon*)self, field);
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    ivars->term            = NULL;
    ivars->lex_q           = SegLexQ_new(num_sub_readers);

    // Derive.
    for (uint32_t i = 0; i < num_sub_readers; i++) {
        LexiconReader *lex_reader = (LexiconReader*)Vec_Fetch(sub_readers, i);
        if (lex_reader && CERTIFY(lex_reader, LEXICONREADER)) {
            Lexicon *seg_lexicon = LexReader_Lexicon(lex_reader, field, NULL);
            if (seg_lexicon != NULL) {
                Vec_Push(seg_lexicons, (Obj*)seg_lexicon);
            }
        }
    }
    ivars->seg_lexicons  = seg_lexicons;

    PolyLex_Reset(self);

    return self;
}

void
PolyLex_Destroy_IMP(PolyLexicon *self) {
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    DECREF(ivars->seg_lexicons);
    DECREF(ivars->lex_q);
    DECREF(ivars->term);
    SUPER_DESTROY(self, POLYLEXICON);
}

static void
S_refresh_lex_q(SegLexQueue *lex_q, Vector *seg_lexicons, Obj *target) {
    // Empty out the queue.
    while (1) {
        SegLexicon *seg_lex = (SegLexicon*)SegLexQ_Pop(lex_q);
        if (seg_lex == NULL) { break; }
        DECREF(seg_lex);
    }

    // Refill the queue.
    for (uint32_t i = 0, max = Vec_Get_Size(seg_lexicons); i < max; i++) {
        SegLexicon *const seg_lexicon
            = (SegLexicon*)Vec_Fetch(seg_lexicons, i);
        SegLex_Seek(seg_lexicon, target);
        if (SegLex_Get_Term(seg_lexicon) != NULL) {
            SegLexQ_Insert(lex_q, INCREF(seg_lexicon));
        }
    }
}

void
PolyLex_Reset_IMP(PolyLexicon *self) {
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    Vector *seg_lexicons = ivars->seg_lexicons;
    uint32_t num_segs = Vec_Get_Size(seg_lexicons);
    SegLexQueue *lex_q = ivars->lex_q;

    // Empty out the queue.
    while (1) {
        SegLexicon *seg_lex = (SegLexicon*)SegLexQ_Pop(lex_q);
        if (seg_lex == NULL) { break; }
        DECREF(seg_lex);
    }

    // Fill the queue with valid SegLexicons.
    for (uint32_t i = 0; i < num_segs; i++) {
        SegLexicon *const seg_lexicon
            = (SegLexicon*)Vec_Fetch(seg_lexicons, i);
        SegLex_Reset(seg_lexicon);
        if (SegLex_Next(seg_lexicon)) {
            SegLexQ_Insert(ivars->lex_q, INCREF(seg_lexicon));
        }
    }

    if (ivars->term != NULL) {
        DECREF(ivars->term);
        ivars->term = NULL;
    }
}

bool
PolyLex_Next_IMP(PolyLexicon *self) {
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    SegLexQueue *lex_q = ivars->lex_q;
    SegLexicon *top_seg_lexicon = (SegLexicon*)SegLexQ_Peek(lex_q);

    // Churn through queue items with equal terms.
    while (top_seg_lexicon != NULL) {
        Obj *const candidate = SegLex_Get_Term(top_seg_lexicon);
        if ((candidate && !ivars->term)
            || Obj_Compare_To(ivars->term, candidate) != 0
           ) {
            // Succeed if the next item in the queue has a different term.
            Obj *temp = ivars->term;
            ivars->term = Obj_Clone(candidate);
            DECREF(temp);
            return true;
        }
        else {
            SegLexicon *seg_lex = (SegLexicon*)SegLexQ_Pop(lex_q);
            DECREF(seg_lex);
            if (SegLex_Next(top_seg_lexicon)) {
                SegLexQ_Insert(lex_q, INCREF(top_seg_lexicon));
            }
            top_seg_lexicon = (SegLexicon*)SegLexQ_Peek(lex_q);
        }
    }

    // If queue is empty, iterator is finished.
    DECREF(ivars->term);
    ivars->term = NULL;
    return false;
}

void
PolyLex_Seek_IMP(PolyLexicon *self, Obj *target) {
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    Vector *seg_lexicons = ivars->seg_lexicons;
    SegLexQueue *lex_q = ivars->lex_q;

    if (target == NULL) {
        PolyLex_Reset(self);
        return;
    }

    // Refresh the queue, set vars.
    S_refresh_lex_q(lex_q, seg_lexicons, target);
    SegLexicon *least = (SegLexicon*)SegLexQ_Peek(lex_q);
    DECREF(ivars->term);
    ivars->term = NULL;
    if (least) {
        Obj *least_term = SegLex_Get_Term(least);
        ivars->term = least_term ? Obj_Clone(least_term) : NULL;
    }

    // Scan up to the real target.
    do {
        if (ivars->term) {
            const int32_t comparison = Obj_Compare_To(ivars->term, target);
            if (comparison >= 0) { break; }
        }
    } while (PolyLex_Next(self));
}

Obj*
PolyLex_Get_Term_IMP(PolyLexicon *self) {
    return PolyLex_IVARS(self)->term;
}

uint32_t
PolyLex_Get_Num_Seg_Lexicons_IMP(PolyLexicon *self) {
    PolyLexiconIVARS *const ivars = PolyLex_IVARS(self);
    return Vec_Get_Size(ivars->seg_lexicons);
}

SegLexQueue*
SegLexQ_new(uint32_t max_size) {
    SegLexQueue *self = (SegLexQueue*)Class_Make_Obj(SEGLEXQUEUE);
    return (SegLexQueue*)PriQ_init((PriorityQueue*)self, max_size);
}

bool
SegLexQ_Less_Than_IMP(SegLexQueue *self, Obj *a, Obj *b) {
    SegLexicon *const lex_a  = (SegLexicon*)a;
    SegLexicon *const lex_b  = (SegLexicon*)b;
    Obj *const term_a = SegLex_Get_Term(lex_a);
    Obj *const term_b = SegLex_Get_Term(lex_b);
    UNUSED_VAR(self);
    return Obj_Compare_To(term_a, term_b) < 0;
}




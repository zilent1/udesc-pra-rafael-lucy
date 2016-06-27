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

#define C_LUCY_RAWLEXICON
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Index/RawLexicon.h"
#include "Lucy/Index/Posting/MatchPosting.h"
#include "Lucy/Index/TermStepper.h"
#include "Lucy/Index/TermInfo.h"
#include "Lucy/Plan/FieldType.h"
#include "Lucy/Plan/Schema.h"
#include "Lucy/Store/InStream.h"

RawLexicon*
RawLex_new(Schema *schema, String *field, InStream *instream,
           int64_t start, int64_t end) {
    RawLexicon *self = (RawLexicon*)Class_Make_Obj(RAWLEXICON);
    return RawLex_init(self, schema, field, instream, start, end);
}

RawLexicon*
RawLex_init(RawLexicon *self, Schema *schema, String *field,
            InStream *instream, int64_t start, int64_t end) {
    FieldType *type = Schema_Fetch_Type(schema, field);
    Lex_init((Lexicon*)self, field);
    RawLexiconIVARS *const ivars = RawLex_IVARS(self);

    // Assign
    ivars->start = start;
    ivars->end   = end;
    ivars->len   = end - start;
    ivars->instream = (InStream*)INCREF(instream);

    // Get ready to begin.
    InStream_Seek(ivars->instream, ivars->start);

    // Get steppers.
    ivars->term_stepper  = FType_Make_Term_Stepper(type);
    ivars->tinfo_stepper = (TermStepper*)MatchTInfoStepper_new(schema);

    return self;
}

void
RawLex_Destroy_IMP(RawLexicon *self) {
    RawLexiconIVARS *const ivars = RawLex_IVARS(self);
    DECREF(ivars->instream);
    DECREF(ivars->term_stepper);
    DECREF(ivars->tinfo_stepper);
    SUPER_DESTROY(self, RAWLEXICON);
}

bool
RawLex_Next_IMP(RawLexicon *self) {
    RawLexiconIVARS *const ivars = RawLex_IVARS(self);
    if (InStream_Tell(ivars->instream) >= ivars->len) { return false; }
    TermStepper_Read_Delta(ivars->term_stepper, ivars->instream);
    TermStepper_Read_Delta(ivars->tinfo_stepper, ivars->instream);
    return true;
}

Obj*
RawLex_Get_Term_IMP(RawLexicon *self) {
    RawLexiconIVARS *const ivars = RawLex_IVARS(self);
    return TermStepper_Get_Value(ivars->term_stepper);
}

int32_t
RawLex_Doc_Freq_IMP(RawLexicon *self) {
    RawLexiconIVARS *const ivars = RawLex_IVARS(self);
    TermInfo *tinfo = (TermInfo*)TermStepper_Get_Value(ivars->tinfo_stepper);
    return tinfo ? TInfo_Get_Doc_Freq(tinfo) : 0;
}



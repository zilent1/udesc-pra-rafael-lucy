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

#define C_LUCY_LEXICON
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Index/Lexicon.h"

Lexicon*
Lex_init(Lexicon *self, String *field) {
    LexiconIVARS *const ivars = Lex_IVARS(self);
    ivars->field = Str_Clone(field);
    ABSTRACT_CLASS_CHECK(self, LEXICON);
    return self;
}

String*
Lex_Get_Field_IMP(Lexicon *self) {
    return Lex_IVARS(self)->field;
}

void
Lex_Destroy_IMP(Lexicon *self) {
    LexiconIVARS *const ivars = Lex_IVARS(self);
    DECREF(ivars->field);
    SUPER_DESTROY(self, LEXICON);
}



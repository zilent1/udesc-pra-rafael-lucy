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

/** Clownfish::CFC::Model::DocuComment - Formatted comment a la Doxygen.
 */

#ifndef H_CFCDOCUCOMMENT
#define H_CFCDOCUCOMMENT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCDocuComment CFCDocuComment;

struct cmark_node;

/** Parse comment text.
 */
CFCDocuComment*
CFCDocuComment_parse(const char *raw_text);

void
CFCDocuComment_destroy(CFCDocuComment *self);

const char*
CFCDocuComment_get_description(CFCDocuComment *self);

const char*
CFCDocuComment_get_brief(CFCDocuComment *self);

const char*
CFCDocuComment_get_long(CFCDocuComment *self);

const char**
CFCDocuComment_get_param_names(CFCDocuComment *self);

const char**
CFCDocuComment_get_param_docs(CFCDocuComment *self);

// May be NULL.
const char*
CFCDocuComment_get_retval(CFCDocuComment *self);

int
CFCMarkdown_code_block_is_host(struct cmark_node *code_block,
                               const char *lang);

int
CFCMarkdown_code_block_is_last(struct cmark_node *code_block);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCDOCUCOMMENT */


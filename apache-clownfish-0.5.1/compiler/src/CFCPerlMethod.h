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

#ifndef H_CFCPERLMETHOD
#define H_CFCPERLMETHOD

#ifdef __cplusplus
extern "C" {
#endif

/** Clownfish::CFC::Binding::Perl::Method - Binding for an object method.
 *
 * 
 * This class isa Clownfish::CFC::Binding::Perl::Subroutine -- see its
 * documentation for various code-generating routines.
 * 
 * Method bindings use labeled parameters if the C function takes more than one
 * argument (other than "self").  If there is only one argument, the binding
 * will be set up to accept a single positional argument.
 */
typedef struct CFCPerlMethod CFCPerlMethod;
struct CFCClass;
struct CFCMethod;

CFCPerlMethod*
CFCPerlMethod_new(struct CFCClass *klass, struct CFCMethod *method);

/**
 * @param method A Clownfish::CFC::Model::Method.
 */
CFCPerlMethod*
CFCPerlMethod_init(CFCPerlMethod *self, struct CFCClass *klass,
                   struct CFCMethod *method);

void
CFCPerlMethod_destroy(CFCPerlMethod *self);

/**
 * Create the Perl name of the method.
 */
char*
CFCPerlMethod_perl_name(struct CFCMethod *method);

/** Generate C code for the XSUB.
 */
char*
CFCPerlMethod_xsub_def(CFCPerlMethod *self, struct CFCClass *klass);

/** Return C code implementing a callback to Perl for this method.  This code
 * is run when a Perl subclass has overridden a method in a Clownfish base
 * class.
 */
char*
CFCPerlMethod_callback_def(struct CFCMethod *method, struct CFCClass *klass);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCPERLMETHOD */


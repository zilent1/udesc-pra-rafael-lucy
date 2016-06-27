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

#define C_LUCY_REGEXTOKENIZER
#define C_LUCY_TOKEN
#include "XSBind.h"

#include "Lucy/Analysis/RegexTokenizer.h"
#include "Lucy/Analysis/Token.h"
#include "Lucy/Analysis/Inversion.h"
#include "Clownfish/Util/Memory.h"
#include "Clownfish/Util/StringHelper.h"

static SV*
S_compile_token_re(pTHX_ cfish_String *pattern);

bool
lucy_RegexTokenizer_is_available(void) {
    return true;
}

lucy_RegexTokenizer*
lucy_RegexTokenizer_init(lucy_RegexTokenizer *self,
                         cfish_String *pattern) {
    lucy_Analyzer_init((lucy_Analyzer*)self);
    lucy_RegexTokenizerIVARS *const ivars = lucy_RegexTokenizer_IVARS(self);
    #define DEFAULT_PATTERN "\\w+(?:['\\x{2019}]\\w+)*"
    if (pattern) {
        if (CFISH_Str_Contains_Utf8(pattern, "\\p", 2)
            || CFISH_Str_Contains_Utf8(pattern, "\\P", 2)
           ) {
            CFISH_DECREF(self);
            THROW(CFISH_ERR, "\\p and \\P constructs forbidden");
        }
        ivars->pattern = CFISH_Str_Clone(pattern);
    }
    else {
        ivars->pattern = cfish_Str_new_from_trusted_utf8(
                            DEFAULT_PATTERN, sizeof(DEFAULT_PATTERN) - 1);
    }

    // Acquire a compiled regex engine for matching one token.
    dTHX;
    SV *token_re = S_compile_token_re(aTHX_ ivars->pattern);
#if (PERL_VERSION > 10)
    REGEXP *rx = SvRX((SV*)token_re);
#else
    if (!SvROK(token_re)) {
        THROW(CFISH_ERR, "token_re is not a qr// entity");
    }
    SV *inner = SvRV(token_re);
    MAGIC *magic = NULL;
    if (SvMAGICAL((SV*)inner)) {
        magic = mg_find((SV*)inner, PERL_MAGIC_qr);
    }
    if (!magic) {
        THROW(CFISH_ERR, "token_re is not a qr// entity");
    }
    REGEXP *rx = (REGEXP*)magic->mg_obj;
#endif
    if (rx == NULL) {
        THROW(CFISH_ERR, "Failed to extract REGEXP from token_re '%s'",
              SvPV_nolen((SV*)token_re));
    }
    ivars->token_re = rx;
    (void)ReREFCNT_inc(((REGEXP*)ivars->token_re));
    SvREFCNT_dec(token_re);

    return self;
}

static SV*
S_compile_token_re(pTHX_ cfish_String *pattern) {
    dSP;
    ENTER;
    SAVETMPS;
    EXTEND(SP, 1);
    PUSHMARK(SP);
    XPUSHs((SV*)CFISH_Str_To_Host(pattern));
    PUTBACK;
    call_pv("Lucy::Analysis::RegexTokenizer::_compile_token_re", G_SCALAR);
    SPAGAIN;
    SV *token_re_sv = POPs;
    (void)SvREFCNT_inc(token_re_sv);
    PUTBACK;
    FREETMPS;
    LEAVE;
    return token_re_sv;
}

void
LUCY_RegexTokenizer_Destroy_IMP(lucy_RegexTokenizer *self) {
    dTHX;
    lucy_RegexTokenizerIVARS *const ivars = lucy_RegexTokenizer_IVARS(self);
    CFISH_DECREF(ivars->pattern);
    ReREFCNT_dec(((REGEXP*)ivars->token_re));
    CFISH_SUPER_DESTROY(self, LUCY_REGEXTOKENIZER);
}

void
LUCY_RegexTokenizer_Tokenize_Utf8_IMP(lucy_RegexTokenizer *self,
                                      const char *string, size_t string_len,
                                      lucy_Inversion *inversion) {
    dTHX;
    lucy_RegexTokenizerIVARS *const ivars = lucy_RegexTokenizer_IVARS(self);
    uint32_t   num_code_points = 0;
    SV        *wrapper    = sv_newmortal();
#if (PERL_VERSION > 10)
    REGEXP    *rx         = (REGEXP*)ivars->token_re;
    regexp    *rx_struct  = (regexp*)SvANY(rx);
#else
    REGEXP    *rx         = (REGEXP*)ivars->token_re;
    regexp    *rx_struct  = rx;
#endif
    char      *string_beg = (char*)string;
    char      *string_end = string_beg + string_len;
    char      *string_arg = string_beg;


    // Fake up an SV wrapper to feed to the regex engine.
    sv_upgrade(wrapper, SVt_PV);
    SvREADONLY_on(wrapper);
    SvLEN(wrapper) = 0;
    SvUTF8_on(wrapper);

    // Wrap the string in an SV to please the regex engine.
    SvPVX(wrapper) = string_beg;
    SvCUR_set(wrapper, string_len);
    SvPOK_on(wrapper);

    while (pregexec(rx, string_arg, string_end, string_arg, 1, wrapper, 1)) {
#if ((PERL_VERSION >= 10) || (PERL_VERSION == 9 && PERL_SUBVERSION >= 5))
        char *const start_ptr = string_arg + rx_struct->offs[0].start;
        char *const end_ptr   = string_arg + rx_struct->offs[0].end;
#else
        char *const start_ptr = string_arg + rx_struct->startp[0];
        char *const end_ptr   = string_arg + rx_struct->endp[0];
#endif
        uint32_t start, end;

        // Get start and end offsets in Unicode code points.
        for (; string_arg < start_ptr; num_code_points++) {
            string_arg += cfish_StrHelp_UTF8_COUNT[(uint8_t)(*string_arg)];
            if (string_arg > string_end) {
                THROW(CFISH_ERR, "scanned past end of '%s'", string_beg);
            }
        }
        start = num_code_points;
        for (; string_arg < end_ptr; num_code_points++) {
            string_arg += cfish_StrHelp_UTF8_COUNT[(uint8_t)(*string_arg)];
            if (string_arg > string_end) {
                THROW(CFISH_ERR, "scanned past end of '%s'", string_beg);
            }
        }
        end = num_code_points;

        // Add a token to the new inversion.
        LUCY_Inversion_Append(inversion,
                              lucy_Token_new(
                                  start_ptr,
                                  (end_ptr - start_ptr),
                                  start,
                                  end,
                                  1.0f,   // boost always 1 for now
                                  1       // position increment
                              )
                             );
    }
}



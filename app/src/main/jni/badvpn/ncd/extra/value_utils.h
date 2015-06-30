/**
 * @file value_utils.h
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NCD_VALUE_UTILS_H
#define NCD_VALUE_UTILS_H

#include <stdint.h>
#include <limits.h>

#include <misc/debug.h>
#include <misc/parse_number.h>
#include <misc/strdup.h>
#include <misc/balloc.h>
#include <system/BTime.h>
#include <ncd/NCDVal.h>
#include <ncd/NCDStringIndex.h>
#include <ncd/static_strings.h>

static int ncd_is_none (NCDValRef val);
static NCDValRef ncd_make_boolean (NCDValMem *mem, int value, NCDStringIndex *string_index);
static int ncd_read_boolean (NCDValRef val);
static int ncd_read_uintmax (NCDValRef string, uintmax_t *out) WARN_UNUSED;
static int ncd_read_time (NCDValRef string, btime_t *out) WARN_UNUSED;
static NCD_string_id_t ncd_get_string_id (NCDValRef string, NCDStringIndex *string_index);
static NCDValRef ncd_make_uintmax (NCDValMem *mem, uintmax_t value);
static char * ncd_strdup (NCDValRef stringnonulls);

static int ncd_is_none (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    if (NCDVal_IsIdString(string)) {
        return NCDVal_IdStringId(string) == NCD_STRING_NONE;
    } else {
        return NCDVal_StringEquals(string, "<none>");
    }
}

static NCDValRef ncd_make_boolean (NCDValMem *mem, int value, NCDStringIndex *string_index)
{
    ASSERT(mem)
    ASSERT(string_index)
    
    NCD_string_id_t str_id = (value ? NCD_STRING_TRUE : NCD_STRING_FALSE);
    return NCDVal_NewIdString(mem, str_id, string_index);
}

static int ncd_read_boolean (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    if (NCDVal_IsIdString(string)) {
        return NCDVal_IdStringId(string) == NCD_STRING_TRUE;
    } else {
        return NCDVal_StringEquals(string, "true");
    }
}

static int ncd_read_uintmax (NCDValRef string, uintmax_t *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    size_t length = NCDVal_StringLength(string);
    
    if (NCDVal_IsContinuousString(string)) {
        return parse_unsigned_integer_bin(NCDVal_StringData(string), length, out);
    }
    
    b_cstring cstr = NCDVal_StringCstring(string);
    
    return parse_unsigned_integer_cstr(cstr, 0, cstr.length, out);
}

static int ncd_read_time (NCDValRef string, btime_t *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    uintmax_t n;
    if (!ncd_read_uintmax(string, &n)) {
        return 0;
    }
    
    if (n > INT64_MAX) {
        return 0;
    }
    
    *out = n;
    return 1;
}

static NCD_string_id_t ncd_get_string_id (NCDValRef string, NCDStringIndex *string_index)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(string_index)
    
    if (NCDVal_IsIdString(string)) {
        return NCDVal_IdStringId(string);
    } else if (NCDVal_IsContinuousString(string)) {
        return NCDStringIndex_GetBin(string_index, NCDVal_StringData(string), NCDVal_StringLength(string));
    }
    
    b_cstring cstr = NCDVal_StringCstring(string);
    
    char *temp = b_cstring_strdup(cstr, 0, cstr.length);
    if (!temp) {
        return -1;
    }
    
    NCD_string_id_t res = NCDStringIndex_GetBin(string_index, temp, cstr.length);
    BFree(temp);
    
    return res;
}

static NCDValRef ncd_make_uintmax (NCDValMem *mem, uintmax_t value)
{
    ASSERT(mem)
    
    int size = compute_decimal_repr_size(value);
    
    NCDValRef val = NCDVal_NewStringUninitialized(mem, size);
    
    if (!NCDVal_IsInvalid(val)) {
        char *data = (char *)NCDVal_StringData(val);
        generate_decimal_repr(value, data, size);
    }
    
    return val;
}

static char * ncd_strdup (NCDValRef stringnonulls)
{
    ASSERT(NCDVal_IsStringNoNulls(stringnonulls))
    
    size_t length = NCDVal_StringLength(stringnonulls);
    
    if (NCDVal_IsContinuousString(stringnonulls)) {
        return b_strdup_bin(NCDVal_StringData(stringnonulls), length);
    }
    
    b_cstring cstr = NCDVal_StringCstring(stringnonulls);
    
    return b_cstring_strdup(cstr, 0, cstr.length);
}

#endif

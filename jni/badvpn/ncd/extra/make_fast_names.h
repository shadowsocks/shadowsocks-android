/**
 * @file make_fast_names.h
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

#include <stddef.h>

#include <misc/debug.h>
#include <misc/merge.h>
#include <misc/balloc.h>
#include <ncd/NCDStringIndex.h>

// Input parameters:
// #define NAMES_PARAM_NAME
// #define NAMES_PARAM_TYPE struct instance
// #define NAMES_PARAM_MEMBER_DYNAMIC_NAMES dynamic_names
// #define NAMES_PARAM_MEMBER_STATIC_NAMES static_names
// #define NAMES_PARAM_MEMBER_NUM_NAMES num_names
// #define NAMES_PARAM_NUM_STATIC_NAMES 10

#define MakeFastNames_count_names MERGE(NAMES_PARAM_NAME, _count_names)
#define MakeFastNames_add_name MERGE(NAMES_PARAM_NAME, _add_name)
#define MakeFastNames_InitNames MERGE(NAMES_PARAM_NAME, _InitNames)
#define MakeFastNames_FreeNames MERGE(NAMES_PARAM_NAME, _FreeNames)
#define MakeFastNames_GetNames MERGE(NAMES_PARAM_NAME, _GetNames)

static size_t MakeFastNames_count_names (const char *str, size_t str_len)
{
    size_t count = 1;
    
    while (str_len > 0) {
        if (*str == '.') {
            count++;
        }
        str++;
        str_len--;
    }
    
    return count;
}

static int MakeFastNames_add_name (NAMES_PARAM_TYPE *o, NCDStringIndex *string_index, const char *str, size_t str_len, const char *remain, size_t remain_len)
{
    ASSERT(str)
    ASSERT(!!o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES == (o->NAMES_PARAM_MEMBER_NUM_NAMES > NAMES_PARAM_NUM_STATIC_NAMES))
    
    NCD_string_id_t id = NCDStringIndex_GetBin(string_index, str, str_len);
    if (id < 0) {
        return 0;
    }
    
    if (o->NAMES_PARAM_MEMBER_NUM_NAMES < NAMES_PARAM_NUM_STATIC_NAMES) {
        o->NAMES_PARAM_MEMBER_STATIC_NAMES[o->NAMES_PARAM_MEMBER_NUM_NAMES++] = id;
        return 1;
    }
    
    if (o->NAMES_PARAM_MEMBER_NUM_NAMES == NAMES_PARAM_NUM_STATIC_NAMES) {
        size_t num_more = (!remain ? 0 : MakeFastNames_count_names(remain, remain_len));
        size_t num_all = o->NAMES_PARAM_MEMBER_NUM_NAMES + 1 + num_more;
        
        if (!(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES = BAllocArray(num_all, sizeof(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES[0])))) {
            return 0;
        }
        
        memcpy(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES, o->NAMES_PARAM_MEMBER_STATIC_NAMES, NAMES_PARAM_NUM_STATIC_NAMES * sizeof(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES[0]));
    }
    
    o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES[o->NAMES_PARAM_MEMBER_NUM_NAMES++] = id;
    
    return 1;
}

static int MakeFastNames_InitNames (NAMES_PARAM_TYPE *o, NCDStringIndex *string_index, const char *str, size_t str_len)
{
    ASSERT(str)
    
    o->NAMES_PARAM_MEMBER_NUM_NAMES = 0;
    o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES = NULL;
    
    size_t i = 0;
    while (i < str_len) {
        if (str[i] == '.') {
            if (!MakeFastNames_add_name(o, string_index, str, i, str + (i + 1), str_len - (i + 1))) {
                goto fail;
            }
            str += i + 1;
            str_len -= i + 1;
            i = 0;
            continue;
        }
        i++;
    }
    
    if (!MakeFastNames_add_name(o, string_index, str, i, NULL, 0)) {
        goto fail;
    }
    
    return 1;
    
fail:
    BFree(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES);
    return 0;
}

static void MakeFastNames_FreeNames (NAMES_PARAM_TYPE *o)
{
    if (o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES) {
        BFree(o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES);
    }
}

static NCD_string_id_t * MakeFastNames_GetNames (NAMES_PARAM_TYPE *o)
{
    ASSERT(o->NAMES_PARAM_MEMBER_NUM_NAMES > 0)
    
    return (o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES ? o->NAMES_PARAM_MEMBER_DYNAMIC_NAMES : o->NAMES_PARAM_MEMBER_STATIC_NAMES);
}

#undef MakeFastNames_count_names
#undef MakeFastNames_add_name
#undef MakeFastNames_InitNames
#undef MakeFastNames_FreeNames
#undef MakeFastNames_GetNames

#undef NAMES_PARAM_NAME
#undef NAMES_PARAM_TYPE
#undef NAMES_PARAM_MEMBER_DYNAMIC_NAMES
#undef NAMES_PARAM_MEMBER_STATIC_NAMES
#undef NAMES_PARAM_MEMBER_NUM_NAMES
#undef NAMES_PARAM_NUM_STATIC_NAMES

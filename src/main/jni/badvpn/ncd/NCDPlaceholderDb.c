/**
 * @file NCDPlaceholderDb.c
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

#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include <misc/balloc.h>
#include <misc/strdup.h>
#include <base/BLog.h>
#include <ncd/make_name_indices.h>

#include "NCDPlaceholderDb.h"

#include <generated/blog_channel_NCDPlaceholderDb.h>

int NCDPlaceholderDb_Init (NCDPlaceholderDb *o, NCDStringIndex *string_index)
{
    ASSERT(string_index)
    
    o->count = 0;
    o->capacity = 1;
    o->string_index = string_index;
    
    if (!(o->arr = BAllocArray(o->capacity, sizeof(o->arr[0])))) {
        BLog(BLOG_ERROR, "NCDPlaceholderDb_Init failed");
        return 0;
    }
    
    return 1;
}

void NCDPlaceholderDb_Free (NCDPlaceholderDb *o)
{
    for (size_t i = 0; i < o->count; i++) {
        BFree(o->arr[i].varnames);
    }
    
    BFree(o->arr);
}

int NCDPlaceholderDb_AddVariable (NCDPlaceholderDb *o, const char *varname, int *out_plid)
{
    ASSERT(varname)
    ASSERT(out_plid)
    ASSERT(o->count <= o->capacity)
    ASSERT(o->capacity > 0)
    
    if (o->count == o->capacity) {
        if (o->capacity > SIZE_MAX / 2) {
            BLog(BLOG_ERROR, "too many placeholder entries (cannot resize)");
            return 0;
        }
        size_t newcap = 2 * o->capacity;
        
        struct NCDPlaceholderDb__entry *newarr = BAllocArray(newcap, sizeof(newarr[0]));
        if (!newarr) {
            BLog(BLOG_ERROR, "BAllocArray failed");
            return 0;
        }
        
        memcpy(newarr, o->arr, o->count * sizeof(newarr[0]));
        BFree(o->arr);
        
        o->arr = newarr;
        o->capacity = newcap;
    }
    
    ASSERT(o->count < o->capacity)
    
    if (o->count > INT_MAX) {
        BLog(BLOG_ERROR, "too many placeholder entries (cannot fit integer)");
        return 0;
    }
    
    NCD_string_id_t *varnames;
    size_t num_names;
    if (!ncd_make_name_indices(o->string_index, varname, &varnames, &num_names)) {
        BLog(BLOG_ERROR, "ncd_make_name_indices failed");
        return 0;
    }
    
    *out_plid = o->count;
    
    o->arr[o->count].varnames = varnames;
    o->arr[o->count].num_names = num_names;
    o->count++;
    
    return 1;
}

void NCDPlaceholderDb_GetVariable (NCDPlaceholderDb *o, int plid, const NCD_string_id_t **out_varnames, size_t *out_num_names)
{
    ASSERT(plid >= 0)
    ASSERT(plid < o->count)
    ASSERT(out_varnames)
    ASSERT(out_num_names)
    
    *out_varnames = o->arr[plid].varnames;
    *out_num_names = o->arr[plid].num_names;
}

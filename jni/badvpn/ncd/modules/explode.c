/**
 * @file explode.c
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
 * 
 * @section DESCRIPTION
 * 
 * Synopsis:
 *   explode(string delimiter, string input [, string limit])
 * 
 * Description:
 *   Splits the string 'input' into a list of components. The first component
 *   is the part of 'input' until the first occurence of 'delimiter', if any.
 *   If 'delimiter' was found, the remaining components are defined recursively
 *   via the same procedure, starting with the part of 'input' after the first
 *   substring.
 *   'delimiter' must be nonempty.
 * 
 * Variables:
 *   list (empty) - the components of 'input', determined based on 'delimiter'
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <misc/exparray.h>
#include <misc/string_begins_with.h>
#include <misc/substring.h>
#include <misc/balloc.h>
#include <ncd/NCDModule.h>
#include <ncd/static_strings.h>
#include <ncd/extra/value_utils.h>

#include <generated/blog_channel_ncd_explode.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct instance {
    NCDModuleInst *i;
    struct ExpArray arr;
    size_t num;
};

struct substring {
    char *data;
    size_t len;
};

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
    // read arguments
    NCDValRef delimiter_arg;
    NCDValRef input_arg;
    NCDValRef limit_arg = NCDVal_NewInvalid();
    if (!NCDVal_ListRead(params->args, 2, &delimiter_arg, &input_arg) && !NCDVal_ListRead(params->args, 3, &delimiter_arg, &input_arg, &limit_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsString(delimiter_arg) || !NCDVal_IsString(input_arg) || (!NCDVal_IsInvalid(limit_arg) && !NCDVal_IsString(limit_arg))) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    size_t limit = SIZE_MAX;
    if (!NCDVal_IsInvalid(limit_arg)) {
        uintmax_t n;
        if (!ncd_read_uintmax(limit_arg, &n) || n == 0) {
            ModuleLog(i, BLOG_ERROR, "bad limit argument");
            goto fail0;
        }
        n--;
        limit = (n <= SIZE_MAX ? n : SIZE_MAX);
    }
    
    const char *del_data = NCDVal_StringData(delimiter_arg);
    size_t del_len = NCDVal_StringLength(delimiter_arg);
    
    if (del_len == 0) {
        ModuleLog(i, BLOG_ERROR, "delimiter must be nonempty");
        goto fail0;
    }
    
    size_t *table = BAllocArray(del_len, sizeof(table[0]));
    if (!table) {
        ModuleLog(i, BLOG_ERROR, "ExpArray_init failed");
        goto fail0;
    }
    
    build_substring_backtrack_table(del_data, del_len, table);
    
    if (!ExpArray_init(&o->arr, sizeof(struct substring), 8)) {
        ModuleLog(i, BLOG_ERROR, "ExpArray_init failed");
        goto fail1;
    }
    o->num = 0;
    
    const char *data = NCDVal_StringData(input_arg);
    size_t len = NCDVal_StringLength(input_arg);
    
    while (1) {
        size_t start;
        int is_end = 0;
        if (limit == 0 || !find_substring(data, len, del_data, del_len, table, &start)) {
            start = len;
            is_end = 1;
        }
        
        if (!ExpArray_resize(&o->arr, o->num + 1)) {
            ModuleLog(i, BLOG_ERROR, "ExpArray_init failed");
            goto fail2;
        }
        
        struct substring *elem = &((struct substring *)o->arr.v)[o->num];
        
        if (!(elem->data = BAlloc(start))) {
            ModuleLog(i, BLOG_ERROR, "BAlloc failed");
            goto fail2;
        }
        
        memcpy(elem->data, data, start);
        elem->len = start;
        o->num++;
        
        if (is_end) {
            break;
        }
        
        data += start + del_len;
        len -= start + del_len;
        limit--;
    }
    
    BFree(table);
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;

fail2:
    while (o->num-- > 0) {
        BFree(((struct substring *)o->arr.v)[o->num].data);
    }
    free(o->arr.v);
fail1:
    BFree(table);
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    
    while (o->num-- > 0) {
        BFree(((struct substring *)o->arr.v)[o->num].data);
    }
    free(o->arr.v);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar2 (void *vo, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    
    if (name == NCD_STRING_EMPTY) {
        *out = NCDVal_NewList(mem, o->num);
        if (NCDVal_IsInvalid(*out)) {
            goto fail;
        }
        for (size_t j = 0; j < o->num; j++) {
            struct substring *elem = &((struct substring *)o->arr.v)[j];
            NCDValRef str = NCDVal_NewStringBin(mem, (uint8_t *)elem->data, elem->len);
            if (NCDVal_IsInvalid(str)) {
                goto fail;
            }
            if (!NCDVal_ListAppend(*out, str)) {
                goto fail;
            }
        }
        return 1;
    }
    
    return 0;
    
fail:
    *out = NCDVal_NewInvalid();
    return 1;
}

static struct NCDModule modules[] = {
    {
        .type = "explode",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar2 = func_getvar2,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_explode = {
    .modules = modules
};

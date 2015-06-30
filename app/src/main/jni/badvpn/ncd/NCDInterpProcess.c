/**
 * @file NCDInterpProcess.c
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

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include <misc/balloc.h>
#include <misc/maxalign.h>
#include <misc/strdup.h>
#include <base/BLog.h>
#include <ncd/make_name_indices.h>

#include "NCDInterpProcess.h"

#include <generated/blog_channel_ncd.h>

static int compute_prealloc (NCDInterpProcess *o)
{
    int size = 0;
    
    for (int i = 0; i < o->num_stmts; i++) {
        int mod = size % BMAX_ALIGN;
        int align_size = (mod == 0 ? 0 : BMAX_ALIGN - mod);
        
        if (align_size + o->stmts[i].alloc_size > INT_MAX - size) {
            return 0;
        }
        
        o->stmts[i].prealloc_offset = size + align_size;
        size += align_size + o->stmts[i].alloc_size;
    }
    
    ASSERT(size >= 0)
    
    o->prealloc_size = size;
    
    return 1;
}

static int convert_value_recurser (NCDPlaceholderDb *pdb, NCDStringIndex *string_index, NCDValue *value, NCDValMem *mem, NCDValRef *out)
{
    ASSERT(pdb)
    ASSERT(string_index)
    ASSERT((NCDValue_Type(value), 1))
    ASSERT(mem)
    ASSERT(out)
    
    switch (NCDValue_Type(value)) {
        case NCDVALUE_STRING: {
            const char *str = NCDValue_StringValue(value);
            size_t len = NCDValue_StringLength(value);
            
            NCD_string_id_t string_id = NCDStringIndex_GetBin(string_index, str, len);
            if (string_id < 0) {
                BLog(BLOG_ERROR, "NCDStringIndex_GetBin failed");
                goto fail;
            }
            
            *out = NCDVal_NewIdString(mem, string_id, string_index);
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
        } break;
        
        case NCDVALUE_LIST: {
            *out = NCDVal_NewList(mem, NCDValue_ListCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *e = NCDValue_ListFirst(value); e; e = NCDValue_ListNext(value, e)) {
                NCDValRef vval;
                if (!convert_value_recurser(pdb, string_index, e, mem, &vval)) {
                    goto fail;
                }
                
                if (!NCDVal_ListAppend(*out, vval)) {
                    BLog(BLOG_ERROR, "depth limit exceeded");
                    goto fail;
                }
            }
        } break;
        
        case NCDVALUE_MAP: {
            *out = NCDVal_NewMap(mem, NCDValue_MapCount(value));
            if (NCDVal_IsInvalid(*out)) {
                goto fail;
            }
            
            for (NCDValue *ekey = NCDValue_MapFirstKey(value); ekey; ekey = NCDValue_MapNextKey(value, ekey)) {
                NCDValue *eval = NCDValue_MapKeyValue(value, ekey);
                
                NCDValRef vkey;
                NCDValRef vval;
                if (!convert_value_recurser(pdb, string_index, ekey, mem, &vkey) ||
                    !convert_value_recurser(pdb, string_index, eval, mem, &vval)
                ) {
                    goto fail;
                }
                
                int inserted;
                if (!NCDVal_MapInsert(*out, vkey, vval, &inserted)) {
                    BLog(BLOG_ERROR, "depth limit exceeded");
                    goto fail;
                }
                if (!inserted) {
                    BLog(BLOG_ERROR, "duplicate key in map");
                    goto fail;
                }
            }
        } break;
        
        case NCDVALUE_VAR: {
            int plid;
            if (!NCDPlaceholderDb_AddVariable(pdb, NCDValue_VarName(value), &plid)) {
                goto fail;
            }
            
            if (NCDVAL_MINIDX + plid >= -1) {
                goto fail;
            }
            
            *out = NCDVal_NewPlaceholder(mem, plid);
        } break;
        
        default:
            goto fail;
    }
    
    return 1;
    
fail:
    return 0;
}

int NCDInterpProcess_Init (NCDInterpProcess *o, NCDProcess *process, NCDStringIndex *string_index, NCDPlaceholderDb *pdb, NCDModuleIndex *module_index)
{
    ASSERT(process)
    ASSERT(string_index)
    ASSERT(pdb)
    ASSERT(module_index)
    
    NCDBlock *block = NCDProcess_Block(process);
    
    if (NCDBlock_NumStatements(block) > INT_MAX) {
        BLog(BLOG_ERROR, "too many statements");
        goto fail0;
    }
    int num_stmts = NCDBlock_NumStatements(block);
    
    if (!(o->stmts = BAllocArray(num_stmts, sizeof(o->stmts[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    o->num_hash_buckets = num_stmts;
    
    if (!(o->hash_buckets = BAllocArray(o->num_hash_buckets, sizeof(o->hash_buckets[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        goto fail1;
    }
    
    for (size_t i = 0; i < o->num_hash_buckets; i++) {
        o->hash_buckets[i] = -1;
    }
    
    if (!(o->name = b_strdup(NCDProcess_Name(process)))) {
        BLog(BLOG_ERROR, "b_strdup failed");
        goto fail2;
    }
    
    o->num_stmts = 0;
    o->prealloc_size = -1;
    o->is_template = NCDProcess_IsTemplate(process);
    o->cache = NULL;
    
    for (NCDStatement *s = NCDBlock_FirstStatement(block); s; s = NCDBlock_NextStatement(block, s)) {
        ASSERT(NCDStatement_Type(s) == NCDSTATEMENT_REG)
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        
        e->name = -1;
        e->objnames = NULL;
        e->num_objnames = 0;
        e->alloc_size = 0;
        
        if (NCDStatement_Name(s)) {
            e->name = NCDStringIndex_Get(string_index, NCDStatement_Name(s));
            if (e->name < 0) {
                BLog(BLOG_ERROR, "NCDStringIndex_Get failed");
                goto loop_fail0;
            }
        }
        
        e->cmdname = NCDStringIndex_Get(string_index, NCDStatement_RegCmdName(s));
        if (e->cmdname < 0) {
            BLog(BLOG_ERROR, "NCDStringIndex_Get failed");
            goto loop_fail0;
        }
        
        NCDValMem_Init(&e->arg_mem);
        
        NCDValRef val;
        if (!convert_value_recurser(pdb, string_index, NCDStatement_RegArgs(s), &e->arg_mem, &val)) {
            BLog(BLOG_ERROR, "convert_value_recurser failed");
            goto loop_fail1;
        }
        
        e->arg_ref = NCDVal_ToSafe(val);
        
        if (!NCDValReplaceProg_Init(&e->arg_prog, val)) {
            BLog(BLOG_ERROR, "NCDValReplaceProg_Init failed");
            goto loop_fail1;
        }
        
        if (NCDStatement_RegObjName(s)) {
            if (!ncd_make_name_indices(string_index, NCDStatement_RegObjName(s), &e->objnames, &e->num_objnames)) {
                BLog(BLOG_ERROR, "ncd_make_name_indices failed");
                goto loop_fail2;
            }
            
            e->binding.method_name_id = NCDModuleIndex_GetMethodNameId(module_index, NCDStatement_RegCmdName(s));
            if (e->binding.method_name_id == -1) {
                BLog(BLOG_ERROR, "NCDModuleIndex_GetMethodNameId failed");
                goto loop_fail3;
            }
        } else {
            e->binding.simple_module = NCDModuleIndex_FindModule(module_index, NCDStatement_RegCmdName(s));
        }
        
        if (e->name >= 0) {
            size_t bucket_idx = e->name % o->num_hash_buckets;
            e->hash_next = o->hash_buckets[bucket_idx];
            o->hash_buckets[bucket_idx] = o->num_stmts;
        }
        
        o->num_stmts++;
        continue;
        
    loop_fail3:
        BFree(e->objnames);
    loop_fail2:
        NCDValReplaceProg_Free(&e->arg_prog);
    loop_fail1:
        NCDValMem_Free(&e->arg_mem);
    loop_fail0:
        goto fail3;
    }
    
    ASSERT(o->num_stmts == num_stmts)
    
    DebugObject_Init(&o->d_obj);
    return 1;
    
fail3:
    while (o->num_stmts-- > 0) {
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        BFree(e->objnames);
        NCDValReplaceProg_Free(&e->arg_prog);
        NCDValMem_Free(&e->arg_mem);
    }
    free(o->name);
fail2:
    BFree(o->hash_buckets);
fail1:
    BFree(o->stmts);
fail0:
    return 0;
}

void NCDInterpProcess_Free (NCDInterpProcess *o)
{
    DebugObject_Free(&o->d_obj);
    
    while (o->num_stmts-- > 0) {
        struct NCDInterpProcess__stmt *e = &o->stmts[o->num_stmts];
        BFree(e->objnames);
        NCDValReplaceProg_Free(&e->arg_prog);
        NCDValMem_Free(&e->arg_mem);
    }
    
    free(o->name);
    BFree(o->hash_buckets);
    BFree(o->stmts);
}

int NCDInterpProcess_FindStatement (NCDInterpProcess *o, int from_index, NCD_string_id_t name)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(from_index >= 0)
    ASSERT(from_index <= o->num_stmts)
    
    size_t bucket_idx = name % o->num_hash_buckets;
    int stmt_idx = o->hash_buckets[bucket_idx];
    ASSERT(stmt_idx >= -1)
    ASSERT(stmt_idx < o->num_stmts)
    
    while (stmt_idx >= 0) {
        if (stmt_idx < from_index && o->stmts[stmt_idx].name == name) {
            return stmt_idx;
        }
        
        stmt_idx = o->stmts[stmt_idx].hash_next;
        ASSERT(stmt_idx >= -1)
        ASSERT(stmt_idx < o->num_stmts)
    }
    
    return -1;
}

const char * NCDInterpProcess_StatementCmdName (NCDInterpProcess *o, int i, NCDStringIndex *string_index)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(string_index)
    
    return NCDStringIndex_Value(string_index, o->stmts[i].cmdname);
}

void NCDInterpProcess_StatementObjNames (NCDInterpProcess *o, int i, const NCD_string_id_t **out_objnames, size_t *out_num_objnames)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(out_objnames)
    ASSERT(out_num_objnames)
    
    *out_objnames = o->stmts[i].objnames;
    *out_num_objnames = o->stmts[i].num_objnames;
}

const struct NCDInterpModule * NCDInterpProcess_StatementGetSimpleModule (NCDInterpProcess *o, int i, NCDStringIndex *string_index, NCDModuleIndex *module_index)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(!o->stmts[i].objnames)
    
    struct NCDInterpProcess__stmt *e = &o->stmts[i];
    
    if (!e->binding.simple_module) {
        const char *cmdname = NCDStringIndex_Value(string_index, e->cmdname);
        e->binding.simple_module = NCDModuleIndex_FindModule(module_index, cmdname);
    }
    
    return e->binding.simple_module;
}

const struct NCDInterpModule * NCDInterpProcess_StatementGetMethodModule (NCDInterpProcess *o, int i, NCD_string_id_t obj_type, NCDModuleIndex *module_index)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->stmts[i].objnames)
    ASSERT(obj_type >= 0)
    ASSERT(module_index)
    
    return NCDModuleIndex_GetMethodModule(module_index, obj_type, o->stmts[i].binding.method_name_id);
}

int NCDInterpProcess_CopyStatementArgs (NCDInterpProcess *o, int i, NCDValMem *out_valmem, NCDValRef *out_val, NCDValReplaceProg *out_prog)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(out_valmem)
    ASSERT(out_val)
    ASSERT(out_prog)
    
    struct NCDInterpProcess__stmt *e = &o->stmts[i];
    
    if (!NCDValMem_InitCopy(out_valmem, &e->arg_mem)) {
        return 0;
    }
    
    *out_val = NCDVal_FromSafe(out_valmem, e->arg_ref);
    *out_prog = e->arg_prog;
    return 1;
}

void NCDInterpProcess_StatementBumpAllocSize (NCDInterpProcess *o, int i, int alloc_size)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(alloc_size >= 0)
    
    if (alloc_size > o->stmts[i].alloc_size) {
        o->stmts[i].alloc_size = alloc_size;
        o->prealloc_size = -1;
    }
}

int NCDInterpProcess_PreallocSize (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->prealloc_size == -1 || o->prealloc_size >= 0)
    
    if (o->prealloc_size < 0 && !compute_prealloc(o)) {
        return -1;
    }
    
    return o->prealloc_size;
}

int NCDInterpProcess_StatementPreallocSize (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->prealloc_size >= 0)
    
    return o->stmts[i].alloc_size;
}

int NCDInterpProcess_StatementPreallocOffset (NCDInterpProcess *o, int i)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(i >= 0)
    ASSERT(i < o->num_stmts)
    ASSERT(o->prealloc_size >= 0)
    
    return o->stmts[i].prealloc_offset;
}

const char * NCDInterpProcess_Name (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->name;
}

int NCDInterpProcess_IsTemplate (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->is_template;
}

int NCDInterpProcess_NumStatements (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->num_stmts;
}

int NCDInterpProcess_CachePush (NCDInterpProcess *o, void *elem)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(elem)
    
    if (o->cache) {
        return 0;
    }
    
    o->cache = elem;
    
    return 1;
}

void * NCDInterpProcess_CachePull (NCDInterpProcess *o)
{
    DebugObject_Access(&o->d_obj);
    
    void *elem = o->cache;
    o->cache = NULL;
    
    return elem;
}

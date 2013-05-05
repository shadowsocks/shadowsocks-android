/**
 * @file NCDVal.c
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
#include <stddef.h>
#include <stdarg.h>

#include <misc/balloc.h>
#include <misc/strdup.h>
#include <misc/offset.h>
#include <base/BLog.h>

#include "NCDVal.h"

#include <generated/blog_channel_NCDVal.h>

#define TYPE_MASK_EXTERNAL_TYPE ((1 << 3) - 1)
#define TYPE_MASK_INTERNAL_TYPE ((1 << 5) - 1)
#define TYPE_SHIFT_DEPTH 5

#define STOREDSTRING_TYPE (NCDVAL_STRING | (0 << 3))
#define IDSTRING_TYPE (NCDVAL_STRING | (1 << 3))
#define EXTERNALSTRING_TYPE (NCDVAL_STRING | (2 << 3))
#define COMPOSEDSTRING_TYPE (NCDVAL_STRING | (3 << 3))

static int make_type (int internal_type, int depth)
{
    ASSERT(internal_type == NCDVAL_LIST ||
           internal_type == NCDVAL_MAP ||
           internal_type == STOREDSTRING_TYPE ||
           internal_type == IDSTRING_TYPE ||
           internal_type == EXTERNALSTRING_TYPE ||
           internal_type == COMPOSEDSTRING_TYPE)
    ASSERT(depth >= 0)
    ASSERT(depth <= NCDVAL_MAX_DEPTH)
    
    return (internal_type | (depth << TYPE_SHIFT_DEPTH));
}

static int get_external_type (int type)
{
    return (type & TYPE_MASK_EXTERNAL_TYPE);
}

static int get_internal_type (int type)
{
    return (type & TYPE_MASK_INTERNAL_TYPE);
}

static int get_depth (int type)
{
    return (type >> TYPE_SHIFT_DEPTH);
}

static int bump_depth (int *type_ptr, int elem_depth)
{
    if (get_depth(*type_ptr) < elem_depth + 1) {
        if (elem_depth + 1 > NCDVAL_MAX_DEPTH) {
            return 0;
        }
        *type_ptr = make_type(get_internal_type(*type_ptr), elem_depth + 1);
    }
    
    return 1;
}

static void * NCDValMem__BufAt (NCDValMem *o, NCDVal__idx idx)
{
    ASSERT(idx >= 0)
    ASSERT(idx < o->used)
    
    return (o->buf ? o->buf : o->fastbuf) + idx;
}

static NCDVal__idx NCDValMem__Alloc (NCDValMem *o, NCDVal__idx alloc_size, NCDVal__idx align)
{
    NCDVal__idx mod = o->used % align;
    NCDVal__idx align_extra = mod ? (align - mod) : 0;
    
    if (alloc_size > NCDVAL_MAXIDX - align_extra) {
        return -1;
    }
    NCDVal__idx aligned_alloc_size = align_extra + alloc_size;
    
    if (aligned_alloc_size > o->size - o->used) {
        NCDVal__idx newsize = (o->buf ? o->size : NCDVAL_FIRST_SIZE);
        while (aligned_alloc_size > newsize - o->used) {
            if (newsize > NCDVAL_MAXIDX / 2) {
                return -1;
            }
            newsize *= 2;
        }
        
        char *newbuf;
        
        if (!o->buf) {
            newbuf = malloc(newsize);
            if (!newbuf) {
                return -1;
            }
            memcpy(newbuf, o->fastbuf, o->used);
        } else {
            newbuf = realloc(o->buf, newsize);
            if (!newbuf) {
                return -1;
            }
        }
        
        o->buf = newbuf;
        o->size = newsize;
    }
    
    NCDVal__idx idx = o->used + align_extra;
    o->used += aligned_alloc_size;
    
    return idx;
}

static NCDValRef NCDVal__Ref (NCDValMem *mem, NCDVal__idx idx)
{
    ASSERT(idx == -1 || mem)
    
    NCDValRef ref = {mem, idx};
    return ref;
}

static void NCDVal__AssertMem (NCDValMem *mem)
{
    ASSERT(mem)
    ASSERT(mem->size >= 0)
    ASSERT(mem->used >= 0)
    ASSERT(mem->used <= mem->size)
    ASSERT(mem->buf || mem->size == NCDVAL_FASTBUF_SIZE)
    ASSERT(!mem->buf || mem->size >= NCDVAL_FIRST_SIZE)
}

static void NCDVal_AssertExternal (NCDValMem *mem, const void *e_buf, size_t e_len)
{
#ifndef NDEBUG
    const char *e_cbuf = e_buf;
    char *buf = (mem->buf ? mem->buf : mem->fastbuf);
    ASSERT(e_cbuf >= buf + mem->size || e_cbuf + e_len <= buf)
#endif
}

static void NCDVal__AssertValOnly (NCDValMem *mem, NCDVal__idx idx)
{
    // placeholders
    if (idx < -1) {
        return;
    }
    
    ASSERT(idx >= 0)
    ASSERT(idx + sizeof(int) <= mem->used)
    
#ifndef NDEBUG
    int *type_ptr = NCDValMem__BufAt(mem, idx);
    
    ASSERT(get_depth(*type_ptr) >= 0)
    ASSERT(get_depth(*type_ptr) <= NCDVAL_MAX_DEPTH)
    
    switch (get_internal_type(*type_ptr)) {
        case STOREDSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__string) <= mem->used)
            struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
            ASSERT(str_e->length >= 0)
            ASSERT(idx + sizeof(struct NCDVal__string) + str_e->length + 1 <= mem->used)
        } break;
        case NCDVAL_LIST: {
            ASSERT(idx + sizeof(struct NCDVal__list) <= mem->used)
            struct NCDVal__list *list_e = NCDValMem__BufAt(mem, idx);
            ASSERT(list_e->maxcount >= 0)
            ASSERT(list_e->count >= 0)
            ASSERT(list_e->count <= list_e->maxcount)
            ASSERT(idx + sizeof(struct NCDVal__list) + list_e->maxcount * sizeof(NCDVal__idx) <= mem->used)
        } break;
        case NCDVAL_MAP: {
            ASSERT(idx + sizeof(struct NCDVal__map) <= mem->used)
            struct NCDVal__map *map_e = NCDValMem__BufAt(mem, idx);
            ASSERT(map_e->maxcount >= 0)
            ASSERT(map_e->count >= 0)
            ASSERT(map_e->count <= map_e->maxcount)
            ASSERT(idx + sizeof(struct NCDVal__map) + map_e->maxcount * sizeof(struct NCDVal__mapelem) <= mem->used)
        } break;
        case IDSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__idstring) <= mem->used)
            struct NCDVal__idstring *ids_e = NCDValMem__BufAt(mem, idx);
            ASSERT(ids_e->string_id >= 0)
            ASSERT(ids_e->string_index)
        } break;
        case EXTERNALSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__externalstring) <= mem->used)
            struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(mem, idx);
            ASSERT(exs_e->data)
            ASSERT(!exs_e->ref.target || exs_e->ref.next >= -1)
            ASSERT(!exs_e->ref.target || exs_e->ref.next < mem->used)
        } break;
        case COMPOSEDSTRING_TYPE: {
            ASSERT(idx + sizeof(struct NCDVal__composedstring) <= mem->used)
            struct NCDVal__composedstring *cms_e = NCDValMem__BufAt(mem, idx);
            ASSERT(cms_e->func_getptr)
            ASSERT(!cms_e->ref.target || cms_e->ref.next >= -1)
            ASSERT(!cms_e->ref.target || cms_e->ref.next < mem->used)
        } break;
        default: ASSERT(0);
    }
#endif
}

static void NCDVal__AssertVal (NCDValRef val)
{
    NCDVal__AssertMem(val.mem);
    NCDVal__AssertValOnly(val.mem, val.idx);
}

static NCDValMapElem NCDVal__MapElem (NCDVal__idx elemidx)
{
    ASSERT(elemidx >= 0 || elemidx == -1)
    
    NCDValMapElem me = {elemidx};
    return me;
}

static void NCDVal__MapAssertElemOnly (NCDValRef map, NCDVal__idx elemidx)
{
#ifndef NDEBUG
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    ASSERT(elemidx >= map.idx + offsetof(struct NCDVal__map, elems))
    ASSERT(elemidx < map.idx + offsetof(struct NCDVal__map, elems) + map_e->count * sizeof(struct NCDVal__mapelem))

    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, elemidx);
    NCDVal__AssertValOnly(map.mem, me_e->key_idx);
    NCDVal__AssertValOnly(map.mem, me_e->val_idx);
#endif
}

static void NCDVal__MapAssertElem (NCDValRef map, NCDValMapElem me)
{
    ASSERT(NCDVal_IsMap(map))
    NCDVal__MapAssertElemOnly(map, me.elemidx);
}

static NCDVal__idx NCDVal__MapElemIdx (NCDVal__idx mapidx, NCDVal__idx pos)
{
    return mapidx + offsetof(struct NCDVal__map, elems) + pos * sizeof(struct NCDVal__mapelem);
}

static int NCDVal__Depth (NCDValRef val)
{
    ASSERT(val.idx != -1)
    
    // handle placeholders
    if (val.idx < 0) {
        return 0;
    }
    
    int *elem_type_ptr = NCDValMem__BufAt(val.mem, val.idx);
    int depth = get_depth(*elem_type_ptr);
    ASSERT(depth >= 0)
    ASSERT(depth <= NCDVAL_MAX_DEPTH)
    
    return depth;
}

static int NCDValMem__NeedRegisterLink (NCDValMem *mem, NCDVal__idx val_idx)
{
    NCDVal__AssertValOnly(mem, val_idx);
    
    return !(val_idx < -1) && get_internal_type(*(int *)NCDValMem__BufAt(mem, val_idx)) == COMPOSEDSTRING_TYPE;
}

static int NCDValMem__RegisterLink (NCDValMem *mem, NCDVal__idx val_idx, NCDVal__idx link_idx)
{
    NCDVal__AssertValOnly(mem, val_idx);
    ASSERT(NCDValMem__NeedRegisterLink(mem, val_idx))
    
    NCDVal__idx cms_link_idx = NCDValMem__Alloc(mem, sizeof(struct NCDVal__cms_link), __alignof(struct NCDVal__cms_link));
    if (cms_link_idx < 0) {
        return 0;
    }
    
    struct NCDVal__cms_link *cms_link = NCDValMem__BufAt(mem, cms_link_idx);
    cms_link->link_idx = link_idx;
    cms_link->next_cms_link = mem->first_cms_link;
    mem->first_cms_link = cms_link_idx;
    
    return 1;
}

static void NCDValMem__PopLastRegisteredLink (NCDValMem *mem)
{
    ASSERT(mem->first_cms_link != -1)
    
    struct NCDVal__cms_link *cms_link = NCDValMem__BufAt(mem, mem->first_cms_link);
    mem->first_cms_link = cms_link->next_cms_link;
}

static NCDValRef NCDVal__CopyComposedStringToStored (NCDValRef val)
{
    ASSERT(NCDVal_IsComposedString(val))
    
    struct NCDVal__composedstring cms_e = *(struct NCDVal__composedstring *)NCDValMem__BufAt(val.mem, val.idx);
    
    NCDValRef copy = NCDVal_NewStringUninitialized(val.mem, cms_e.length);
    if (NCDVal_IsInvalid(copy)) {
        return NCDVal_NewInvalid();
    }
    
    char *copy_data = (char *)NCDVal_StringData(copy);
    
    size_t pos = 0;
    while (pos < cms_e.length) {
        const char *chunk_data;
        size_t chunk_len;
        cms_e.func_getptr(cms_e.user, cms_e.offset + pos, &chunk_data, &chunk_len);
        ASSERT(chunk_data)
        ASSERT(chunk_len > 0)
        if (chunk_len > cms_e.length - pos) {
            chunk_len = cms_e.length - pos;
        }
        memcpy(copy_data + pos, chunk_data, chunk_len);
        pos += chunk_len;
    }
    
    return copy;
}

static const char * NCDVal__composedstring_cstring_func (const b_cstring *cstr, size_t offset, size_t *out_length)
{
    ASSERT(offset < cstr->length)
    ASSERT(out_length)
    ASSERT(cstr->func == NCDVal__composedstring_cstring_func)
    
    size_t str_offset = cstr->user1.size;
    NCDVal_ComposedString_func_getptr func_getptr = (NCDVal_ComposedString_func_getptr)cstr->user2.fptr;
    void *user = cstr->user3.ptr;
    
    const char *data;
    func_getptr(user, str_offset + offset, &data, out_length);
    
    ASSERT(data)
    ASSERT(*out_length > 0)
    
    return data;
}

#include "NCDVal_maptree.h"
#include <structure/CAvl_impl.h>

void NCDValMem_Init (NCDValMem *o)
{
    o->buf = NULL;
    o->size = NCDVAL_FASTBUF_SIZE;
    o->used = 0;
    o->first_ref = -1;
    o->first_cms_link = -1;
}

void NCDValMem_Free (NCDValMem *o)
{
    NCDVal__AssertMem(o);
    
    NCDVal__idx refidx = o->first_ref;
    while (refidx != -1) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, refidx);
        ASSERT(ref->target)
        BRefTarget_Deref(ref->target);
        refidx = ref->next;
    }
    
    if (o->buf) {
        BFree(o->buf);
    }
}

int NCDValMem_InitCopy (NCDValMem *o, NCDValMem *other)
{
    NCDVal__AssertMem(other);
    
    o->size = other->size;
    o->used = other->used;
    o->first_ref = other->first_ref;
    o->first_cms_link = other->first_cms_link;
    
    if (!other->buf) {
        o->buf = NULL;
        memcpy(o->fastbuf, other->fastbuf, other->used);
    } else {
        o->buf = BAlloc(other->size);
        if (!o->buf) {
            goto fail0;
        }
        memcpy(o->buf, other->buf, other->used);
    }
    
    NCDVal__idx refidx = o->first_ref;
    while (refidx != -1) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, refidx);
        ASSERT(ref->target)
        if (!BRefTarget_Ref(ref->target)) {
            goto fail1;
        }
        refidx = ref->next;
    }
    
    return 1;
    
fail1:;
    NCDVal__idx undo_refidx = o->first_ref;
    while (undo_refidx != refidx) {
        struct NCDVal__ref *ref = NCDValMem__BufAt(o, undo_refidx);
        BRefTarget_Deref(ref->target);
        undo_refidx = ref->next;
    }
    if (o->buf) {
        BFree(o->buf);
    }
fail0:
    return 0;
}

int NCDValMem_ConvertNonContinuousStrings (NCDValMem *o, NCDValRef *root_val)
{
    NCDVal__AssertMem(o);
    ASSERT(root_val)
    ASSERT(root_val->mem == o)
    NCDVal__AssertValOnly(o, root_val->idx);
    
    while (o->first_cms_link != -1) {
        struct NCDVal__cms_link cms_link = *(struct NCDVal__cms_link *)NCDValMem__BufAt(o, o->first_cms_link);
        
        NCDVal__idx val_idx = *(NCDVal__idx *)NCDValMem__BufAt(o, cms_link.link_idx);
        NCDValRef val = NCDVal__Ref(o, val_idx);
        ASSERT(NCDVal_IsComposedString(val))
        
        NCDValRef copy = NCDVal__CopyComposedStringToStored(val);
        if (NCDVal_IsInvalid(copy)) {
            return 0;
        }
        
        *(int *)NCDValMem__BufAt(o, cms_link.link_idx) = copy.idx;
        
        o->first_cms_link = cms_link.next_cms_link;
    }
    
    if (NCDVal_IsComposedString(*root_val)) {
        NCDValRef copy = NCDVal__CopyComposedStringToStored(*root_val);
        if (NCDVal_IsInvalid(copy)) {
            return 0;
        }
        *root_val = copy;
    }
    
    return 1;
}

void NCDVal_Assert (NCDValRef val)
{
    ASSERT(val.idx == -1 || (NCDVal__AssertVal(val), 1))
}

int NCDVal_IsInvalid (NCDValRef val)
{
    NCDVal_Assert(val);
    
    return (val.idx == -1);
}

int NCDVal_IsPlaceholder (NCDValRef val)
{
    NCDVal_Assert(val);
    
    return (val.idx < -1);
}

int NCDVal_Type (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    if (val.idx < -1) {
        return NCDVAL_PLACEHOLDER;
    }
    
    int *type_ptr = NCDValMem__BufAt(val.mem, val.idx);
    
    return get_external_type(*type_ptr);
}

NCDValRef NCDVal_NewInvalid (void)
{
    NCDValRef ref = {NULL, -1};
    return ref;
}

NCDValRef NCDVal_NewPlaceholder (NCDValMem *mem, int plid)
{
    NCDVal__AssertMem(mem);
    ASSERT(plid >= 0)
    ASSERT(NCDVAL_MINIDX + plid < -1)
    
    NCDValRef ref = {mem, NCDVAL_MINIDX + plid};
    return ref;
}

int NCDVal_PlaceholderId (NCDValRef val)
{
    ASSERT(NCDVal_IsPlaceholder(val))
    
    return (val.idx - NCDVAL_MINIDX);
}

NCDValRef NCDVal_NewCopy (NCDValMem *mem, NCDValRef val)
{
    NCDVal__AssertMem(mem);
    NCDVal__AssertVal(val);
    
    if (val.idx < -1) {
        return NCDVal_NewPlaceholder(mem, NCDVal_PlaceholderId(val));
    }
    
    void *ptr = NCDValMem__BufAt(val.mem, val.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            
            NCDVal__idx size = sizeof(struct NCDVal__string) + str_e->length + 1;
            NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__string));
            if (idx < 0) {
                goto fail;
            }
            
            str_e = NCDValMem__BufAt(val.mem, val.idx);
            struct NCDVal__string *new_str_e = NCDValMem__BufAt(mem, idx);
            
            memcpy(new_str_e, str_e, size);
            
            return NCDVal__Ref(mem, idx);
        } break;
        
        case NCDVAL_LIST: {
            struct NCDVal__list *list_e = ptr;
            
            NCDVal__idx size = sizeof(struct NCDVal__list) + list_e->maxcount * sizeof(NCDVal__idx);
            NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__list));
            if (idx < 0) {
                goto fail;
            }
            
            list_e = NCDValMem__BufAt(val.mem, val.idx);
            struct NCDVal__list *new_list_e = NCDValMem__BufAt(mem, idx);
            
            *new_list_e = *list_e;
            
            NCDVal__idx count = list_e->count;
            
            for (NCDVal__idx i = 0; i < count; i++) {
                NCDValRef elem_copy = NCDVal_NewCopy(mem, NCDVal__Ref(val.mem, list_e->elem_indices[i]));
                if (NCDVal_IsInvalid(elem_copy)) {
                    goto fail;
                }
                
                if (NCDValMem__NeedRegisterLink(mem, elem_copy.idx)) {
                    if (!NCDValMem__RegisterLink(mem, elem_copy.idx, idx + offsetof(struct NCDVal__list, elem_indices) + i * sizeof(NCDVal__idx))) {
                        goto fail;
                    }
                }
                
                list_e = NCDValMem__BufAt(val.mem, val.idx);
                new_list_e = NCDValMem__BufAt(mem, idx);
                
                new_list_e->elem_indices[i] = elem_copy.idx;
            }
            
            return NCDVal__Ref(mem, idx);
        } break;
        
        case NCDVAL_MAP: {
            size_t count = NCDVal_MapCount(val);
            
            NCDValRef copy = NCDVal_NewMap(mem, count);
            if (NCDVal_IsInvalid(copy)) {
                goto fail;
            }
            
            for (NCDValMapElem e = NCDVal_MapFirst(val); !NCDVal_MapElemInvalid(e); e = NCDVal_MapNext(val, e)) {
                NCDValRef key_copy = NCDVal_NewCopy(mem, NCDVal_MapElemKey(val, e));
                NCDValRef val_copy = NCDVal_NewCopy(mem, NCDVal_MapElemVal(val, e));
                if (NCDVal_IsInvalid(key_copy) || NCDVal_IsInvalid(val_copy)) {
                    goto fail;
                }
                
                int inserted;
                if (!NCDVal_MapInsert(copy, key_copy, val_copy, &inserted)) {
                    goto fail;
                }
                ASSERT_EXECUTE(inserted)
            }
            
            return copy;
        } break;
        
        case IDSTRING_TYPE: {
            NCDVal__idx size = sizeof(struct NCDVal__idstring);
            NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__idstring));
            if (idx < 0) {
                goto fail;
            }
            
            struct NCDVal__idstring *ids_e = NCDValMem__BufAt(val.mem, val.idx);
            struct NCDVal__idstring *new_ids_e = NCDValMem__BufAt(mem, idx);
            
            *new_ids_e = *ids_e;
            
            return NCDVal__Ref(mem, idx);
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            
            return NCDVal_NewExternalString(mem, exs_e->data, exs_e->length, exs_e->ref.target);
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            struct NCDVal__composedstring *cms_e = ptr;
            
            NCDValComposedStringResource resource;
            resource.func_getptr = cms_e->func_getptr;
            resource.user = cms_e->user;
            resource.ref_target = cms_e->ref.target;
            
            return NCDVal_NewComposedString(mem, resource, cms_e->offset, cms_e->length);
        } break;
        
        default: ASSERT(0);
    }
    
    ASSERT(0);
    
fail:
    return NCDVal_NewInvalid();
}

int NCDVal_Compare (NCDValRef val1, NCDValRef val2)
{
    NCDVal__AssertVal(val1);
    NCDVal__AssertVal(val2);
    
    int type1 = NCDVal_Type(val1);
    int type2 = NCDVal_Type(val2);
    
    if (type1 != type2) {
        return (type1 > type2) - (type1 < type2);
    }
    
    switch (type1) {
        case NCDVAL_STRING: {
            size_t len1 = NCDVal_StringLength(val1);
            size_t len2 = NCDVal_StringLength(val2);
            size_t min_len = len1 < len2 ? len1 : len2;
            
            int cmp = NCDVal_StringMemCmp(val1, val2, 0, 0, min_len);
            if (cmp) {
                return (cmp > 0) - (cmp < 0);
            }
            
            return (len1 > len2) - (len1 < len2);
        } break;
        
        case NCDVAL_LIST: {
            size_t count1 = NCDVal_ListCount(val1);
            size_t count2 = NCDVal_ListCount(val2);
            size_t min_count = count1 < count2 ? count1 : count2;
            
            for (size_t i = 0; i < min_count; i++) {
                NCDValRef ev1 = NCDVal_ListGet(val1, i);
                NCDValRef ev2 = NCDVal_ListGet(val2, i);
                
                int cmp = NCDVal_Compare(ev1, ev2);
                if (cmp) {
                    return cmp;
                }
            }
            
            return (count1 > count2) - (count1 < count2);
        } break;
        
        case NCDVAL_MAP: {
            NCDValMapElem e1 = NCDVal_MapOrderedFirst(val1);
            NCDValMapElem e2 = NCDVal_MapOrderedFirst(val2);
            
            while (1) {
                int inv1 = NCDVal_MapElemInvalid(e1);
                int inv2 = NCDVal_MapElemInvalid(e2);
                if (inv1 || inv2) {
                    return inv2 - inv1;
                }
                
                NCDValRef key1 = NCDVal_MapElemKey(val1, e1);
                NCDValRef key2 = NCDVal_MapElemKey(val2, e2);
                
                int cmp = NCDVal_Compare(key1, key2);
                if (cmp) {
                    return cmp;
                }
                
                NCDValRef value1 = NCDVal_MapElemVal(val1, e1);
                NCDValRef value2 = NCDVal_MapElemVal(val2, e2);
                
                cmp = NCDVal_Compare(value1, value2);
                if (cmp) {
                    return cmp;
                }
                
                e1 = NCDVal_MapOrderedNext(val1, e1);
                e2 = NCDVal_MapOrderedNext(val2, e2);
            }
        } break;
        
        case NCDVAL_PLACEHOLDER: {
            int plid1 = NCDVal_PlaceholderId(val1);
            int plid2 = NCDVal_PlaceholderId(val2);
            
            return (plid1 > plid2) - (plid1 < plid2);
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

NCDValSafeRef NCDVal_ToSafe (NCDValRef val)
{
    NCDVal_Assert(val);
    
    NCDValSafeRef sval = {val.idx};
    return sval;
}

NCDValRef NCDVal_FromSafe (NCDValMem *mem, NCDValSafeRef sval)
{
    NCDVal__AssertMem(mem);
    ASSERT(sval.idx == -1 || (NCDVal__AssertValOnly(mem, sval.idx), 1))
    
    NCDValRef val = {mem, sval.idx};
    return val;
}

NCDValRef NCDVal_Moved (NCDValMem *mem, NCDValRef val)
{
    NCDVal__AssertMem(mem);
    ASSERT(val.idx == -1 || (NCDVal__AssertValOnly(mem, val.idx), 1))
    
    NCDValRef val2 = {mem, val.idx};
    return val2;
}

int NCDVal_HasOnlyContinuousStrings (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    switch (NCDVal_Type(val)) {
        case NCDVAL_STRING: {
            if (!NCDVal_IsContinuousString(val)) {
                return 0;
            }
        } break;
        
        case NCDVAL_LIST: {
            size_t count = NCDVal_ListCount(val);
            for (size_t i = 0; i < count; i++) {
                NCDValRef elem = NCDVal_ListGet(val, i);
                if (!NCDVal_HasOnlyContinuousStrings(elem)) {
                    return 0;
                }
            }
        } break;
        
        case NCDVAL_MAP: {
            for (NCDValMapElem me = NCDVal_MapFirst(val); !NCDVal_MapElemInvalid(me); me = NCDVal_MapNext(val, me)) {
                NCDValRef e_key = NCDVal_MapElemKey(val, me);
                NCDValRef e_val = NCDVal_MapElemVal(val, me);
                if (!NCDVal_HasOnlyContinuousStrings(e_key) || !NCDVal_HasOnlyContinuousStrings(e_val)) {
                    return 0;
                }
            }
        } break;
        
        case NCDVAL_PLACEHOLDER: {
        } break;
        
        default:
            ASSERT(0);
    }
    
    return 1;
}

int NCDVal_IsString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_STRING;
}

int NCDVal_IsContinuousString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    if (val.idx < -1) {
        return 0;
    }
    
    switch (get_internal_type(*(int *)NCDValMem__BufAt(val.mem, val.idx))) {
        case STOREDSTRING_TYPE:
        case IDSTRING_TYPE:
        case EXTERNALSTRING_TYPE:
            return 1;
        default:
            return 0;
    }
}

int NCDVal_IsStoredString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && get_internal_type(*(int *)NCDValMem__BufAt(val.mem, val.idx)) == STOREDSTRING_TYPE;
}

int NCDVal_IsIdString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && get_internal_type(*(int *)NCDValMem__BufAt(val.mem, val.idx)) == IDSTRING_TYPE;
}

int NCDVal_IsExternalString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && get_internal_type(*(int *)NCDValMem__BufAt(val.mem, val.idx)) == EXTERNALSTRING_TYPE;
}

int NCDVal_IsComposedString (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return !(val.idx < -1) && get_internal_type(*(int *)NCDValMem__BufAt(val.mem, val.idx)) == COMPOSEDSTRING_TYPE;
}

int NCDVal_IsStringNoNulls (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_STRING && !NCDVal_StringHasNulls(val);
}

NCDValRef NCDVal_NewString (NCDValMem *mem, const char *data)
{
    NCDVal__AssertMem(mem);
    ASSERT(data)
    NCDVal_AssertExternal(mem, data, strlen(data));
    
    return NCDVal_NewStringBin(mem, (const uint8_t *)data, strlen(data));
}

NCDValRef NCDVal_NewStringBin (NCDValMem *mem, const uint8_t *data, size_t len)
{
    NCDVal__AssertMem(mem);
    ASSERT(len == 0 || data)
    NCDVal_AssertExternal(mem, data, len);
    
    if (len > NCDVAL_MAXIDX - sizeof(struct NCDVal__string) - 1) {
        goto fail;
    }
    
    NCDVal__idx size = sizeof(struct NCDVal__string) + len + 1;
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__string));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
    str_e->type = make_type(STOREDSTRING_TYPE, 0);
    str_e->length = len;
    if (len > 0) {
        memcpy(str_e->data, data, len);
    }
    str_e->data[len] = '\0';
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewStringUninitialized (NCDValMem *mem, size_t len)
{
    NCDVal__AssertMem(mem);
    
    if (len > NCDVAL_MAXIDX - sizeof(struct NCDVal__string) - 1) {
        goto fail;
    }
    
    NCDVal__idx size = sizeof(struct NCDVal__string) + len + 1;
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__string));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__string *str_e = NCDValMem__BufAt(mem, idx);
    str_e->type = make_type(STOREDSTRING_TYPE, 0);
    str_e->length = len;
    str_e->data[len] = '\0';
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewIdString (NCDValMem *mem, NCD_string_id_t string_id, NCDStringIndex *string_index)
{
    NCDVal__AssertMem(mem);
    ASSERT(string_id >= 0)
    ASSERT(string_index)
    
    NCDVal__idx size = sizeof(struct NCDVal__idstring);
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__idstring));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(mem, idx);
    ids_e->type = make_type(IDSTRING_TYPE, 0);
    ids_e->string_id = string_id;
    ids_e->string_index = string_index;
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewExternalString (NCDValMem *mem, const char *data, size_t len,
                                    BRefTarget *ref_target)
{
    NCDVal__AssertMem(mem);
    ASSERT(data)
    NCDVal_AssertExternal(mem, data, len);
    
    NCDVal__idx size = sizeof(struct NCDVal__externalstring);
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__externalstring));
    if (idx < 0) {
        goto fail;
    }
    
    if (ref_target) {
        if (!BRefTarget_Ref(ref_target)) {
            goto fail;
        }
    }
    
    struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(mem, idx);
    exs_e->type = make_type(EXTERNALSTRING_TYPE, 0);
    exs_e->data = data;
    exs_e->length = len;
    exs_e->ref.target = ref_target;
    
    if (ref_target) {
        exs_e->ref.next = mem->first_ref;
        mem->first_ref = idx + offsetof(struct NCDVal__externalstring, ref);
    }
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

NCDValRef NCDVal_NewComposedString (NCDValMem *mem, NCDValComposedStringResource resource, size_t offset, size_t length)
{
    NCDVal__AssertMem(mem);
    ASSERT(resource.func_getptr)
    
    NCDVal__idx size = sizeof(struct NCDVal__composedstring);
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__composedstring));
    if (idx < 0) {
        goto fail;
    }
    
    if (resource.ref_target) {
        if (!BRefTarget_Ref(resource.ref_target)) {
            goto fail;
        }
    }
    
    struct NCDVal__composedstring *cms_e = NCDValMem__BufAt(mem, idx);
    cms_e->type = make_type(COMPOSEDSTRING_TYPE, 0);
    cms_e->offset = offset;
    cms_e->length = length;
    cms_e->func_getptr = resource.func_getptr;
    cms_e->user = resource.user;
    cms_e->ref.target = resource.ref_target;
    
    if (resource.ref_target) {
        cms_e->ref.next = mem->first_ref;
        mem->first_ref = idx + offsetof(struct NCDVal__composedstring, ref);
    }
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

const char * NCDVal_StringData (NCDValRef contstring)
{
    ASSERT(NCDVal_IsContinuousString(contstring))
    
    void *ptr = NCDValMem__BufAt(contstring.mem, contstring.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            return str_e->data;
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            const char *value = NCDStringIndex_Value(ids_e->string_index, ids_e->string_id);
            return value;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            return exs_e->data;
        } break;
        
        default:
            ASSERT(0);
            return NULL;
    }
}

size_t NCDVal_StringLength (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            return str_e->length;
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            return NCDStringIndex_Length(ids_e->string_index, ids_e->string_id);
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            return exs_e->length;
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            struct NCDVal__composedstring *cms_e = ptr;
            return cms_e->length;
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

b_cstring NCDValComposedStringResource_Cstring (NCDValComposedStringResource resource, size_t offset, size_t length)
{
    b_cstring cstr;
    cstr.length = length;
    cstr.func = NCDVal__composedstring_cstring_func;
    cstr.user1.size = offset;
    cstr.user2.fptr = (void (*) (void))resource.func_getptr;
    cstr.user3.ptr = resource.user;
    return cstr;
}

b_cstring NCDVal_StringCstring (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            return b_cstring_make_buf(str_e->data, str_e->length);
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            return b_cstring_make_buf(NCDStringIndex_Value(ids_e->string_index, ids_e->string_id), NCDStringIndex_Length(ids_e->string_index, ids_e->string_id));
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            return b_cstring_make_buf(exs_e->data, exs_e->length);
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            struct NCDVal__composedstring *cms_e = ptr;
            b_cstring cstr;
            cstr.length = cms_e->length;
            cstr.func = NCDVal__composedstring_cstring_func;
            cstr.user1.size = cms_e->offset;
            cstr.user2.fptr = (void (*) (void))cms_e->func_getptr;
            cstr.user3.ptr = cms_e->user;
            return cstr;
        } break;
        
        default: {
            ASSERT(0);
            return b_cstring_make_empty();
        } break;
    }
}

int NCDVal_StringNullTerminate (NCDValRef string, NCDValNullTermString *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            out->data = str_e->data;
            out->is_allocated = 0;
            return 1;
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            out->data = (char *)NCDStringIndex_Value(ids_e->string_index, ids_e->string_id);
            out->is_allocated = 0;
            return 1;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            
            char *copy = b_strdup_bin(exs_e->data, exs_e->length);
            if (!copy) {
                return 0;
            }
            
            out->data = copy;
            out->is_allocated = 1;
            return 1;
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            struct NCDVal__composedstring *cms_e = ptr;
            size_t length = cms_e->length;
            
            if (length == SIZE_MAX) {
                return 0;
            }
            
            char *copy = BAlloc(length + 1);
            if (!copy) {
                return 0;
            }
            
            NCDVal_StringCopyOut(string, 0, length, copy);
            copy[length] = '\0';
            
            out->data = copy;
            out->is_allocated = 1;
            return 1;
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

NCDValNullTermString NCDValNullTermString_NewDummy (void)
{
    NCDValNullTermString nts;
    nts.data = NULL;
    nts.is_allocated = 0;
    return nts;
}

void NCDValNullTermString_Free (NCDValNullTermString *o)
{
    if (o->is_allocated) {
        BFree(o->data);
    }
}

int NCDVal_StringContinuize (NCDValRef string, NCDValContString *out)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(out)
    
    if (NCDVal_IsContinuousString(string)) {
        out->data = (char *)NCDVal_StringData(string);
        out->is_allocated = 0;
        return 1;
    }
    
    size_t length = NCDVal_StringLength(string);
    
    char *data = BAlloc(length);
    if (!data) {
        return 0;
    }
    
    NCDVal_StringCopyOut(string, 0, length, data);
    
    out->data = data;
    out->is_allocated = 1;
    return 1;
}

NCDValContString NCDValContString_NewDummy (void)
{
    NCDValContString cts;
    cts.data = NULL;
    cts.is_allocated = 0;
    return cts;
}

void NCDValContString_Free (NCDValContString *o)
{
    if (o->is_allocated) {
        BFree(o->data);
    }
}

void NCDVal_IdStringGet (NCDValRef idstring, NCD_string_id_t *out_string_id,
                         NCDStringIndex **out_string_index)
{
    ASSERT(NCDVal_IsIdString(idstring))
    ASSERT(out_string_id)
    ASSERT(out_string_index)
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    *out_string_id = ids_e->string_id;
    *out_string_index = ids_e->string_index;
}

NCD_string_id_t NCDVal_IdStringId (NCDValRef idstring)
{
    ASSERT(NCDVal_IsIdString(idstring))
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    return ids_e->string_id;
}

NCDStringIndex * NCDVal_IdStringStringIndex (NCDValRef idstring)
{
    ASSERT(NCDVal_IsIdString(idstring))
    
    struct NCDVal__idstring *ids_e = NCDValMem__BufAt(idstring.mem, idstring.idx);
    return ids_e->string_index;
}

BRefTarget * NCDVal_ExternalStringTarget (NCDValRef externalstring)
{
    ASSERT(NCDVal_IsExternalString(externalstring))
    
    struct NCDVal__externalstring *exs_e = NCDValMem__BufAt(externalstring.mem, externalstring.idx);
    return exs_e->ref.target;
}

NCDValComposedStringResource NCDVal_ComposedStringResource (NCDValRef composedstring)
{
    ASSERT(NCDVal_IsComposedString(composedstring))
    
    struct NCDVal__composedstring *cms_e = NCDValMem__BufAt(composedstring.mem, composedstring.idx);
    
    NCDValComposedStringResource res;
    res.func_getptr = cms_e->func_getptr;
    res.user = cms_e->user;
    res.ref_target = cms_e->ref.target;
    
    return res;
}

size_t NCDVal_ComposedStringOffset (NCDValRef composedstring)
{
    ASSERT(NCDVal_IsComposedString(composedstring))
    
    struct NCDVal__composedstring *cms_e = NCDValMem__BufAt(composedstring.mem, composedstring.idx);
    
    return cms_e->offset;
}

int NCDVal_StringHasNulls (NCDValRef string)
{
    ASSERT(NCDVal_IsString(string))
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            return NCDStringIndex_HasNulls(ids_e->string_index, ids_e->string_id);
        } break;
        
        case STOREDSTRING_TYPE:
        case EXTERNALSTRING_TYPE: {
            const char *data = NCDVal_StringData(string);
            size_t length = NCDVal_StringLength(string);
            return !!memchr(data, '\0', length);
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            b_cstring cstr = NCDVal_StringCstring(string);
            return b_cstring_memchr(cstr, 0, cstr.length, '\0', NULL);
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

int NCDVal_StringEquals (NCDValRef string, const char *data)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(data)
    
    size_t data_len = strlen(data);
    
    return NCDVal_StringLength(string) == data_len && NCDVal_StringRegionEquals(string, 0, data_len, data);
}

int NCDVal_StringEqualsId (NCDValRef string, NCD_string_id_t string_id,
                           NCDStringIndex *string_index)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(string_id >= 0)
    ASSERT(string_index)
    
    void *ptr = NCDValMem__BufAt(string.mem, string.idx);
    
    switch (get_internal_type(*(int *)ptr)) {
        case STOREDSTRING_TYPE: {
            struct NCDVal__string *str_e = ptr;
            const char *string_data = NCDStringIndex_Value(string_index, string_id);
            size_t string_length = NCDStringIndex_Length(string_index, string_id);
            return (string_length == str_e->length) && !memcmp(string_data, str_e->data, string_length);
        } break;
        
        case IDSTRING_TYPE: {
            struct NCDVal__idstring *ids_e = ptr;
            ASSERT(ids_e->string_index == string_index)
            return ids_e->string_id == string_id;
        } break;
        
        case EXTERNALSTRING_TYPE: {
            struct NCDVal__externalstring *exs_e = ptr;
            const char *string_data = NCDStringIndex_Value(string_index, string_id);
            size_t string_length = NCDStringIndex_Length(string_index, string_id);
            return (string_length == exs_e->length) && !memcmp(string_data, exs_e->data, string_length);
        } break;
        
        case COMPOSEDSTRING_TYPE: {
            struct NCDVal__composedstring *cms_e = ptr;
            const char *string_data = NCDStringIndex_Value(string_index, string_id);
            size_t string_length = NCDStringIndex_Length(string_index, string_id);
            return (string_length == cms_e->length) && NCDVal_StringRegionEquals(string, 0, string_length, string_data);
        } break;
        
        default:
            ASSERT(0);
            return 0;
    }
}

int NCDVal_StringMemCmp (NCDValRef string1, NCDValRef string2, size_t start1, size_t start2, size_t length)
{
    ASSERT(NCDVal_IsString(string1))
    ASSERT(NCDVal_IsString(string2))
    ASSERT(start1 <= NCDVal_StringLength(string1))
    ASSERT(start2 <= NCDVal_StringLength(string2))
    ASSERT(length <= NCDVal_StringLength(string1) - start1)
    ASSERT(length <= NCDVal_StringLength(string2) - start2)
    
    if (NCDVal_IsContinuousString(string1) && NCDVal_IsContinuousString(string2)) {
        return memcmp(NCDVal_StringData(string1) + start1, NCDVal_StringData(string2) + start2, length);
    }
    
    b_cstring cstr1 = NCDVal_StringCstring(string1);
    b_cstring cstr2 = NCDVal_StringCstring(string2);
    return b_cstring_memcmp(cstr1, cstr2, start1, start2, length);
}

void NCDVal_StringCopyOut (NCDValRef string, size_t start, size_t length, char *dst)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(start <= NCDVal_StringLength(string))
    ASSERT(length <= NCDVal_StringLength(string) - start)
    
    if (NCDVal_IsContinuousString(string)) {
        memcpy(dst, NCDVal_StringData(string) + start, length);
        return;
    }
    
    b_cstring cstr = NCDVal_StringCstring(string);
    b_cstring_copy_to_buf(cstr, start, length, dst);
}

int NCDVal_StringRegionEquals (NCDValRef string, size_t start, size_t length, const char *data)
{
    ASSERT(NCDVal_IsString(string))
    ASSERT(start <= NCDVal_StringLength(string))
    ASSERT(length <= NCDVal_StringLength(string) - start)
    
    if (NCDVal_IsContinuousString(string)) {
        return !memcmp(NCDVal_StringData(string) + start, data, length);
    }
    
    b_cstring cstr = NCDVal_StringCstring(string);
    return b_cstring_equals_buffer(cstr, start, length, data);
}

int NCDVal_IsList (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_LIST;
}

NCDValRef NCDVal_NewList (NCDValMem *mem, size_t maxcount)
{
    NCDVal__AssertMem(mem);
    
    if (maxcount > (NCDVAL_MAXIDX - sizeof(struct NCDVal__list)) / sizeof(NCDVal__idx)) {
        goto fail;
    }
    
    NCDVal__idx size = sizeof(struct NCDVal__list) + maxcount * sizeof(NCDVal__idx);
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__list));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(mem, idx);
    list_e->type = make_type(NCDVAL_LIST, 0);
    list_e->maxcount = maxcount;
    list_e->count = 0;
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

int NCDVal_ListAppend (NCDValRef list, NCDValRef elem)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(NCDVal_ListCount(list) < NCDVal_ListMaxCount(list))
    ASSERT(elem.mem == list.mem)
    NCDVal__AssertValOnly(list.mem, elem.idx);
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    int new_type = list_e->type;
    if (!bump_depth(&new_type, NCDVal__Depth(elem))) {
        return 0;
    }
    
    if (NCDValMem__NeedRegisterLink(list.mem, elem.idx)) {
        if (!NCDValMem__RegisterLink(list.mem, elem.idx, list.idx + offsetof(struct NCDVal__list, elem_indices) + list_e->count * sizeof(NCDVal__idx))) {
            return 0;
        }
        list_e = NCDValMem__BufAt(list.mem, list.idx);
    }
    
    list_e->type = new_type;
    list_e->elem_indices[list_e->count++] = elem.idx;
    
    return 1;
}

size_t NCDVal_ListCount (NCDValRef list)
{
    ASSERT(NCDVal_IsList(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    return list_e->count;
}

size_t NCDVal_ListMaxCount (NCDValRef list)
{
    ASSERT(NCDVal_IsList(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    return list_e->maxcount;
}

NCDValRef NCDVal_ListGet (NCDValRef list, size_t pos)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(pos < NCDVal_ListCount(list))
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    ASSERT(pos < list_e->count)
    NCDVal__AssertValOnly(list.mem, list_e->elem_indices[pos]);
    
    return NCDVal__Ref(list.mem, list_e->elem_indices[pos]);
}

int NCDVal_ListRead (NCDValRef list, int num, ...)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(num >= 0)
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    if (num != list_e->count) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    for (int i = 0; i < num; i++) {
        NCDValRef *dest = va_arg(ap, NCDValRef *);
        *dest = NCDVal__Ref(list.mem, list_e->elem_indices[i]);
    }
    
    va_end(ap);
    
    return 1;
}

int NCDVal_ListReadHead (NCDValRef list, int num, ...)
{
    ASSERT(NCDVal_IsList(list))
    ASSERT(num >= 0)
    
    struct NCDVal__list *list_e = NCDValMem__BufAt(list.mem, list.idx);
    
    if (num > list_e->count) {
        return 0;
    }
    
    va_list ap;
    va_start(ap, num);
    
    for (int i = 0; i < num; i++) {
        NCDValRef *dest = va_arg(ap, NCDValRef *);
        *dest = NCDVal__Ref(list.mem, list_e->elem_indices[i]);
    }
    
    va_end(ap);
    
    return 1;
}

int NCDVal_IsMap (NCDValRef val)
{
    NCDVal__AssertVal(val);
    
    return NCDVal_Type(val) == NCDVAL_MAP;
}

NCDValRef NCDVal_NewMap (NCDValMem *mem, size_t maxcount)
{
    NCDVal__AssertMem(mem);
    
    if (maxcount > (NCDVAL_MAXIDX - sizeof(struct NCDVal__map)) / sizeof(struct NCDVal__mapelem)) {
        goto fail;
    }
    
    NCDVal__idx size = sizeof(struct NCDVal__map) + maxcount * sizeof(struct NCDVal__mapelem);
    NCDVal__idx idx = NCDValMem__Alloc(mem, size, __alignof(struct NCDVal__map));
    if (idx < 0) {
        goto fail;
    }
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(mem, idx);
    map_e->type = make_type(NCDVAL_MAP, 0);
    map_e->maxcount = maxcount;
    map_e->count = 0;
    NCDVal__MapTree_Init(&map_e->tree);
    
    return NCDVal__Ref(mem, idx);
    
fail:
    return NCDVal_NewInvalid();
}

int NCDVal_MapInsert (NCDValRef map, NCDValRef key, NCDValRef val, int *out_inserted)
{
    ASSERT(NCDVal_IsMap(map))
    ASSERT(NCDVal_MapCount(map) < NCDVal_MapMaxCount(map))
    ASSERT(key.mem == map.mem)
    ASSERT(val.mem == map.mem)
    NCDVal__AssertValOnly(map.mem, key.idx);
    NCDVal__AssertValOnly(map.mem, val.idx);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    int new_type = map_e->type;
    if (!bump_depth(&new_type, NCDVal__Depth(key)) || !bump_depth(&new_type, NCDVal__Depth(val))) {
        goto fail0;
    }
    
    NCDVal__idx elemidx = NCDVal__MapElemIdx(map.idx, map_e->count);
    
    if (NCDValMem__NeedRegisterLink(map.mem, key.idx)) {
        if (!NCDValMem__RegisterLink(map.mem, key.idx, elemidx + offsetof(struct NCDVal__mapelem, key_idx))) {
            goto fail0;
        }
        map_e = NCDValMem__BufAt(map.mem, map.idx);
    }
    
    if (NCDValMem__NeedRegisterLink(map.mem, val.idx)) {
        if (!NCDValMem__RegisterLink(map.mem, val.idx, elemidx + offsetof(struct NCDVal__mapelem, val_idx))) {
            goto fail1;
        }
        map_e = NCDValMem__BufAt(map.mem, map.idx);
    }
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, elemidx);
    ASSERT(me_e == &map_e->elems[map_e->count])
    me_e->key_idx = key.idx;
    me_e->val_idx = val.idx;
    
    int res = NCDVal__MapTree_Insert(&map_e->tree, map.mem, NCDVal__MapTreeDeref(map.mem, elemidx), NULL);
    if (!res) {
        if (out_inserted) {
            *out_inserted = 0;
        }
        return 1;
    }
    
    map_e->type = new_type;
    map_e->count++;
    
    if (out_inserted) {
        *out_inserted = 1;
    }
    return 1;
    
fail1:
    if (NCDValMem__NeedRegisterLink(map.mem, key.idx)) {
        NCDValMem__PopLastRegisteredLink(map.mem);
    }
fail0:
    return 0;
}

size_t NCDVal_MapCount (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    return map_e->count;
}

size_t NCDVal_MapMaxCount (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    return map_e->maxcount;
}

int NCDVal_MapElemInvalid (NCDValMapElem me)
{
    ASSERT(me.elemidx >= 0 || me.elemidx == -1)
    
    return me.elemidx < 0;
}

NCDValMapElem NCDVal_MapFirst (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    if (map_e->count == 0) {
        return NCDVal__MapElem(-1);
    }
    
    NCDVal__idx elemidx = NCDVal__MapElemIdx(map.idx, 0);
    NCDVal__MapAssertElemOnly(map, elemidx);
    
    return NCDVal__MapElem(elemidx);
}

NCDValMapElem NCDVal_MapNext (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    ASSERT(map_e->count > 0)
    
    NCDVal__idx last_elemidx = NCDVal__MapElemIdx(map.idx, map_e->count - 1);
    ASSERT(me.elemidx <= last_elemidx)
    
    if (me.elemidx == last_elemidx) {
        return NCDVal__MapElem(-1);
    }
    
    NCDVal__idx elemidx = me.elemidx + sizeof(struct NCDVal__mapelem);
    NCDVal__MapAssertElemOnly(map, elemidx);
    
    return NCDVal__MapElem(elemidx);
}

NCDValMapElem NCDVal_MapOrderedFirst (NCDValRef map)
{
    ASSERT(NCDVal_IsMap(map))
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_GetFirst(&map_e->tree, map.mem);
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValMapElem NCDVal_MapOrderedNext (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_GetNext(&map_e->tree, map.mem, NCDVal__MapTreeDeref(map.mem, me.elemidx));
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValRef NCDVal_MapElemKey (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, me.elemidx);
    
    return NCDVal__Ref(map.mem, me_e->key_idx);
}

NCDValRef NCDVal_MapElemVal (NCDValRef map, NCDValMapElem me)
{
    NCDVal__MapAssertElem(map, me);
    
    struct NCDVal__mapelem *me_e = NCDValMem__BufAt(map.mem, me.elemidx);
    
    return NCDVal__Ref(map.mem, me_e->val_idx);
}

NCDValMapElem NCDVal_MapFindKey (NCDValRef map, NCDValRef key)
{
    ASSERT(NCDVal_IsMap(map))
    NCDVal__AssertVal(key);
    
    struct NCDVal__map *map_e = NCDValMem__BufAt(map.mem, map.idx);
    
    NCDVal__MapTreeRef ref = NCDVal__MapTree_LookupExact(&map_e->tree, map.mem, key);
    ASSERT(ref.link == -1 || (NCDVal__MapAssertElemOnly(map, ref.link), 1))
    
    return NCDVal__MapElem(ref.link);
}

NCDValRef NCDVal_MapGetValue (NCDValRef map, const char *key_str)
{
    ASSERT(NCDVal_IsMap(map))
    ASSERT(key_str)
    
    NCDValMem mem;
    mem.buf = NULL;
    mem.size = NCDVAL_FASTBUF_SIZE;
    mem.used = sizeof(struct NCDVal__externalstring);
    mem.first_ref = -1;
    
    struct NCDVal__externalstring *exs_e = (void *)mem.fastbuf;
    exs_e->type = make_type(EXTERNALSTRING_TYPE, 0);
    exs_e->data = key_str;
    exs_e->length = strlen(key_str);
    exs_e->ref.target = NULL;
    
    NCDValRef key = NCDVal__Ref(&mem, 0);
    
    NCDValMapElem elem = NCDVal_MapFindKey(map, key);
    if (NCDVal_MapElemInvalid(elem)) {
        return NCDVal_NewInvalid();
    }
    
    return NCDVal_MapElemVal(map, elem);
}

static void replaceprog_build_recurser (NCDValMem *mem, NCDVal__idx idx, size_t *out_num_instr, NCDValReplaceProg *prog)
{
    ASSERT(idx >= 0)
    NCDVal__AssertValOnly(mem, idx);
    ASSERT(out_num_instr)
    
    *out_num_instr = 0;
    
    void *ptr = NCDValMem__BufAt(mem, idx);
    
    struct NCDVal__instr instr;
    
    switch (get_internal_type(*((int *)(ptr)))) {
        case STOREDSTRING_TYPE:
        case IDSTRING_TYPE:
        case EXTERNALSTRING_TYPE:
        case COMPOSEDSTRING_TYPE: {
        } break;
        
        case NCDVAL_LIST: {
            struct NCDVal__list *list_e = ptr;
            
            for (NCDVal__idx i = 0; i < list_e->count; i++) {
                int elem_changed = 0;
                
                if (list_e->elem_indices[i] < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = list_e->elem_indices[i] - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__list, elem_indices) + i * sizeof(NCDVal__idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                    elem_changed = 1;
                } else {
                    size_t elem_num_instr;
                    replaceprog_build_recurser(mem, list_e->elem_indices[i], &elem_num_instr, prog);
                    (*out_num_instr) += elem_num_instr;
                    if (elem_num_instr > 0) {
                        elem_changed = 1;
                    }
                }
                
                if (elem_changed) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_BUMPDEPTH;
                        instr.bumpdepth.parent_idx = idx;
                        instr.bumpdepth.child_idx_idx = idx + offsetof(struct NCDVal__list, elem_indices) + i * sizeof(NCDVal__idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                }
            }
        } break;
        
        case NCDVAL_MAP: {
            struct NCDVal__map *map_e = ptr;
            
            for (NCDVal__idx i = 0; i < map_e->count; i++) {
                int key_changed = 0;
                int val_changed = 0;
                
                if (map_e->elems[i].key_idx < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = map_e->elems[i].key_idx - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, key_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                    key_changed = 1;
                } else {
                    size_t key_num_instr;
                    replaceprog_build_recurser(mem, map_e->elems[i].key_idx, &key_num_instr, prog);
                    (*out_num_instr) += key_num_instr;
                    if (key_num_instr > 0) {
                        key_changed = 1;
                    }
                }
                
                if (map_e->elems[i].val_idx < -1) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_PLACEHOLDER;
                        instr.placeholder.plid = map_e->elems[i].val_idx - NCDVAL_MINIDX;
                        instr.placeholder.plidx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, val_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                    val_changed = 1;
                } else {
                    size_t val_num_instr;
                    replaceprog_build_recurser(mem, map_e->elems[i].val_idx, &val_num_instr, prog);
                    (*out_num_instr) += val_num_instr;
                    if (val_num_instr > 0) {
                        val_changed = 1;
                    }
                }
                
                if (key_changed) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_REINSERT;
                        instr.reinsert.mapidx = idx;
                        instr.reinsert.elempos = i;
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                    
                    if (prog) {
                        instr.type = NCDVAL_INSTR_BUMPDEPTH;
                        instr.bumpdepth.parent_idx = idx;
                        instr.bumpdepth.child_idx_idx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, key_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                }
                
                if (val_changed) {
                    if (prog) {
                        instr.type = NCDVAL_INSTR_BUMPDEPTH;
                        instr.bumpdepth.parent_idx = idx;
                        instr.bumpdepth.child_idx_idx = idx + offsetof(struct NCDVal__map, elems) + i * sizeof(struct NCDVal__mapelem) + offsetof(struct NCDVal__mapelem, val_idx);
                        prog->instrs[prog->num_instrs++] = instr;
                    }
                    (*out_num_instr)++;
                }
            }
        } break;
        
        default: ASSERT(0);
    }
}

int NCDValReplaceProg_Init (NCDValReplaceProg *o, NCDValRef val)
{
    NCDVal__AssertVal(val);
    ASSERT(!NCDVal_IsPlaceholder(val))
    
    size_t num_instrs;
    replaceprog_build_recurser(val.mem, val.idx, &num_instrs, NULL);
    
    if (!(o->instrs = BAllocArray(num_instrs, sizeof(o->instrs[0])))) {
        BLog(BLOG_ERROR, "BAllocArray failed");
        return 0;
    }
    
    o->num_instrs = 0;
    
    size_t num_instrs2;
    replaceprog_build_recurser(val.mem, val.idx, &num_instrs2, o);
    
    ASSERT(num_instrs2 == num_instrs)
    ASSERT(o->num_instrs == num_instrs)
    
    return 1;
}

void NCDValReplaceProg_Free (NCDValReplaceProg *o)
{
    BFree(o->instrs);
}

int NCDValReplaceProg_Execute (NCDValReplaceProg prog, NCDValMem *mem, NCDVal_replace_func replace, void *arg)
{
    NCDVal__AssertMem(mem);
    ASSERT(replace)
    
    for (size_t i = 0; i < prog.num_instrs; i++) {
        struct NCDVal__instr instr = prog.instrs[i];
        
        switch (instr.type) {
            case NCDVAL_INSTR_PLACEHOLDER: {
#ifndef NDEBUG
                NCDVal__idx *check_plptr = NCDValMem__BufAt(mem, instr.placeholder.plidx);
                ASSERT(*check_plptr < -1)
                ASSERT(*check_plptr - NCDVAL_MINIDX == instr.placeholder.plid)
#endif
                NCDValRef repval;
                if (!replace(arg, instr.placeholder.plid, mem, &repval) || NCDVal_IsInvalid(repval)) {
                    return 0;
                }
                ASSERT(repval.mem == mem)
                
                if (NCDValMem__NeedRegisterLink(mem, repval.idx)) {
                    NCDValMem__RegisterLink(mem, repval.idx, instr.placeholder.plidx);
                }
                
                NCDVal__idx *plptr = NCDValMem__BufAt(mem, instr.placeholder.plidx);
                *plptr = repval.idx;
            } break;
            
            case NCDVAL_INSTR_REINSERT: {
                NCDVal__AssertValOnly(mem, instr.reinsert.mapidx);
                struct NCDVal__map *map_e = NCDValMem__BufAt(mem, instr.reinsert.mapidx);
                ASSERT(map_e->type == NCDVAL_MAP)
                ASSERT(instr.reinsert.elempos >= 0)
                ASSERT(instr.reinsert.elempos < map_e->count)
                
                NCDVal__MapTreeRef ref = {&map_e->elems[instr.reinsert.elempos], NCDVal__MapElemIdx(instr.reinsert.mapidx, instr.reinsert.elempos)};
                NCDVal__MapTree_Remove(&map_e->tree, mem, ref);
                if (!NCDVal__MapTree_Insert(&map_e->tree, mem, ref, NULL)) {
                    BLog(BLOG_ERROR, "duplicate key in map");
                    return 0;
                }
            } break;
            
            case NCDVAL_INSTR_BUMPDEPTH: {
                NCDVal__AssertValOnly(mem, instr.bumpdepth.parent_idx);
                int *parent_type_ptr = NCDValMem__BufAt(mem, instr.bumpdepth.parent_idx);
                
                NCDVal__idx *child_type_idx_ptr = NCDValMem__BufAt(mem, instr.bumpdepth.child_idx_idx);
                NCDVal__AssertValOnly(mem, *child_type_idx_ptr);
                int *child_type_ptr = NCDValMem__BufAt(mem, *child_type_idx_ptr);
                
                if (!bump_depth(parent_type_ptr, get_depth(*child_type_ptr))) {
                    BLog(BLOG_ERROR, "depth limit exceeded");
                    return 0;
                }
            } break;
            
            default: ASSERT(0);
        }
    }
    
    return 1;
}

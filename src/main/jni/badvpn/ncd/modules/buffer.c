/**
 * @file buffer.c
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
 *   buffer([string data])
 * 
 * Variables:
 *   string (empty) - data in the buffer
 *   string length - number of bytes in the buffer
 * 
 * Description:
 *   Implements an array of bytes which supports appending bytes and removing
 *   bytes from the beginning. The buffer is implemented using chunks;
 *   the time complexity of operations depends on the number of chunks affected,
 *   and not on the actual number of bytes. Each append operation produces a single
 *   chunk. In particular:
 * 
 *   Complexity of append and construction:
 *     log(total number of chunks) + (time for copying data).
 *   Complexity of consume:
 *     log(total number of chunks) * (1 + (number of chunks in consumed range))
 *   Complexity of referencing and unreferencing a range:
 *     log(total number of chunks) * (1 + (number of chunks in referenced range))
 * 
 * Synopsis:
 *   buffer::append(string data)
 * 
 * Description:
 *   Appends the given data to the end of the buffer.
 * 
 * Synopsis:
 *   buffer::consume(string amount)
 * 
 * Description:
 *   Removes the specified number of bytes from the beginning of the buffer.
 *   'amount' must not be larger than the current length of the buffer.
 */

#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <misc/debug.h>
#include <misc/balloc.h>
#include <misc/compare.h>
#include <misc/offset.h>
#include <structure/SAvl.h>
#include <ncd/NCDModule.h>
#include <ncd/static_strings.h>
#include <ncd/extra/value_utils.h>

#include <generated/blog_channel_ncd_buffer.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

struct chunk;

#include "buffer_chunks_tree.h"
#include <structure/SAvl_decl.h>

struct buffer {
    struct instance *inst;
    ChunksTree chunks_tree;
    int refcnt;
};

struct chunk {
    struct buffer *buf;
    size_t offset;
    size_t length;
    ChunksTreeNode chunks_tree_node;
    int refcnt;
    char data[];
};

struct reference {
    struct chunk *first_chunk;
    size_t first_offset;
    size_t length;
    BRefTarget ref_target;
};

struct instance {
    NCDModuleInst *i;
    size_t offset;
    size_t total_length;
    struct buffer *buf;
};

#include "buffer_chunks_tree.h"
#include <structure/SAvl_impl.h>

static void instance_assert (struct instance *inst);
static int instance_append (struct instance *inst, NCDValRef string);
static void instance_consume (struct instance *inst, size_t amount);
static struct buffer * buffer_init (struct instance *inst, NCDModuleInst *i);
static void buffer_free (struct buffer *buf);
static void buffer_detach (struct buffer *buf);
static struct chunk * buffer_get_existing_chunk (struct buffer *buf, size_t offset);
static struct chunk * chunk_init (struct instance *inst, size_t length);
static void chunk_unref (struct chunk *c);
static void chunk_assert (struct chunk *c);
static struct reference * reference_init (struct instance *inst, size_t offset, size_t length, NCDValComposedStringResource *out_resource);
static void reference_ref_target_func_release (BRefTarget *ref_target);
static void reference_assert (struct reference *ref);
static void reference_resource_func_getptr (void *user, size_t offset, const char **out_data, size_t *out_length);

static void instance_assert (struct instance *inst)
{
    ASSERT(inst->buf->inst == inst)
}

static int instance_append (struct instance *inst, NCDValRef string)
{
    instance_assert(inst);
    ASSERT(NCDVal_IsString(string))
    
    size_t length = NCDVal_StringLength(string);
    
    // if string is empty do nothing, we can't make an empty chunk
    if (length == 0) {
        return 1;
    }
    
    // init chunk
    struct chunk *c = chunk_init(inst, length);
    if (!c) {
        return 0;
    }
    
    // copy data to chunk
    NCDVal_StringCopyOut(string, 0, length, c->data);
    
    return 1;
}

static void instance_consume (struct instance *inst, size_t amount)
{
    instance_assert(inst);
    ASSERT(amount <= inst->total_length - inst->offset)
    
    // nothing do to if amount is zero
    if (amount == 0) {
        return;
    }
    
    // find chunk where the byte in the buffer resides
    struct chunk *c = buffer_get_existing_chunk(inst->buf, inst->offset);
    
    // increment buffer offset
    inst->offset += amount;
    
    // unreference chunks which no longer contain buffer contents
    while (c && c->offset + c->length <= inst->offset) {
        struct chunk *next_c = ChunksTree_GetNext(&inst->buf->chunks_tree, 0, c);
        chunk_unref(c);
        c = next_c;
    }
}

static struct buffer * buffer_init (struct instance *inst, NCDModuleInst *i)
{
    ASSERT(inst)
    
    // allocate structure
    struct buffer *buf = BAlloc(sizeof(*buf));
    if (!buf) {
        ModuleLog(i, BLOG_ERROR, "BAlloc failed");
        return NULL;
    }
    
    // set instance pointer
    buf->inst = inst;
    
    // init chunks tree
    ChunksTree_Init(&buf->chunks_tree);
    
    // set refcnt to 0 (number of reference objects)
    buf->refcnt = 0;
    
    return buf;
}

static void buffer_free (struct buffer *buf)
{
    ASSERT(!buf->inst)
    ASSERT(ChunksTree_IsEmpty(&buf->chunks_tree))
    ASSERT(buf->refcnt == 0)
    
    // free structure
    BFree(buf);
}

static void buffer_detach (struct buffer *buf)
{
    ASSERT(buf->inst)
    struct instance *inst = buf->inst;
    
    // consume entire buffer to free any chunks that aren't referenced
    instance_consume(inst, inst->total_length - inst->offset);
    
    // clear instance pointer
    buf->inst = NULL;
    
    // free buffer if there are no more chunks
    if (ChunksTree_IsEmpty(&buf->chunks_tree)) {
        buffer_free(buf);
    }
}

static struct chunk * buffer_get_existing_chunk (struct buffer *buf, size_t offset)
{
    struct chunk *c = ChunksTree_GetLastLesserEqual(&buf->chunks_tree, 0, offset);
    
    ASSERT(c)
    chunk_assert(c);
    ASSERT(offset >= c->offset)
    ASSERT(offset < c->offset + c->length)
    
    return c;
}

static struct chunk * chunk_init (struct instance *inst, size_t length)
{
    instance_assert(inst);
    ASSERT(length > 0)
    struct buffer *buf = inst->buf;
    
    // make sure length is not too large
    if (length >= SIZE_MAX - inst->total_length) {
        ModuleLog(inst->i, BLOG_ERROR, "length overflow");
        return NULL;
    }
    
    // allocate structure
    bsize_t size = bsize_add(bsize_fromsize(sizeof(struct chunk)), bsize_fromsize(length));
    struct chunk *c = BAllocSize(size);
    if (!c) {
        ModuleLog(inst->i, BLOG_ERROR, "BAllocSize failed");
        return NULL;
    }
    
    // set some members
    c->buf = buf;
    c->offset = inst->total_length;
    c->length = length;
    
    // insert into chunks tree
    int res = ChunksTree_Insert(&buf->chunks_tree, 0, c, NULL);
    B_ASSERT_USE(res)
    
    // set reference count to 1 (referenced by buffer contents)
    c->refcnt = 1;
    
    // increment buffer length
    inst->total_length += length;
    
    chunk_assert(c);
    return c;
}

static void chunk_unref (struct chunk *c)
{
    chunk_assert(c);
    
    // decrement reference count
    c->refcnt--;
    
    // if reference count is not yet zero, do nothing else
    if (c->refcnt > 0) {
        return;
    }
    
    // remove from chunks tree
    ChunksTree_Remove(&c->buf->chunks_tree, 0, c);
    
    // free structure
    BFree(c);
}

static void chunk_assert (struct chunk *c)
{
    ASSERT(c->buf)
    ASSERT(c->length > 0)
    ASSERT(!c->buf->inst || c->offset <= c->buf->inst->total_length)
    ASSERT(!c->buf->inst || c->length <= c->buf->inst->total_length - c->offset)
    ASSERT(c->refcnt > 0)
}

static struct reference * reference_init (struct instance *inst, size_t offset, size_t length, NCDValComposedStringResource *out_resource)
{
    instance_assert(inst);
    struct buffer *buf = inst->buf;
    ASSERT(offset >= inst->offset)
    ASSERT(offset <= inst->total_length)
    ASSERT(length <= inst->total_length - offset)
    ASSERT(length > 0)
    ASSERT(out_resource)
    
    // check buffer reference count. This ensures we can always increment the
    // chunk reference counts, below. We use (INT_MAX - 1) here because the buffer
    // itself can also own references to chunks.
    if (buf->refcnt == INT_MAX - 1) {
        ModuleLog(inst->i, BLOG_ERROR, "too many references");
        return NULL;
    }
    
    // allocate structure
    struct reference *ref = BAlloc(sizeof(*ref));
    if (!ref) {
        ModuleLog(inst->i, BLOG_ERROR, "BAlloc failed");
        return NULL;
    }
    
    // find chunk where the first byte of the interval resides
    struct chunk *c = buffer_get_existing_chunk(buf, offset);
    
    // set some members
    ref->first_chunk = c;
    ref->first_offset = offset - c->offset;
    ref->length = length;
    
    // increment buffer reference count
    buf->refcnt++;
    
    // reference chunks
    do {
        struct chunk *next_c = ChunksTree_GetNext(&buf->chunks_tree, 0, c);
        ASSERT(c->refcnt < INT_MAX)
        c->refcnt++;
        c = next_c;
    } while (c && c->offset < offset + length);
    
    // init reference target
    BRefTarget_Init(&ref->ref_target, reference_ref_target_func_release);
    
    // write resource
    out_resource->func_getptr = reference_resource_func_getptr;
    out_resource->user = ref;
    out_resource->ref_target = &ref->ref_target;
    
    reference_assert(ref);
    return ref;
}

static void reference_ref_target_func_release (BRefTarget *ref_target)
{
    struct reference *ref = UPPER_OBJECT(ref_target, struct reference, ref_target);
    reference_assert(ref);
    struct buffer *buf = ref->first_chunk->buf;
    
    // compute offset
    size_t offset = ref->first_chunk->offset + ref->first_offset;
    
    // unreference chunks
    struct chunk *c = ref->first_chunk;
    do {
        struct chunk *next_c = ChunksTree_GetNext(&buf->chunks_tree, 0, c);
        chunk_unref(c);
        c = next_c;
    } while (c && c->offset < offset + ref->length);
    
    // decrement buffer reference count
    ASSERT(buf->refcnt > 0)
    buf->refcnt--;
    
    // free structure
    BFree(ref);
    
    // if the instance has died and there are no more chunks, free buffer
    if (!buf->inst && ChunksTree_IsEmpty(&buf->chunks_tree)) {
        buffer_free(buf);
    }
}

static void reference_assert (struct reference *ref)
{
    ASSERT(ref->first_chunk)
    ASSERT(ref->first_offset < ref->first_chunk->length)
    ASSERT(ref->length > 0)
    chunk_assert(ref->first_chunk);
}

static void reference_resource_func_getptr (void *user, size_t offset, const char **out_data, size_t *out_length)
{
    struct reference *ref = user;
    reference_assert(ref);
    ASSERT(offset < ref->length)
    ASSERT(out_data)
    ASSERT(out_length)
    
    // compute absolute offset of request
    size_t abs_offset = ref->first_chunk->offset + ref->first_offset + offset;
    
    // find chunk where the byte at the requested offset resides
    struct chunk *c = buffer_get_existing_chunk(ref->first_chunk->buf, abs_offset);
    
    // compute offset of this byte within the chunk
    size_t chunk_offset = abs_offset - c->offset;
    
    // return the data from this byte to the end of the chunk
    *out_data = c->data + chunk_offset;
    *out_length = c->length - chunk_offset;
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct instance *o = vo;
    o->i = i;
    
    // pass instance pointer to methods
    NCDModuleInst_Backend_PassMemToMethods(i);
    
    // read arguments
    NCDValRef data_arg = NCDVal_NewInvalid();
    if (!NCDVal_ListRead(params->args, 0) &&
        !NCDVal_ListRead(params->args, 1, &data_arg)
    ) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsInvalid(data_arg) && !NCDVal_IsString(data_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // set offset and total length
    o->offset = 0;
    o->total_length = 0;
    
    // allocate buffer
    o->buf = buffer_init(o, i);
    if (!o->buf) {
        goto fail0;
    }
    
    // append initial data
    if (!NCDVal_IsInvalid(data_arg)) {
        if (!instance_append(o, data_arg)) {
            goto fail1;
        }
    }
    
    // signal up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail1:
    o->buf->inst = NULL;
    buffer_free(o->buf);
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    instance_assert(o);
    
    // detach buffer from instance
    buffer_detach(o->buf);
    
    // die
    NCDModuleInst_Backend_Dead(o->i);
}

static int func_getvar (void *vo, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out)
{
    struct instance *o = vo;
    instance_assert(o);
    
    if (name == NCD_STRING_EMPTY) {
        if (o->total_length - o->offset == 0) {
            *out = NCDVal_NewStringUninitialized(mem, 0);
        } else {
            NCDValComposedStringResource resource;
            struct reference *ref = reference_init(o, o->offset, o->total_length - o->offset, &resource);
            if (!ref) {
                goto fail;
            }
            *out = NCDVal_NewComposedString(mem, resource, 0, ref->length);
            BRefTarget_Deref(resource.ref_target);
        }
        return 1;
    }
    
    if (name == NCD_STRING_LENGTH) {
        *out = ncd_make_uintmax(mem, o->total_length - o->offset);
        return 1;
    }
    
    return 0;
    
fail:
    *out = NCDVal_NewInvalid();
    return 1;
}

static void append_func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // read arguments
    NCDValRef data_arg;
    if (!NCDVal_ListRead(params->args, 1, &data_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsString(data_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // get instance
    struct instance *inst = params->method_user;
    
    // append
    if (!instance_append(inst, data_arg)) {
        ModuleLog(i, BLOG_ERROR, "instance_append failed");
        goto fail0;
    }
    
    // go up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static void consume_func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // read arguments
    NCDValRef amount_arg;
    if (!NCDVal_ListRead(params->args, 1, &amount_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    if (!NCDVal_IsString(amount_arg)) {
        ModuleLog(i, BLOG_ERROR, "wrong type");
        goto fail0;
    }
    
    // parse amount
    uintmax_t amount;
    if (!ncd_read_uintmax(amount_arg, &amount)) {
        ModuleLog(i, BLOG_ERROR, "wrong amount");
        goto fail0;
    }
    
    // get instance
    struct instance *inst = params->method_user;
    
    // check amount
    if (amount > inst->total_length - inst->offset) {
        ModuleLog(i, BLOG_ERROR, "amount is more than buffer length");
        goto fail0;
    }
    
    // consume
    instance_consume(inst, amount);
    
    // go up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static struct NCDModule modules[] = {
    {
        .type = "buffer",
        .func_new2 = func_new,
        .func_die = func_die,
        .func_getvar2 = func_getvar,
        .alloc_size = sizeof(struct instance),
        .flags = NCDMODULE_FLAG_ACCEPT_NON_CONTINUOUS_STRINGS
    }, {
        .type = "buffer::append",
        .func_new2 = append_func_new,
        .flags = NCDMODULE_FLAG_ACCEPT_NON_CONTINUOUS_STRINGS
    }, {
        .type = "buffer::consume",
        .func_new2 = consume_func_new,
        .flags = NCDMODULE_FLAG_ACCEPT_NON_CONTINUOUS_STRINGS
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_buffer = {
    .modules = modules
};

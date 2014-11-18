/**
 * @file SLinkedList_impl.h
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

#include "SLinkedList_header.h"

static SLinkedListEntry * SLinkedListNext (SLinkedListEntry *entry)
{
    ASSERT(entry)
    
    return SLinkedList_next(entry);
}

static SLinkedListEntry * SLinkedListPrev (SLinkedListEntry *entry)
{
    ASSERT(entry)
    
    return SLinkedList_prev(entry);
}

static void SLinkedList_Init (SLinkedList *o)
{
    o->first = NULL;
#if SLINKEDLIST_PARAM_FEATURE_LAST
    o->last = NULL;
#endif
}

static void SLinkedList_Prepend (SLinkedList *o, SLinkedListEntry *entry)
{
    ASSERT(entry)

    SLinkedList_prev(entry) = NULL;
    SLinkedList_next(entry) = o->first;
    if (o->first) {
        SLinkedList_prev(o->first) = entry;
    } else {
#if SLINKEDLIST_PARAM_FEATURE_LAST
        o->last = entry;
#endif
    }
    o->first = entry;
}

#if SLINKEDLIST_PARAM_FEATURE_LAST
static void SLinkedList_Append (SLinkedList *o, SLinkedListEntry *entry)
{
    ASSERT(entry)

    SLinkedList_next(entry) = NULL;
    SLinkedList_prev(entry) = o->last;
    if (o->last) {
        SLinkedList_next(o->last) = entry;
    } else {
        o->first = entry;
    }
    o->last = entry;
}
#endif

static void SLinkedList_InsertBefore (SLinkedList *o, SLinkedListEntry *entry, SLinkedListEntry *before_entry)
{
    ASSERT(entry)
    ASSERT(before_entry)
    
    SLinkedList_prev(entry) = SLinkedList_prev(before_entry);
    SLinkedList_next(entry) = before_entry;
    if (SLinkedList_prev(before_entry)) {
        SLinkedList_next(SLinkedList_prev(before_entry)) = entry;
    } else {
        o->first = entry;
    }
    SLinkedList_prev(before_entry) = entry;
}

static void SLinkedList_InsertAfter (SLinkedList *o, SLinkedListEntry *entry, SLinkedListEntry *after_entry)
{
    ASSERT(entry)
    ASSERT(after_entry)
    
    SLinkedList_next(entry) = SLinkedList_next(after_entry);
    SLinkedList_prev(entry) = after_entry;
    if (SLinkedList_next(after_entry)) {
        SLinkedList_prev(SLinkedList_next(after_entry)) = entry;
    } else {
#if SLINKEDLIST_PARAM_FEATURE_LAST
        o->last = entry;
#endif
    }
    SLinkedList_next(after_entry) = entry;
}

static void SLinkedList_Remove (SLinkedList *o, SLinkedListEntry *entry)
{
    if (SLinkedList_prev(entry)) {
        SLinkedList_next(SLinkedList_prev(entry)) = SLinkedList_next(entry);
    } else {
        o->first = SLinkedList_next(entry);
    }
    if (SLinkedList_next(entry)) {
        SLinkedList_prev(SLinkedList_next(entry)) = SLinkedList_prev(entry);
    } else {
#if SLINKEDLIST_PARAM_FEATURE_LAST
        o->last = SLinkedList_prev(entry);
#endif
    }
}

static SLinkedListEntry * SLinkedList_First (const SLinkedList *o)
{
    return o->first;
}

#if SLINKEDLIST_PARAM_FEATURE_LAST
static SLinkedListEntry * SLinkedList_Last (const SLinkedList *o)
{
    return o->last;
}
#endif

static int SLinkedList_IsEmpty (const SLinkedList *o)
{
    return !o->first;
}

#include "SLinkedList_footer.h"

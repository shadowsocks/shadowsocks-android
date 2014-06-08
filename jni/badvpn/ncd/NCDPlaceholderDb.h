/**
 * @file NCDPlaceholderDb.h
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

#ifndef BADVPN_NCDPLACEHOLDERDB_H
#define BADVPN_NCDPLACEHOLDERDB_H

#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDStringIndex.h>

struct NCDPlaceholderDb__entry {
    NCD_string_id_t *varnames;
    size_t num_names;
};

/**
 * Associates variable placeholder numbers to variable names.
 * This is populated by {@link NCDInterpProcess_Init} when converting the {@link NCDValue}
 * objects in the AST to compact representations in {@link NCDValMem}. Variables are
 * replaced with placeholder identifiers (integers), which this object associates
 * with their names.
 * During interpretation, when a statement is being initialized, the compact form held
 * by {@link NCDInterpProcess} is byte-copied, and placeholders are replaced with the
 * values of corresponding variables using {@link NCDVal_ReplacePlaceholders}.
 */
typedef struct {
    struct NCDPlaceholderDb__entry *arr;
    size_t count;
    size_t capacity;
    NCDStringIndex *string_index;
} NCDPlaceholderDb;

/**
 * Initializes the placeholder database.
 * Returns 1 on success, and 0 on failure.
 */
int NCDPlaceholderDb_Init (NCDPlaceholderDb *o, NCDStringIndex *string_index) WARN_UNUSED;

/**
 * Frees the placeholder database.
 */
void NCDPlaceholderDb_Free (NCDPlaceholderDb *o);

/**
 * Adds a variable to the database.
 * 
 * @param varname name of the variable (text, including dots). Must not be NULL.
 * @param out_plid on success, the placeholder identifier will be returned here. Must
 *                 not be NULL.
 * @return 1 on success, 0 on failure
 */
int NCDPlaceholderDb_AddVariable (NCDPlaceholderDb *o, const char *varname, int *out_plid) WARN_UNUSED;

/**
 * Retrieves the name of the variable associated with a placeholder identifier.
 * 
 * @param plid placeholder identifier; must have been previously provided by
 *             {@link NCDPlaceholderDb_AddVariable}.
 * @return name of the variable, split by dots. The returned value points to
 *         an array of pointers to strings, which is terminated by a NULL
 *         pointer. These all point to internal data in the placeholder
 *         database; they must not be modified, and remain valid until the
 *         database is freed.
 *         Note that there will always be at least one string in the result.
 */
void NCDPlaceholderDb_GetVariable (NCDPlaceholderDb *o, int plid, const NCD_string_id_t **out_varnames, size_t *out_num_names);

#endif

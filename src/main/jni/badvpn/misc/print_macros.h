/**
 * @file print_macros.h
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
 * Format macros for printf() for non-standard compilers.
 */

#ifndef BADVPN_PRINT_MACROS
#define BADVPN_PRINT_MACROS

#ifdef _MSC_VER

// size_t
#define PRIsz "Iu"

// signed exact width (intN_t)
#define PRId8 "d"
#define PRIi8 "i"
#define PRId16 "d"
#define PRIi16 "i"
#define PRId32 "I32d"
#define PRIi32 "I32i"
#define PRId64 "I64d"
#define PRIi64 "I64i"

// unsigned exact width (uintN_t)
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"
#define PRIo32 "I32o"
#define PRIu32 "I32u"
#define PRIx32 "I32x"
#define PRIX32 "I32X"
#define PRIo64 "I64o"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#define PRIX64 "I64X"

// signed maximum width (intmax_t)
#define PRIdMAX "I64d"
#define PRIiMAX "I64i"

// unsigned maximum width (uintmax_t)
#define PRIoMAX "I64o"
#define PRIuMAX "I64u"
#define PRIxMAX "I64x"
#define PRIXMAX "I64X"

// signed pointer (intptr_t)
#define PRIdPTR "Id"
#define PRIiPTR "Ii"

// unsigned pointer (uintptr_t)
#define PRIoPTR "Io"
#define PRIuPTR "Iu"
#define PRIxPTR "Ix"
#define PRIXPTR "IX"

#else

#include <inttypes.h>

#define PRIsz "zu"

#endif

#endif

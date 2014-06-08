/**
 * @file write_file.h
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

#ifndef BADVPN_WRITE_FILE_H
#define BADVPN_WRITE_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <misc/debug.h>
#include <misc/cstring.h>

static int write_file (const char *file, const uint8_t *data, size_t len)
{
    FILE *f = fopen(file, "w");
    if (!f) {
        goto fail0;
    }
    
    while (len > 0) {
        size_t res = fwrite(data, 1, len, f);
        if (res == 0) {
            goto fail1;
        }
        
        ASSERT(res <= len)
        
        data += res;
        len -= res;
    }
    
    if (fclose(f) != 0) {
        return 0;
    }
    
    return 1;
    
fail1:
    fclose(f);
fail0:
    return 0;
}

static int write_file_cstring (const char *file, b_cstring cstr, size_t offset, size_t length)
{
    b_cstring_assert_range(cstr, offset, length);
    
    FILE *f = fopen(file, "w");
    if (!f) {
        goto fail0;
    }
    
    B_CSTRING_LOOP_RANGE(cstr, offset, length, pos, chunk_data, chunk_length, {
        size_t chunk_pos = 0;
        while (chunk_pos < chunk_length) {
            size_t res = fwrite(chunk_data + chunk_pos, 1, chunk_length - chunk_pos, f);
            if (res == 0) {
                goto fail1;
            }
            ASSERT(res <= chunk_length - chunk_pos)
            chunk_pos += res;
        }
    })
    
    if (fclose(f) != 0) {
        return 0;
    }
    
    return 1;
    
fail1:
    fclose(f);
fail0:
    return 0;
}

#endif

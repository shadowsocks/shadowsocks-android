/**
 * @file find_program.h
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
 * Function that finds the absolute path of a program by checking a predefined
 * list of directories.
 */

#ifndef BADVPN_FIND_PROGRAM_H
#define BADVPN_FIND_PROGRAM_H

#include <stdlib.h>
#include <unistd.h>

#include <misc/concat_strings.h>
#include <misc/debug.h>

static char * badvpn_find_program (const char *name);

static char * badvpn_find_program (const char *name)
{
    ASSERT(name)
    
    const char *dirs[] = {"/usr/sbin", "/usr/bin", "/sbin", "/bin", NULL};
    
    for (size_t i = 0; dirs[i]; i++) {
        char *path = concat_strings(3, dirs[i], "/", name);
        if (!path) {
            goto fail;
        }
        
        if (access(path, X_OK) == 0) {
            return path;
        }
        
        free(path);
    }
    
fail:
    return NULL;
}

#endif

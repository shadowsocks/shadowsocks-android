/* debug.c -  Various debugging facilities
 * Copyright (C) 2001 Thomas Moestl
 *
 * This file is part of the pdnsd package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "helpers.h"
#include "error.h"


/*
 * This is indeed very primitive (it does not track allocation failures
 * and so on). It should be expanded some time.
 */
#ifdef ALLOC_DEBUG
void *DBGcalloc(size_t n, size_t sz, char *file, int line)
{
	DEBUG_MSG("+ calloc, %s:%d\n", file, line);
	return calloc(n, sz);
}

void *DBGmalloc(size_t sz, char *file, int line)
{
	DEBUG_MSG("+ malloc, %s:%d\n", file, line);
	return malloc(sz);
}

void *DBGrealloc(void *ptr, size_t sz, char *file, int line)
{
	if (ptr == NULL && sz != 0)
		DEBUG_MSG("+ realloc, %s:%d\n", file, line);
	if (ptr != NULL && sz == 0)
		DEBUG_MSG("- realloc(0), %s:%d\n", file, line);
	return realloc(ptr, sz);
}
void DBGfree(void *ptr, char *file, int line)
{
	DEBUG_MSG("- free, %s:%d\n", file, line);
	free(ptr);
}
#endif

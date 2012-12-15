/* debug.h - Various debugging facilities
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


#ifndef DEBUG_H
#define DEBUG_H

/*
 * A hand-rolled alloc debug factility, because most available libraries have
 * problems with at least one thread implementation.
 */
#ifdef ALLOC_DEBUG
void *DBGcalloc(size_t n, size_t sz, char *file, int line);
void *DBGmalloc(size_t sz, char *file, int line);
void *DBGrealloc(void *ptr, size_t sz, char *file, int line);
void DBGfree(void *ptr, char *file, int line);

#define pdnsd_calloc(n,sz)	DBGcalloc(n,sz,__FILE__,__LINE__)
#define pdnsd_malloc(sz)	DBGmalloc(sz,__FILE__,__LINE__)
#define pdnsd_realloc(ptr,sz)	DBGrealloc(ptr,sz,__FILE__,__LINE__)
#define pdnsd_free(ptr)		DBGfree(ptr,__FILE__,__LINE__)
#else
#define pdnsd_calloc	calloc
#define pdnsd_malloc	malloc
#define pdnsd_realloc	realloc
#define pdnsd_free	free
#endif

#endif /* def DEBUG_H */

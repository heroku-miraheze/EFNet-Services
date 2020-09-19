/*
**   IRC services -- mempool.h
**   Copyright (C) 2001-2004 Chris Behrens
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 1, or (at your option)
**   any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

/*
** $Id: mempool.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <sys/types.h>

#define SI_MEMPOOL_MAGIC	0x31337abc

typedef struct _si_mempool_entry	SI_MEMPOOL_ENTRY;
typedef struct _si_mempool_pentry	SI_MEMPOOL_PENTRY;
typedef struct _si_mempool			SI_MEMPOOL;

struct _si_mempool_entry
{
	SI_MEMPOOL			*e_mempool;
	u_int				e_datalen;
#ifdef SIMEMPOOL_FNDEBUG
	char				*e_filename;
	u_int				e_lineno;
#endif
#ifdef SIMEMPOOL_DEBUG
	SI_MEMPOOL_ENTRY	*e_a_next;
	SI_MEMPOOL_ENTRY	*e_a_prev;
	SI_MEMPOOL_ENTRY	*e_next;
	SI_MEMPOOL_ENTRY	*e_prev;
	SI_MEMPOOL_PENTRY	*e_pe;
	u_int				e_magic;
#endif
	char				e_data[1];
};

struct _si_mempool_pentry
{
	u_int				pe_magic;
};

struct _si_mempool
{
	char				*mp_name;
	SI_MEMPOOL_ENTRY	*mp_entries;
#define SI_MEMPOOL_FLAGS_BOUNDSCHECK	0x001
	u_int				mp_flags;
	int					mp_num_entries;
	int					mp_tot_used;
};


int simempool_init(SI_MEMPOOL *mempool, char *name, u_int flags);
void simempool_deinit(SI_MEMPOOL *mempool);
#ifdef SIMEMPOOL_FNDEBUG
void *simempool_alloc(SI_MEMPOOL *mempool, u_int size, char *filename, u_int lineno);
void *simempool_calloc(SI_MEMPOOL *mempool, u_int num, u_int size, char *filename, u_int lineno);
char *simempool_strdup(SI_MEMPOOL *mempool, char *string, char *filename, u_int lineno);
void *simempool_realloc(SI_MEMPOOL *mempool, void *addr, u_int size, char *filename, u_int lineno);

#ifndef __MEMPOOL_C__
#define simempool_alloc(__a, __b)   simempool_alloc(__a, __b, __FILE__, __LINE__)
#define simempool_realloc(__a, __b, __c)   simempool_realloc(__a, __b, __c, __FILE__, __LINE__)
#define simempool_calloc(__a, __b, __c) simempool_calloc(__a, __b, __c, __FILE__, __LINE__)
#define simempool_strdup(__a, __b)  simempool_strdup(__a, __b, __FILE__, __LINE__)
#endif



#else
void *simempool_alloc(SI_MEMPOOL *mempool, u_int size);
void *simempool_calloc(SI_MEMPOOL *mempool, u_int num, u_int size);
char *simempool_strdup(SI_MEMPOOL *mempool, char *string);
void *simempool_realloc(SI_MEMPOOL *mempool, void *addr, u_int size);
#endif
void simempool_free(void *arg);
void simempool_check(SI_MEMPOOL *mempool);
void simempool_dump(SI_MEMPOOL *mempool);

#endif

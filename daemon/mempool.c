/*
**   IRC services -- mempool.c
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


#define __MEMPOOL_C__
#include "services.h"

RCSID("$Id: mempool.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");


#ifdef SIMEMPOOL_DEBUG
static SI_MEMPOOL_ENTRY		*SIMPEntries = NULL;
#endif
static int					SIMPNumEntries = 0;
static int					SIMPTotUsed = 0;

#ifdef SIMEMPOOL_DEBUG
static SI_MEMPOOL_PENTRY    SIMPMagicPEntry = { SI_MEMPOOL_MAGIC };
#endif


#ifdef SIMEMPOOL_DEBUG
static void _simempool_check(SI_MEMPOOL_ENTRY *e);

static void _simempool_check(SI_MEMPOOL_ENTRY *e)
{
	SI_MEMPOOL_PENTRY	pe;

	if (!(e->e_mempool->mp_flags & SI_MEMPOOL_FLAGS_BOUNDSCHECK))
		return;

    if (e->e_pe == NULL)
    {
		fprintf(stderr, "xomempool_check(%x): data integrity error: e_pe == NULL, maybe freed already\n", (u_int)e->e_data);
		fprintf(stderr, "error occurred in module %s\n", e->e_mempool->mp_name);
#ifdef SIMEMPOOL_FNDEBUG
		fprintf(stderr, "data was allocated on line %d of %s\n",
			e->e_lineno, e->e_filename);
#endif
        abort();
        exit(0);
    }

	if (e->e_magic != SI_MEMPOOL_MAGIC)
	{
		fprintf(stderr, "simempool_free(%x): data integrity error: e_magic mismatch: %x != %x\n", (u_int)e->e_data, e->e_magic, SI_MEMPOOL_MAGIC);
		fprintf(stderr, "error occurred in module %s\n", e->e_mempool->mp_name);
#ifdef SIMEMPOOL_FNDEBUG
		fprintf(stderr, "data was allocated on line %d of %s\n",
			e->e_lineno, e->e_filename);
#endif
		abort();
		exit(0);
	}

	memcpy(&pe, e->e_pe, sizeof(SI_MEMPOOL_PENTRY));

	if (pe.pe_magic != SI_MEMPOOL_MAGIC)
	{
		fprintf(stderr, "simempool_free(%x): data integrity error: pe_magic mismatch: %x != %x\n", (u_int)e->e_data, pe.pe_magic, SI_MEMPOOL_MAGIC);
		fprintf(stderr, "error occurred in module %s\n", e->e_mempool->mp_name);
#ifdef SIMEMPOOL_FNDEBUG
		fprintf(stderr, "data was allocated on line %d of %s\n",
			e->e_lineno, e->e_filename);
#endif
		abort();
		exit(0);
	}
}
#endif

int simempool_init(SI_MEMPOOL *mempool, char *name, u_int flags)
{
	if (!mempool)
		return EINVAL;

	memset(mempool, 0, sizeof(SI_MEMPOOL));
	mempool->mp_name = name;
	mempool->mp_flags = flags;

	return 0;
}

void simempool_deinit(SI_MEMPOOL *mempool)
{
	if (!mempool)
		return;
#ifdef SIMEMPOOL_DEBUG
	while(mempool->mp_entries)
		simempool_free(mempool->mp_entries->e_data);
#endif
}

#ifdef SIMEMPOOL_FNDEBUG
void *simempool_alloc(SI_MEMPOOL *mempool, u_int size, char *filename, u_int lineno)
#else
void *simempool_alloc(SI_MEMPOOL *mempool, u_int size)
#endif
{
	SI_MEMPOOL_ENTRY	*e;

	if (!size)
		return NULL;

#ifdef SIMEMPOOL_DEBUG
	if (mempool->mp_flags & SI_MEMPOOL_FLAGS_BOUNDSCHECK)
	{													
	 
		e = (SI_MEMPOOL_ENTRY *)malloc(sizeof(SI_MEMPOOL_ENTRY)+sizeof(SI_MEMPOOL_PENTRY)+size);
		if (!e) 
			return NULL;
		e->e_magic = SI_MEMPOOL_MAGIC;
		e->e_pe = (SI_MEMPOOL_PENTRY *)(e->e_data + size);
		memcpy(e->e_pe, &SIMPMagicPEntry, sizeof(SI_MEMPOOL_PENTRY));
	}																
#endif
	else
	{   
		e = (SI_MEMPOOL_ENTRY *)malloc(sizeof(SI_MEMPOOL_ENTRY)+size);
		if (!e)													   
			return NULL;											  
#ifdef SIMEMPOOL_DEBUG
		e->e_pe = (SI_MEMPOOL_PENTRY *)0x1;
#endif
	}									  

	e->e_datalen = size;
	e->e_mempool = mempool;

#ifdef SIMEMPOOL_FNDEBUG
	e->e_filename = filename;
	e->e_lineno = lineno;
#endif
#ifdef SIMEMPOOL_DEBUG
	si_ll_add_to_list(mempool->mp_entries, e, e);
	si_ll_add_to_list(SIMPEntries, e, e_a);
#endif

	mempool->mp_num_entries++;
	mempool->mp_tot_used += size;
	SIMPNumEntries++;
	SIMPTotUsed += size;

	return e->e_data;
}

#ifdef SIMEMPOOL_FNDEBUG
void *simempool_calloc(SI_MEMPOOL *mempool, u_int num, u_int size, char *filename, u_int lineno)
#else
void *simempool_calloc(SI_MEMPOOL *mempool, u_int num, u_int size)
#endif
{
#ifdef SIMEMPOOL_FNDEBUG
	char	*ptr = simempool_alloc(mempool, num*size, filename, lineno);
#else
	char	*ptr = simempool_alloc(mempool, num*size);
#endif

	if (!ptr)
		return NULL;
	memset(ptr, 0, num*size);
	return ptr;
}

#ifdef SIMEMPOOL_FNDEBUG
char *simempool_strdup(SI_MEMPOOL *mempool, char *string, char *filename, u_int lineno)
#else
char *simempool_strdup(SI_MEMPOOL *mempool, char *string)
#endif
{
	u_int	len = strlen(string);
	char	*ptr;

#ifdef SIMEMPOOL_FNDEBUG
	ptr = simempool_alloc(mempool, len+1, filename, lineno);
#else
	ptr = simempool_alloc(mempool, len+1);
#endif
	if (!ptr)
		return NULL;
	return strcpy(ptr, string);
}


#ifdef SIMEMPOOL_FNDEBUG
void *simempool_realloc(SI_MEMPOOL *mempool, void *addr, u_int size, char *filename, u_int lineno)
#else
void *simempool_realloc(SI_MEMPOOL *mempool, void *addr, u_int size)
#endif
{
	if (!addr)
	{
#ifdef SIMEMPOOL_FNDEBUG
		return simempool_alloc(mempool, size, filename, lineno);
#else
		return simempool_alloc(mempool, size);
#endif
	}

	if (!size)
	{
		simempool_free(addr);
		return NULL;
	}

	simempool_free(addr);

#ifdef SIMEMPOOL_FNDEBUG
	return simempool_alloc(mempool, size, filename, lineno);
#else
	return simempool_alloc(mempool, size);
#endif
}

void simempool_free(void *arg)
{
	SI_MEMPOOL_ENTRY	*e;

	e = (SI_MEMPOOL_ENTRY *)(((char *)arg) - offsetof(SI_MEMPOOL_ENTRY, e_data));

#ifdef SIMEMPOOL_DEBUG
	_simempool_check(e);

	si_ll_del_from_list(e->e_mempool->mp_entries, e, e);
#endif

	e->e_mempool->mp_tot_used -= e->e_datalen;

#ifdef NDEBUG
	--e->e_mempool->mp_num_entries;
#else
	if (!--e->e_mempool->mp_num_entries)
	{
		assert(!e->e_mempool->mp_tot_used);
	}
	else
	{
		assert(e->e_mempool->mp_num_entries > 0);
		assert(e->e_mempool->mp_tot_used > 0);
	}
#endif


#ifdef SIMEMPOOL_DEBUG
	si_ll_del_from_list(SIMPEntries, e, e_a);
#endif

	SIMPTotUsed -= e->e_datalen;
#ifdef NDEBUG
	--SIMPNumEntries;
#else
	if (!--SIMPNumEntries)
	{
		assert(!SIMPTotUsed);
	}
	else
	{
		assert(SIMPNumEntries > 0);
		assert(SIMPTotUsed > 0);
#endif
	}

#ifdef SIMEMPOOL_DEBUG
	e->e_pe = NULL;
#endif

	free(e);
}

void simempool_check(SI_MEMPOOL *mempool)
{
#ifdef SIMEMPOOL_DEBUG
	SI_MEMPOOL_ENTRY	*e;

	if (mempool)
	{
		for(e=mempool->mp_entries;e;e=e->e_next)
		{
			_simempool_check(e);
		}
	}	
	else
	{   
		for(e=SIMPEntries;e;e=e->e_a_next)
		{
			_simempool_check(e);
		}
	}	
#endif
}

void simempool_dump(SI_MEMPOOL *mempool)
{
#ifdef SIMEMPOOL_DEBUG
	SI_MEMPOOL_ENTRY	*e;
	int					fd;
	char				buf[2048];

	buf[sizeof(buf)-1] = '\0';

	fd = open("mempool.out", O_CREAT|O_TRUNC|O_WRONLY, 0660);
	if (fd < 0)
		return;

	if (mempool)
	{
		for(e=mempool->mp_entries;e;e=e->e_next)
		{
#ifdef SIMEMPOOL_FNDEBUG
			snprintf(buf, sizeof(buf)-1, "%s:%i: Mempool: %s, %d byte(s)\n",
				e->e_filename, e->e_lineno,
				e->e_mempool->mp_name, e->e_datalen);
#else
			snprintf(buf, sizeof(buf)-1, "Mempool: %s, %d byte(s)\n",
				e->e_mempool->mp_name, e->e_datalen);
#endif
			write(fd, buf, strlen(buf));
		}
	}	
	else
	{   
		for(e=SIMPEntries;e;e=e->e_a_next)
		{
#ifdef SIMEMPOOL_FNDEBUG
			snprintf(buf, sizeof(buf)-1, "%s:%i: Mempool: %s, %d byte(s)\n",
				e->e_filename, e->e_lineno,
				e->e_mempool->mp_name, e->e_datalen);
#else
			snprintf(buf, sizeof(buf)-1, "Mempool: %s, %d byte(s)\n",
				e->e_mempool->mp_name, e->e_datalen);
#endif
			write(fd, buf, strlen(buf));
		}
	}	
	close(fd);
#endif
}

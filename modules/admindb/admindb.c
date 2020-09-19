/*
**   IRC services admindb module -- admindb.c
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

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include "admindb.h"
#include "services.h"

RCSID("$Id: admindb.c,v 1.2 2004/03/30 22:23:44 cbehrens Exp $");

static u_int	_hashtab[256];
static u_int	_hash_inited = 0;

static void _sihash_init(void);
static u_int _sihash_compute(char *nname);
static int _siadmindb_account_find(SI_ADMINDB *adb, u_int *bucket, char *name, u_int hashv, SI_ADMIN_ACCOUNT **arecptr);
static int _siadmindb_userhost_find(SI_ADMINDB *adb, u_int *bucket, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr);
static int _siadmindb_check(SI_ADMINDB *adb);
static void _siadmindb_del_space(SI_ADMINDB *adb, u_int pos, u_int size);
static int _siadmindb_get_space(SI_ADMINDB *adb, u_int size, u_int *posptr, u_int *sizeptr);
static void _siadmindb_account_remove(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec);

static void _sihash_init(void)
{
	int i;
	for (i = 0; i < 256; i++)
		_hashtab[i] = tolower((char)i) * 109;
}

static u_int _sihash_compute(char *nname)
{
    u_char          *name = (u_char *)nname;
    u_int           hash = 1;

    for(;*name;name++)
    {
        hash <<= 1;
        hash += _hashtab[(u_int)*name];
    }
    return hash;
}

static int _siadmindb_account_find(SI_ADMINDB *adb, u_int *bucket, char *name, u_int hashv, SI_ADMIN_ACCOUNT **arecptr)
{
	u_int				pos;
	SI_ADMIN_ACCOUNT	*arec;

	assert(adb && name);

	for(pos=*bucket;pos != (u_int)-1;pos=arec->aa_h_next)
	{
		arec = SI_ADMIN_ACCOUNT_PTR(adb, pos);
		if ((arec->aa_hashv == hashv) &&
				!strcasecmp(name, arec->aa_name))
		{
			if (arecptr)
				*arecptr = arec;
			return 0;
		}
	}
	return ENOENT;
}

static int _siadmindb_userhost_find(SI_ADMINDB *adb, u_int *bucket, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr)
{
	u_int					pos;
	SI_ADMIN_USERHOST		*uhrec;

	assert(adb && userhost);

	for(pos=*bucket;pos != (u_int)-1;pos=uhrec->auh_a_next)
	{
		uhrec = SI_ADMIN_USERHOST_PTR(adb, pos);
		if (!ircd_match(uhrec->auh_userhost, userhost) &&
				!ircd_match(SI_ADMIN_SERVER_PTR(adb, uhrec->auh_server_offset), server))
		{
			if (uhrecptr)
				*uhrecptr = uhrec;
			return 0;
		}
	}
	return ENOENT;
}

static int _siadmindb_check(SI_ADMINDB *adb)
{
	if (adb->adb_info->ai_size != adb->adb_data_size)
	{
		munmap(adb->adb_data, adb->adb_data_size);
		adb->adb_data = (char *)mmap(NULL,
				adb->adb_info->ai_size,
				(adb->adb_flags & SI_ADMINDB_FLAGS_RDONLY) ? 
					PROT_READ : PROT_READ|PROT_WRITE,
				MAP_SHARED, adb->adb_fd, 0);
		if (adb->adb_data == (char *)-1)
			return errno;
		adb->adb_data_size = adb->adb_info->ai_size;
	}
	return 0;
}

static void _siadmindb_del_space(SI_ADMINDB *adb, u_int pos, u_int size)
{
	SI_ADMIN_FREELIST		*fl;

	fl = SI_ADMIN_FREELIST_PTR(adb, pos);
	fl->afl_size = size;
	fl->afl_prev = -1;
	if ((fl->afl_next = adb->adb_info->ai_freelist) != -1)
		SI_ADMIN_FREELIST_PTR(adb, fl->afl_next)->afl_prev = pos;
	adb->adb_info->ai_freelist = pos;
	adb->adb_info->ai_freelist_size += size;
}

static int _siadmindb_get_space(SI_ADMINDB *adb, u_int size, u_int *posptr, u_int *sizeptr)
{
	int						err;
	u_int					pos;
	SI_ADMIN_FREELIST		*fl;

	assert(adb && posptr && sizeptr);
	size = (size + 7) & ~7;
	*sizeptr = size;
	for(pos=adb->adb_info->ai_freelist;pos!=-1;pos=fl->afl_next)
	{
		fl = SI_ADMIN_FREELIST_PTR(adb, pos);
		if ((fl->afl_size == size) ||
				((fl->afl_size > size) &&
				(fl->afl_size - size >= sizeof(SI_ADMIN_FREELIST))))
		{
			u_int		diff = fl->afl_size - size;

			if (fl->afl_prev != (u_int)-1)
				SI_ADMIN_FREELIST_PTR(adb, fl->afl_prev)->afl_next = fl->afl_next;
			else
				adb->adb_info->ai_freelist = fl->afl_next;
			if (fl->afl_next != (u_int)-1)
				SI_ADMIN_FREELIST_PTR(adb, fl->afl_next)->afl_prev = fl->afl_prev;

			adb->adb_info->ai_freelist_size -= fl->afl_size;
			if (diff)
				_siadmindb_del_space(adb, pos+size, diff);
			*posptr = pos;
			return 0;
		}
	}

	pos = adb->adb_info->ai_next_addpos;

	if (pos + size > adb->adb_info->ai_size)
	{
		err = ftruncate(adb->adb_fd,
					pos + size + 128*1024);
		if (err)
		{
			err = ftruncate(adb->adb_fd, pos + size);
			if (err)
			{
				return errno;
			}
			adb->adb_info->ai_size = pos + size;
		}
		else
		{
			adb->adb_info->ai_size = pos + size + 128*1024;
		}
		err = _siadmindb_check(adb);
		if (err)
			return err;
	}

	adb->adb_info->ai_next_addpos += size;

	*posptr = pos;
	return 0;
}

static void _siadmindb_account_remove(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec)
{
	u_int	*bucket;

	assert(adb && arec);
	assert(!arec->aa_userhosts_num);

	if (arec->aa_a_prev != (u_int)-1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_a_prev)->aa_a_next = arec->aa_a_next;
	else
		adb->adb_info->ai_accounts = arec->aa_a_next;
	if (arec->aa_a_next != (u_int)-1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_a_next)->aa_a_prev = arec->aa_a_prev;
	adb->adb_info->ai_num_accounts--;

	bucket = SI_ADMIN_HASHTABLE_PTR(adb, arec->aa_hashv % 
				adb->adb_info->ai_hashtable_size);
	if (arec->aa_h_prev != (u_int)-1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_h_prev)->aa_h_next = arec->aa_h_next;
	else
		*bucket = arec->aa_h_next;
	if (arec->aa_h_next != (u_int)-1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_h_next)->aa_h_prev = arec->aa_h_prev;

	_siadmindb_del_space(adb, ((char *)arec) - adb->adb_data, arec->aa_size);
}

void siadmindb_deinit(SI_ADMINDB *adb)
{
	if (!adb)
		return;

	if (adb->adb_data != (char *)-1)
		munmap((void *)adb->adb_data, adb->adb_data_size);
	if (adb->adb_info != (SI_ADMIN_INFO *)-1)
		munmap((void *)adb->adb_info, sizeof(SI_ADMIN_INFO));
	if (adb->adb_fd >= 0)
		close(adb->adb_fd);
#ifdef _REENTRANT
	pthread_mutex_destroy(&(adb->adb_lock));
#endif
	free(adb);
}

int siadmindb_create(char *filename, u_int hashtable_size)
{
	int				fd;
	int				err;
	SI_ADMINDB		*adb;
	SI_ADMIN_INFO	info;

	memset(&info, 0, sizeof(info));
	info.ai_hashtable = (sizeof(info) + 8191) & ~8191;
	info.ai_hashtable_size = hashtable_size;
	info.ai_next_addpos = (info.ai_hashtable + (sizeof(u_int) * hashtable_size) + 7) & ~7;
	info.ai_size = info.ai_next_addpos + 128*1024;
	info.ai_freelist = -1;
	info.ai_accounts = -1;

	fd = open(filename, O_CREAT|O_EXCL|O_RDWR, 0600);
	if (fd < 0)
		return errno;
	err = ftruncate(fd, info.ai_size);
	if (err)
	{
		err = errno;
		unlink(filename);
		close(fd);
		return err;
	}

	err = write(fd, &info, sizeof(info));
	if (err != sizeof(info))
	{
		if (err < 0)
			err = errno;
		else
			err = EFAULT;
		unlink(filename);
		close(fd);
		return err;
	}

	close(fd);

	err = siadmindb_init(filename, 0, &adb);
	if (err)
	{
		return err;
	}

	memset(adb->adb_data + adb->adb_info->ai_hashtable,
				-1, sizeof(u_int) * hashtable_size);

	siadmindb_deinit(adb);

	return 0;
}

int siadmindb_init(char *filename, u_int flags, SI_ADMINDB **adbptr)
{
	SI_ADMINDB		*adb;
	struct stat		sb;
	int				err;

	if (!filename || !adbptr)
		return EINVAL;

	if (!_hash_inited)
	{
		_sihash_init();
		_hash_inited = 1;
	}

	adb = (SI_ADMINDB *)calloc(1, sizeof(SI_ADMINDB));
	if (!adb)
		return ENOMEM;

#ifdef _REENTRANT
	pthread_mutex_init(&(adb->adb_lock), NULL);
#endif

	adb->adb_info = (SI_ADMIN_INFO *)-1;
	adb->adb_data = (char *)-1;
	adb->adb_flags = flags;

	if (flags & SI_ADMINDB_FLAGS_RDONLY)
		adb->adb_fd = open(filename, O_RDONLY);
	else
		adb->adb_fd = open(filename, O_RDWR);
	if (adb->adb_fd < 0)
	{
		err = errno;
		siadmindb_deinit(adb);
		return err;
	}

	err = fstat(adb->adb_fd, &sb);
	if (err)
	{
		err = errno;
		siadmindb_deinit(adb);
		return err;
	}

	if (sb.st_size < sizeof(SI_ADMIN_INFO))
	{
		siadmindb_deinit(adb);
		return EFAULT;
	}

	adb->adb_info = (SI_ADMIN_INFO *)mmap(NULL,
			sizeof(SI_ADMIN_INFO),
			(flags & SI_ADMINDB_FLAGS_RDONLY) ?
				PROT_READ : PROT_READ|PROT_WRITE,
			MAP_SHARED, adb->adb_fd, 0);
	if (adb->adb_info == (SI_ADMIN_INFO *)-1)
	{
		err = errno;
		siadmindb_deinit(adb);
		return err;
	}

	adb->adb_data_size = sb.st_size;
	adb->adb_data = (char *)mmap(NULL, sb.st_size,
				(flags & SI_ADMINDB_FLAGS_RDONLY) ? 
					PROT_READ : PROT_READ|PROT_WRITE,
				MAP_SHARED, adb->adb_fd, 0);
	if (adb->adb_data == (char *)-1)
	{
		err = errno;
		siadmindb_deinit(adb);
		return err;
	}

	*adbptr = adb;

	return 0;
}

int siadmindb_account_find(SI_ADMINDB *adb, char *name, SI_ADMIN_ACCOUNT **uhrecptr)
{
	u_int		hashv;
	u_int		*bucket;
	int			err;

	if (!adb || !name)
		return EINVAL;

	err = _siadmindb_check(adb);
	if (err)
		return err;

	hashv = _sihash_compute(name);
	bucket = SI_ADMIN_HASHTABLE_PTR(adb, hashv % 
				adb->adb_info->ai_hashtable_size);

	err = _siadmindb_account_find(adb, bucket, name, hashv, uhrecptr);
	if (err)
	{
		return err;
	}

	return 0;
}

int siadmindb_userhost_find(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr)
{
	int			err;

	err = _siadmindb_check(adb);
	if (err)
		return err;

	return _siadmindb_userhost_find(adb, &(arec->aa_userhosts),
					userhost, server, uhrecptr);
}

int siadmindb_account_create(SI_ADMINDB *adb, char *name, SI_ADMIN_ACCOUNT **arecptr)
{
	u_int				pos;
	u_int				size;
	u_int				namelen;
	SI_ADMIN_ACCOUNT	*arec;
	u_int				*bucket;
	int					err;

	if (!adb || !name)
		return EINVAL;

	err = _siadmindb_check(adb);
	if (err)
		return err;

	namelen = strlen(name);
	err = _siadmindb_get_space(adb,
				sizeof(SI_ADMIN_ACCOUNT)+namelen, &pos, &size);
	if (err)
		return err;

	arec = SI_ADMIN_ACCOUNT_PTR(adb, pos);
	arec->aa_size = size;
	arec->aa_hashv = _sihash_compute(name);

	bucket = SI_ADMIN_HASHTABLE_PTR(adb, arec->aa_hashv % 
				adb->adb_info->ai_hashtable_size);

	err = _siadmindb_account_find(adb, bucket, name,
				arec->aa_hashv, NULL);
	if (!err)
	{
		_siadmindb_del_space(adb, pos, size);
		if (arecptr)
			*arecptr = arec;
		return EEXIST;
	}

	arec->aa_ver = SIADMINDB_VERSION;

	strcpy(arec->aa_name, name);
	arec->aa_last_login = 0;
	arec->aa_login_fail_time = 0;
	arec->aa_login_fail_num = 0;
	arec->aa_passwd_fail_time = 0;
	arec->aa_passwd_fail_num = 0;
	arec->aa_expires = 0;
	arec->aa_flags = 0;
	arec->aa_userhosts = -1;
	arec->aa_userhosts_num = 0;
	strcpy(arec->aa_passwd, "**NP**");

	memset(arec->aa_pad, 0, sizeof(arec->aa_pad));

	arec->aa_h_prev = (u_int)-1;
	if ((arec->aa_h_next = *bucket) != -1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_h_next)->aa_h_prev = pos;
	*bucket = pos;
	arec->aa_a_prev = (u_int)-1;
	if ((arec->aa_a_next = adb->adb_info->ai_accounts) != -1)
		SI_ADMIN_ACCOUNT_PTR(adb, arec->aa_a_next)->aa_a_prev = pos;
	adb->adb_info->ai_accounts = pos;
	adb->adb_info->ai_num_accounts++;

	if (arecptr)
		*arecptr = arec;

	return 0;
}

int siadmindb_userhost_create(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT **arecptr, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr)
{
	u_int					pos;
	u_int					size;
	u_int					uhlen;
	u_int					slen;
	SI_ADMIN_USERHOST		*uhrec;
	SI_ADMIN_ACCOUNT		*arec;
	int						err;
	u_int					arec_offset;

	if (!adb || !arecptr || !*arecptr ||
					!userhost || !*userhost || !server || !*server)
		return EINVAL;

	arec_offset = ((char *)(*arecptr)) - adb->adb_data;

	err = _siadmindb_check(adb);
	if (err)
		return err;

	uhlen = strlen(userhost);
	slen = strlen(server);
	err = _siadmindb_get_space(adb,
				sizeof(SI_ADMIN_USERHOST)+uhlen+slen+1, &pos, &size);
	if (err)
		return err;

	arec = (SI_ADMIN_ACCOUNT *)(adb->adb_data + arec_offset);
	*arecptr = arec;

	uhrec = SI_ADMIN_USERHOST_PTR(adb, pos);
	uhrec->auh_size = size;
	uhrec->auh_ver = SIADMINDB_VERSION;

	err = _siadmindb_userhost_find(adb,
				&(arec->aa_userhosts), userhost, server, NULL);
	if (!err)
	{
		_siadmindb_del_space(adb, pos, size);
		if (uhrecptr)
			*uhrecptr = uhrec;
		return EEXIST;
	}

	uhrec->auh_account_offset = ((char *)arec) - adb->adb_data;
	strcpy(uhrec->auh_userhost, userhost);
	uhrec->auh_server_offset = pos + sizeof(SI_ADMIN_USERHOST) + uhlen;
	strcpy(((char *)uhrec)+sizeof(SI_ADMIN_USERHOST)+uhlen, server);

	memset(uhrec->auh_pad, 0, sizeof(uhrec->auh_pad));

	uhrec->auh_a_prev = (u_int)-1;
	if ((uhrec->auh_a_next = arec->aa_userhosts) != -1)
		SI_ADMIN_USERHOST_PTR(adb, uhrec->auh_a_next)->auh_a_prev = pos;

	arec->aa_userhosts = pos;
	arec->aa_userhosts_num++;

	if (uhrecptr)
		*uhrecptr = uhrec;

	return 0;
}

int siadmindb_userhost_remove(SI_ADMINDB *adb, SI_ADMIN_USERHOST *uhrec)
{
	SI_ADMIN_ACCOUNT	*arec;
	int					err;

	if (!adb || !uhrec)
	{
		return EINVAL;
	}

	err = _siadmindb_check(adb);
	if (err)
		return err;

	arec = SI_ADMIN_ACCOUNT_PTR(adb, uhrec->auh_account_offset);

	if (uhrec->auh_a_prev != (u_int)-1)
		SI_ADMIN_USERHOST_PTR(adb, uhrec->auh_a_prev)->auh_a_next = uhrec->auh_a_next;
	else
		arec->aa_userhosts = uhrec->auh_a_next;
	if (uhrec->auh_a_next != (u_int)-1)
		SI_ADMIN_USERHOST_PTR(adb, uhrec->auh_a_next)->auh_a_prev = uhrec->auh_a_prev;
	--arec->aa_userhosts_num;

	_siadmindb_del_space(adb, ((char *)uhrec) - adb->adb_data, uhrec->auh_size);
	return 0;
}

int siadmindb_account_remove(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec)
{
	int		err;

	if (!adb || !arec)
		return EINVAL;
	while(arec->aa_userhosts != -1)
	{
		err = siadmindb_userhost_remove(adb,
				SI_ADMIN_USERHOST_PTR(adb, arec->aa_userhosts));
		if (err)
			return err;
	}
	_siadmindb_account_remove(adb, arec);
	return 0;
}

int siadmindb_lock(SI_ADMINDB *adb)
{
	struct flock	fl;

#ifdef _REENTRANT
	pthread_mutex_lock(&(adb->adb_lock));
#endif

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(adb->adb_fd, F_SETLKW, &fl))
		return errno;
	return 0;
}

int siadmindb_trylock(SI_ADMINDB *adb)
{
	struct flock	fl;

#ifdef _REENTRANT
	pthread_mutex_lock(&(adb->adb_lock));
#endif

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(adb->adb_fd, F_SETLK, &fl))
		return errno;
	return 0;
}

void siadmindb_unlock(SI_ADMINDB *adb)
{
	struct flock	fl;

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	fcntl(adb->adb_fd, F_SETLKW, &fl);

#ifdef _REENTRANT
	pthread_mutex_unlock(&(adb->adb_lock));
#endif
}


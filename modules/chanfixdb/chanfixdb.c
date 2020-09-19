/*
**   IRC services chanfixdb module -- chanfixdb.c
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
#include "chanfixdb.h"
#include "match.h"

RCSID("$Id: chanfixdb.c,v 1.3 2004/04/04 20:12:59 cbehrens Exp $");

/*
**  need to fill in locking stuff, and add the locking for
**   when adding recs... (next_addpos)
*/

static u_int	_hashtab[256];
static u_int	_hash_inited = 0;

static void sihash_init(void);
static u_int sihash_compute_client(char *nname);
static u_int sihash_compute_channel(char *cname);
static int _chanfixdb_channel_find(SI_CHANFIXDB *cfdb, u_int *bucket, char *name, u_int hashv, SI_CHANFIX_CHAN **chptr);
static int _chanfixdb_channel_chanuser_find(SI_CHANFIXDB *cfdb, u_int *bucket, char *userhost, u_int hashv, SI_CHANFIX_CHANUSER **cuptr);
static int _chanfixdb_check(SI_CHANFIXDB *cfdb);
static void _chanfixdb_del_space(SI_CHANFIXDB *cfdb, u_int pos, u_int size);
static int _chanfixdb_get_space(SI_CHANFIXDB *cfdb, u_int size, u_int *posptr, u_int *sizeptr);
static void _chanfixdb_channel_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr);

static void sihash_init(void)
{
	int i;
	for (i = 0; i < 256; i++)
		_hashtab[i] = tolower((char)i) * 109;
}

static u_int sihash_compute_client(char *nname)
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

static u_int sihash_compute_channel(char *cname)
{
    u_char          *name = (u_char *)cname;
    u_int           hash = 1;
    int             i = 20;

    for(;*name && --i;name++)
    {
        hash <<= 1;
        hash += _hashtab[(u_int)*name] + (i << 1);
    }
    return hash;
}

static int chanfixdb_lock_bucket(SI_CHANFIXDB *cfdb, u_int *bucket)
{
	return 0;
}

static void chanfixdb_unlock_bucket(SI_CHANFIXDB *cfdb, u_int *bucket)
{
	return;
}

static int chanfixdb_lock_chan(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr)
{
	return 0;
}

static void chanfixdb_unlock_chan(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr)
{
	return;
}

static int chanfixdb_lock_chanuser(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser)
{
	return 0;
}

static void chanfixdb_unlock_chanuser(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser)
{
	return;
}


static int _chanfixdb_channel_find(SI_CHANFIXDB *cfdb, u_int *bucket, char *name, u_int hashv, SI_CHANFIX_CHAN **chptr)
{
	u_int				pos;
	SI_CHANFIX_CHAN		*chrec;

	assert(cfdb && name);

	for(pos=*bucket;pos != (u_int)-1;pos=chrec->cfc_h_next)
	{
		chrec = SI_CHANFIX_CHAN_PTR(cfdb, pos);
		if ((chrec->cfc_hashv == hashv) &&
				!ircd_strcmp(name, chrec->cfc_name))
		{
			if (chptr)
				*chptr = chrec;
			return 0;
		}
	}
	return ENOENT;
}

static int _chanfixdb_channel_chanuser_find(SI_CHANFIXDB *cfdb, u_int *bucket, char *userhost, u_int hashv, SI_CHANFIX_CHANUSER **cuptr)
{
	u_int					pos;
	SI_CHANFIX_CHANUSER		*curec;

	assert(cfdb && userhost);

	for(pos=*bucket;pos != (u_int)-1;pos=curec->cfcu_h_next)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
		if ((curec->cfcu_hashv == hashv) &&
				!ircd_strcmp(userhost, curec->cfcu_userhost))
		{
			if (cuptr)
				*cuptr = curec;
			return 0;
		}
	}
	return ENOENT;
}

static int _chanfixdb_check(SI_CHANFIXDB *cfdb)
{
	if (cfdb->cf_info->cfi_size != cfdb->cf_data_size)
	{
		munmap(cfdb->cf_data, cfdb->cf_data_size);
		cfdb->cf_data = (char *)mmap(NULL,
				cfdb->cf_info->cfi_size,
				(cfdb->cf_flags & SI_CHANFIXDB_FLAGS_RDONLY) ? 
					PROT_READ : PROT_READ|PROT_WRITE,
				MAP_SHARED, cfdb->cf_fd, 0);
		if (cfdb->cf_data == (char *)-1)
			return errno;
		cfdb->cf_data_size = cfdb->cf_info->cfi_size;
	}
	return 0;
}

static void _chanfixdb_del_space(SI_CHANFIXDB *cfdb, u_int pos, u_int size)
{
	SI_CHANFIX_FREELIST		*fl;

	fl = SI_CHANFIX_FREELIST_PTR(cfdb, pos);
	fl->cffl_size = size;
	fl->cffl_prev = -1;
	if ((fl->cffl_next = cfdb->cf_info->cfi_freelist) != -1)
		SI_CHANFIX_FREELIST_PTR(cfdb, fl->cffl_next)->cffl_prev = pos;
	cfdb->cf_info->cfi_freelist = pos;
	cfdb->cf_info->cfi_freelist_size += size;
}

static int _chanfixdb_get_space(SI_CHANFIXDB *cfdb, u_int size, u_int *posptr, u_int *sizeptr)
{
	int						err;
	u_int					pos;
	SI_CHANFIX_FREELIST		*fl;

	assert(cfdb && posptr && sizeptr);
	size = (size + 7) & ~7;
	*sizeptr = size;
	for(pos=cfdb->cf_info->cfi_freelist;pos!=-1;pos=fl->cffl_next)
	{
		fl = SI_CHANFIX_FREELIST_PTR(cfdb, pos);
		if ((fl->cffl_size == size) ||
				((fl->cffl_size > size) &&
				(fl->cffl_size - size >= sizeof(SI_CHANFIX_FREELIST))))
		{
			u_int		diff = fl->cffl_size - size;

			if (fl->cffl_prev != (u_int)-1)
				SI_CHANFIX_FREELIST_PTR(cfdb, fl->cffl_prev)->cffl_next = fl->cffl_next;
			else
				cfdb->cf_info->cfi_freelist = fl->cffl_next;
			if (fl->cffl_next != (u_int)-1)
				SI_CHANFIX_FREELIST_PTR(cfdb, fl->cffl_next)->cffl_prev = fl->cffl_prev;

			cfdb->cf_info->cfi_freelist_size -= fl->cffl_size;
			if (diff)
				_chanfixdb_del_space(cfdb, pos+size, diff);
			*posptr = pos;
			return 0;
		}
	}

	pos = cfdb->cf_info->cfi_next_addpos;

	if (pos + size > cfdb->cf_info->cfi_size)
	{
		err = ftruncate(cfdb->cf_fd,
					pos + size + 1024*1024);
		if (err)
		{
			err = ftruncate(cfdb->cf_fd, pos + size);
			if (err)
			{
				return errno;
			}
			cfdb->cf_info->cfi_size = pos + size;
		}
		else
		{
			cfdb->cf_info->cfi_size = pos + size + 1024*1024;
		}
		err = _chanfixdb_check(cfdb);
		if (err)
			return err;
	}

	cfdb->cf_info->cfi_next_addpos += size;

	*posptr = pos;
	return 0;
}

static void _chanfixdb_channel_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chan)
{
	u_int	*bucket;

	assert(cfdb && chan);
	assert(!chan->cfc_users_num);

	if (chan->cfc_a_prev != (u_int)-1)
		SI_CHANFIX_CHAN_PTR(cfdb, chan->cfc_a_prev)->cfc_a_next = chan->cfc_a_next;
	else
		cfdb->cf_info->cfi_channels = chan->cfc_a_next;
	if (chan->cfc_a_next != (u_int)-1)
		SI_CHANFIX_CHAN_PTR(cfdb, chan->cfc_a_next)->cfc_a_prev = chan->cfc_a_prev;
	cfdb->cf_info->cfi_num_channels--;

	bucket = SI_CHANFIX_HASHTABLE_PTR(cfdb, chan->cfc_hashv % 
				cfdb->cf_info->cfi_hashtable_size);
	if (chan->cfc_h_prev != (u_int)-1)
		SI_CHANFIX_CHAN_PTR(cfdb, chan->cfc_h_prev)->cfc_h_next = chan->cfc_h_next;
	else
		*bucket = chan->cfc_h_next;
	if (chan->cfc_h_next != (u_int)-1)
		SI_CHANFIX_CHAN_PTR(cfdb, chan->cfc_h_next)->cfc_h_prev = chan->cfc_h_prev;

	_chanfixdb_del_space(cfdb, ((char *)chan) - cfdb->cf_data, chan->cfc_size);
}

void chanfixdb_deinit(SI_CHANFIXDB *dbptr)
{
	if (!dbptr)
		return;

	if (dbptr->cf_data != (char *)-1)
		munmap((void *)dbptr->cf_data, dbptr->cf_data_size);
	if (dbptr->cf_info != (SI_CHANFIX_INFO *)-1)
		munmap((void *)dbptr->cf_info, sizeof(SI_CHANFIX_INFO));
	if (dbptr->cf_fd >= 0)
		close(dbptr->cf_fd);
	free(dbptr);
}

int chanfixdb_create(char *filename, u_int hashtable_size)
{
	int				fd;
	int				err;
	SI_CHANFIXDB	*cfdb;
	SI_CHANFIX_INFO	info;

	memset(&info, 0, sizeof(info));
	info.cfi_hashtable = (sizeof(info) + 8191) & ~8191;
	info.cfi_hashtable_size = hashtable_size;
	info.cfi_next_addpos = (info.cfi_hashtable + (sizeof(u_int) * hashtable_size) + 7) & ~7;
	info.cfi_size = info.cfi_next_addpos + 1024*1024;
	info.cfi_freelist = -1;
	info.cfi_channels = -1;
	info.cfi_chanusers = -1;

	fd = open(filename, O_CREAT|O_EXCL|O_RDWR, 0600);
	if (fd < 0)
		return errno;
	err = ftruncate(fd, info.cfi_size);
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

	err = chanfixdb_init(filename, 0, &cfdb);
	if (err)
	{
		return err;
	}

	memset(cfdb->cf_data + cfdb->cf_info->cfi_hashtable,
				-1, sizeof(u_int) * hashtable_size);

	chanfixdb_deinit(cfdb);

	return 0;
}

int chanfixdb_init(char *filename, u_int flags, SI_CHANFIXDB **dbptr)
{
	SI_CHANFIXDB	*cfdb;
	struct stat		sb;
	int				err;

	if (!filename || !dbptr)
		return EINVAL;

	if (!_hash_inited)
	{
		sihash_init();
		_hash_inited = 1;
	}

	cfdb = (SI_CHANFIXDB *)calloc(1, sizeof(SI_CHANFIXDB));
	if (!cfdb)
		return ENOMEM;
	cfdb->cf_info = (SI_CHANFIX_INFO *)-1;
	cfdb->cf_data = (char *)-1;
	cfdb->cf_flags = flags;

	if (flags & SI_CHANFIXDB_FLAGS_RDONLY)
		cfdb->cf_fd = open(filename, O_RDONLY);
	else
		cfdb->cf_fd = open(filename, O_RDWR);
	if (cfdb->cf_fd < 0)
	{
		err = errno;
		chanfixdb_deinit(cfdb);
		return err;
	}

	err = fstat(cfdb->cf_fd, &sb);
	if (err)
	{
		err = errno;
		chanfixdb_deinit(cfdb);
		return err;
	}

	if (sb.st_size < sizeof(SI_CHANFIX_INFO))
	{
		chanfixdb_deinit(cfdb);
		return EFAULT;
	}

	cfdb->cf_info = (SI_CHANFIX_INFO *)mmap(NULL,
			sizeof(SI_CHANFIX_INFO),
			(flags & SI_CHANFIXDB_FLAGS_RDONLY) ?
				PROT_READ : PROT_READ|PROT_WRITE,
			MAP_SHARED, cfdb->cf_fd, 0);
	if (cfdb->cf_info == (SI_CHANFIX_INFO *)-1)
	{
		err = errno;
		chanfixdb_deinit(cfdb);
		return err;
	}

	cfdb->cf_data_size = sb.st_size;
	cfdb->cf_data = (char *)mmap(NULL, sb.st_size,
				(flags & SI_CHANFIXDB_FLAGS_RDONLY) ? 
					PROT_READ : PROT_READ|PROT_WRITE,
				MAP_SHARED, cfdb->cf_fd, 0);
	if (cfdb->cf_data == (char *)-1)
	{
		err = errno;
		chanfixdb_deinit(cfdb);
		return err;
	}

	*dbptr = cfdb;

	return 0;
}

int chanfixdb_channel_find(SI_CHANFIXDB *cfdb, char *name, SI_CHANFIX_CHAN **chanptr)
{
	u_int		hashv;
	u_int		*bucket;
	int			err;

	if (!cfdb || !name)
		return EINVAL;

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	hashv = sihash_compute_channel(name);
	bucket = SI_CHANFIX_HASHTABLE_PTR(cfdb, hashv % 
				cfdb->cf_info->cfi_hashtable_size);

	err = chanfixdb_lock_bucket(cfdb, bucket);
	if (err)
		return err;

	err = _chanfixdb_channel_find(cfdb, bucket, name, hashv, chanptr);
	if (err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		return err;
	}

	err = chanfixdb_lock_chan(cfdb, *chanptr);
	if (err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		return err;
	}
	return 0;
}

void chanfixdb_channel_release(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr)
{
	if (cfdb && chanptr)
		chanfixdb_unlock_chan(cfdb, chanptr);
}

int chanfixdb_chanuser_find(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr, char *userhost, SI_CHANFIX_CHANUSER **chanuser)
{
	u_int		hashv;
	u_int		*bucket;
	int			err;

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	hashv = sihash_compute_client(userhost);
	bucket = chanptr->cfc_users_offset + (hashv % SI_CHANFIX_CHANUSER_HASHSIZE);
	err = chanfixdb_lock_bucket(cfdb, bucket);
	if (err)
		return err;
	err = _chanfixdb_channel_chanuser_find(cfdb, bucket, userhost, hashv, chanuser);
	if (err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		return err;
	}

	err = chanfixdb_lock_chanuser(cfdb, *chanuser);
	if (err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		return err;
	}

	return 0;
}

void chanfixdb_chanuser_release(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser)
{
	if (cfdb && chanuser)
		chanfixdb_unlock_chanuser(cfdb, chanuser);
}

int chanfixdb_channel_create(SI_CHANFIXDB *cfdb, char *name, SI_CHANFIX_CHAN **chan)
{
	u_int				pos;
	u_int				size;
	u_int				namelen;
	SI_CHANFIX_CHAN		*chanptr;
	u_int				*bucket;
	int					err;

	if (!cfdb || !name)
		return EINVAL;

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	namelen = strlen(name);
	err = _chanfixdb_get_space(cfdb,
				sizeof(SI_CHANFIX_CHAN)+namelen, &pos, &size);
	if (err)
		return err;

	chanptr = SI_CHANFIX_CHAN_PTR(cfdb, pos);
	chanptr->cfc_size = size;
	chanptr->cfc_hashv = sihash_compute_channel(name);

	bucket = SI_CHANFIX_HASHTABLE_PTR(cfdb, chanptr->cfc_hashv % 
				cfdb->cf_info->cfi_hashtable_size);

	err = chanfixdb_lock_bucket(cfdb, bucket);
	if (err)
	{
		_chanfixdb_del_space(cfdb, pos, size);
		return err;
	}

	err = _chanfixdb_channel_find(cfdb, bucket, name,
				chanptr->cfc_hashv, NULL);
	if (!err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		_chanfixdb_del_space(cfdb, pos, size);
		if (chan)
			*chan = chanptr;
		return EEXIST;
	}

	strcpy(chanptr->cfc_name, name);
	chanptr->cfc_last_seen = 0;
	chanptr->cfc_checks = 0;
	memset(chanptr->cfc_users_offset, -1, sizeof(chanptr->cfc_users_offset));
	chanptr->cfc_users_num = 0;
#ifndef OLD_FORMAT
	memset(chanptr->cfc_modes, 0, sizeof(chanptr->cfc_modes));
	chanptr->cfc_flags = 0;
	chanptr->cfc_fixed_time = 0;
	chanptr->cfc_fixed_passes = 0;
	chanptr->cfc_users_sorted_first = (u_int)-1;
	chanptr->cfc_users_sorted_last = (u_int)-1;
#endif
	chanptr->cfc_h_prev = (u_int)-1;
	if ((chanptr->cfc_h_next = *bucket) != -1)
		SI_CHANFIX_CHAN_PTR(cfdb, chanptr->cfc_h_next)->cfc_h_prev = pos;
	*bucket = pos;
	chanptr->cfc_a_prev = (u_int)-1;
	if ((chanptr->cfc_a_next = cfdb->cf_info->cfi_channels) != -1)
		SI_CHANFIX_CHAN_PTR(cfdb, chanptr->cfc_a_next)->cfc_a_prev = pos;
	cfdb->cf_info->cfi_channels = pos;
	cfdb->cf_info->cfi_num_channels++;

	chanfixdb_unlock_bucket(cfdb, bucket);

	if (chan)
		*chan = chanptr;

	return 0;
}

int chanfixdb_chanuser_create(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN **chanptr, char *userhost, SI_CHANFIX_CHANUSER **chanuserptr)
{
	u_int					pos;
	u_int					size;
	u_int					uhlen;
	SI_CHANFIX_CHANUSER		*chanuser;
	SI_CHANFIX_CHAN			*chan;
	u_int					*bucket;
	int						err;
	u_int					chan_offset;

	if (!cfdb || !chanptr || !*chanptr || !userhost)
		return EINVAL;

	chan_offset = ((char *)(*chanptr)) - cfdb->cf_data;

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	uhlen = strlen(userhost);
	err = _chanfixdb_get_space(cfdb,
				sizeof(SI_CHANFIX_CHANUSER)+uhlen, &pos, &size);
	if (err)
		return err;

	chan = (SI_CHANFIX_CHAN *)(cfdb->cf_data + chan_offset);
	*chanptr = chan;

	chanuser = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
	chanuser->cfcu_size = size;
	chanuser->cfcu_hashv = sihash_compute_client(userhost);
	chanuser->cfcu_last_seen = 0;

	bucket = chan->cfc_users_offset + (chanuser->cfcu_hashv %
							SI_CHANFIX_CHANUSER_HASHSIZE);

	err = chanfixdb_lock_bucket(cfdb, bucket);
	if (err)
	{
		_chanfixdb_del_space(cfdb, pos, size);
		return err;
	}

	err = _chanfixdb_channel_chanuser_find(cfdb, bucket, userhost,
				chanuser->cfcu_hashv, NULL);
	if (!err)
	{
		chanfixdb_unlock_bucket(cfdb, bucket);
		_chanfixdb_del_space(cfdb, pos, size);
		if (chanuserptr)
			*chanuserptr = chanuser;
		return EEXIST;
	}

	strcpy(chanuser->cfcu_userhost, userhost);
	chanuser->cfcu_last_seen = 0;
	chanuser->cfcu_chan_offset = ((char *)chan) - cfdb->cf_data;
	chanuser->cfcu_tot_score = 0;
	memset(chanuser->cfcu_scores, 0, sizeof(chanuser->cfcu_scores));

#ifndef OLD_FORMAT
	/* add to cfc_users_sorted */
	chanuser->cfcu_s_prev = (u_int)-1;
	if ((chanuser->cfcu_s_next = chan->cfc_users_sorted_first) == (u_int)-1)
	{
		chan->cfc_users_sorted_last = pos;
	}
	else
	{
		SI_CHANFIX_CHANUSER_PTR(cfdb, chan->cfc_users_sorted_first)->cfcu_s_prev = pos;
	}
	chan->cfc_users_sorted_first = pos;
#endif

	/* add to cfc_users_offset */
	chanuser->cfcu_h_prev = (u_int)-1;
	if ((chanuser->cfcu_h_next = *bucket) != -1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_h_next)->cfcu_h_prev = pos;
	*bucket = pos;
	chan->cfc_users_num++;

	*bucket = pos;
	chanuser->cfcu_a_prev = (u_int)-1;
	if ((chanuser->cfcu_a_next = cfdb->cf_info->cfi_chanusers) != -1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_a_next)->cfcu_a_prev = pos;
	cfdb->cf_info->cfi_chanusers = pos;
	cfdb->cf_info->cfi_num_chanusers++;

	chanfixdb_unlock_bucket(cfdb, bucket);

	if (chanuserptr)
		*chanuserptr = chanuser;

	return 0;
}

int chanfixdb_chanuser_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser)
{
	SI_CHANFIX_CHAN		*chan;
	int					err;

	if (!cfdb || !chanuser)
	{
		return EINVAL;
	}

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	chan = SI_CHANFIX_CHAN_PTR(cfdb, chanuser->cfcu_chan_offset);

	if (chanuser->cfcu_a_prev != (u_int)-1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_a_prev)->cfcu_a_next = chanuser->cfcu_a_next;
	else
		cfdb->cf_info->cfi_chanusers = chanuser->cfcu_a_next;
	if (chanuser->cfcu_a_next != (u_int)-1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_a_next)->cfcu_a_prev = chanuser->cfcu_a_prev;
	cfdb->cf_info->cfi_num_chanusers--;

	if (chanuser->cfcu_h_prev != (u_int)-1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_h_prev)->cfcu_h_next = chanuser->cfcu_h_next;
	else
		chan->cfc_users_offset[chanuser->cfcu_hashv % SI_CHANFIX_CHANUSER_HASHSIZE] = chanuser->cfcu_h_next;
	if (chanuser->cfcu_h_next != (u_int)-1)
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_h_next)->cfcu_h_prev = chanuser->cfcu_h_prev;

#ifndef OLD_FORMAT
	if (chanuser->cfcu_s_prev == (u_int)-1)
		chan->cfc_users_sorted_first = chanuser->cfcu_s_next;
	else
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_prev)->cfcu_s_next = chanuser->cfcu_s_next;
	if (chanuser->cfcu_s_next == (u_int)-1)
		chan->cfc_users_sorted_last = chanuser->cfcu_s_prev;
	else
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_next)->cfcu_s_prev = chanuser->cfcu_s_prev;
#endif

#if 0
	/* let's not do this automagically */
	if (!--chan->cfc_users_num)
	{
		_chanfixdb_channel_remove(cfdb, chan);
	}
#else
	--chan->cfc_users_num;
#endif

	_chanfixdb_del_space(cfdb, ((char *)chanuser) - cfdb->cf_data, chanuser->cfcu_size);
	return 0;
}


int chanfixdb_chanuser_sort(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser)
{
#ifdef OLD_FORMAT
	return 0;
#else
	SI_CHANFIX_CHANUSER		*cu;
	SI_CHANFIX_CHAN			*chan;
	u_int					chanuserpos;
	int						err;

	if (!cfdb || !chanuser)
		return EINVAL;

	err = _chanfixdb_check(cfdb);
	if (err)
		return err;

	chanuserpos = ((char *)chanuser) - cfdb->cf_data;
	chan = SI_CHANFIX_CHAN_PTR(cfdb, chanuser->cfcu_chan_offset);

	if ((chanuser->cfcu_s_next != (u_int)-1) &&
		(chanuser->cfcu_tot_score > (cu = SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_next))->cfcu_tot_score))
	{
		/* search forwards to find where to move 'chanuser'. */

		do
		{
			if (cu->cfcu_s_next == (u_int)-1)
			{
				cu = NULL;
				break;
			}
		} while ((chanuser->cfcu_tot_score > (cu = SI_CHANFIX_CHANUSER_PTR(cfdb, cu->cfcu_s_next))->cfcu_tot_score));

		/* remove from old spot first */
		if (chanuser->cfcu_s_prev == (u_int)-1)
			chan->cfc_users_sorted_first = chanuser->cfcu_s_next;
		else
			SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_prev)->cfcu_s_next = chanuser->cfcu_s_next;
		if (chanuser->cfcu_s_next == (u_int)-1)
			chan->cfc_users_sorted_last = chanuser->cfcu_s_prev;
		else
			SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_next)->cfcu_s_prev = chanuser->cfcu_s_prev;

		if (!cu)
		{
			/* put it at the end */

			chanuser->cfcu_s_next = (u_int)-1;
			if ((chanuser->cfcu_s_prev = chan->cfc_users_sorted_last) == (u_int)-1)
				chan->cfc_users_sorted_first = chanuserpos;
			else
				SI_CHANFIX_CHANUSER_PTR(cfdb, chan->cfc_users_sorted_last)->cfcu_s_next = chanuserpos;
			chan->cfc_users_sorted_last = chanuserpos;

			return 0;
		}

		/* add before 'cu' */

		chanuser->cfcu_s_next = ((char *)cu) - cfdb->cf_data;
		if ((chanuser->cfcu_s_prev = cu->cfcu_s_prev) == (u_int)-1)
			chan->cfc_users_sorted_first = chanuserpos;
		else
			SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_prev)->cfcu_s_next = chanuserpos;
		cu->cfcu_s_prev = chanuserpos;

		return 0;
	}
	else if ((chanuser->cfcu_s_prev == (u_int)-1) ||
		(chanuser->cfcu_tot_score >= (cu = SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_prev))->cfcu_tot_score))
	{
		return 0;
	}

	/* search backwards to find where to move 'chanuser'. */

	do
	{
		if (cu->cfcu_s_prev == (u_int)-1)
		{
			cu = NULL;
			break;
		}
	} while ((chanuser->cfcu_tot_score < (cu = SI_CHANFIX_CHANUSER_PTR(cfdb, cu->cfcu_s_prev))->cfcu_tot_score));

	/* remove from old spot first */
	if (chanuser->cfcu_s_prev == (u_int)-1)
		chan->cfc_users_sorted_first = chanuser->cfcu_s_next;
	else
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_prev)->cfcu_s_next = chanuser->cfcu_s_next;
	if (chanuser->cfcu_s_next == (u_int)-1)
		chan->cfc_users_sorted_last = chanuser->cfcu_s_prev;
	else
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_next)->cfcu_s_prev = chanuser->cfcu_s_prev;

	if (!cu)
	{
		/* put it at beginning */

		chanuser->cfcu_s_prev = (u_int)-1;
		if ((chanuser->cfcu_s_next = chan->cfc_users_sorted_first) == (u_int)-1)
			chan->cfc_users_sorted_last = chanuserpos;
		else
			SI_CHANFIX_CHANUSER_PTR(cfdb, chan->cfc_users_sorted_first)->cfcu_s_prev = chanuserpos;
		chan->cfc_users_sorted_first = chanuserpos;

		return 0;
	}

	/* put after 'cu' */
	chanuser->cfcu_s_prev = ((char *)cu) - cfdb->cf_data;
	if ((chanuser->cfcu_s_next = cu->cfcu_s_next) == (u_int)-1)
		chan->cfc_users_sorted_last = chanuserpos;
	else
		SI_CHANFIX_CHANUSER_PTR(cfdb, chanuser->cfcu_s_next)->cfcu_s_prev = chanuserpos;
	cu->cfcu_s_next = chanuserpos;

	return 0;
#endif
}

int chanfixdb_channel_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chan)
{
	if (!cfdb || !chan)
		return EINVAL;
	if (chan->cfc_users_num)
		return EEXIST;
	_chanfixdb_channel_remove(cfdb, chan);
	return 0;
}

void chanfixdb_channel_cleanup(SI_CHANFIXDB *cfdb)
{
	u_int				pos;
	u_int				nextpos;
	SI_CHANFIX_CHAN		*chrec;

	for(pos=cfdb->cf_info->cfi_channels;
				pos!=(u_int)-1;
				pos=nextpos)
	{
		chrec = SI_CHANFIX_CHAN_PTR(cfdb, pos);
		nextpos = chrec->cfc_a_next;
		if (!chrec->cfc_users_num)
		{
			chanfixdb_channel_remove(cfdb, chrec);
		}
	}

}

void chanfixdb_roll(SI_CHANFIXDB *cfdb)
{
	int					i;
	u_int				pos;
	SI_CHANFIX_CHAN		*chan;

	for(pos=cfdb->cf_info->cfi_channels;
			pos!=(u_int)-1;
			pos=chan->cfc_a_next)
	{
		chan = SI_CHANFIX_CHAN_PTR(cfdb, pos);

		/* roll chan user scores */

		chanfixdb_chanuser_roll(cfdb, chan);

		/* roll the modes */

		for(i=0;
			i<(sizeof(chan->cfc_modes)/sizeof(chan->cfc_modes[0]));
			i++)
		{
			chan->cfc_modes[i].cfcm_tot_score -=
				chan->cfc_modes[i].cfcm_scores[
			(sizeof(chan->cfc_modes)/sizeof(chan->cfc_modes[0])) - 1];

			memmove(chan->cfc_modes[i].cfcm_scores + 1,
				chan->cfc_modes[i].cfcm_scores,
				sizeof(chan->cfc_modes[i].cfcm_scores) -
					sizeof(chan->cfc_modes[i].cfcm_scores[0]));

			chan->cfc_modes[i].cfcm_scores[0] = 0;
		}
	}
}

void chanfixdb_chanuser_roll(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chan)
{
	u_int					hv;
	u_int					pos;
	u_int					nextpos;
	SI_CHANFIX_CHANUSER		*curec;

	for(hv=0;hv<SI_CHANFIX_CHANUSER_HASHSIZE;hv++)
	{
		for(pos=chan->cfc_users_offset[hv];
					pos != (u_int)-1;
					pos = nextpos)
		{
			curec = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
			nextpos = curec->cfcu_h_next;

			curec->cfcu_tot_score -= curec->cfcu_scores[sizeof(curec->cfcu_scores)/sizeof(curec->cfcu_scores[0]) - 1];
			memmove(curec->cfcu_scores+1,
				curec->cfcu_scores,
				sizeof(curec->cfcu_scores) - sizeof(curec->cfcu_scores[0]));
			curec->cfcu_scores[0] = 0;

			if (curec->cfcu_tot_score)
			{
				chanfixdb_chanuser_sort(cfdb, curec);
				continue;
			}
			chanfixdb_chanuser_remove(cfdb, curec);
		}
	}
}

void chanfixdb_dump(SI_CHANFIXDB *db1, char *filename)
{
	FILE				*foo;
	SI_CHANFIX_CHANUSER	*curec;
	SI_CHANFIX_CHAN		*chrec;
	u_int				pos;
	int					i;

	foo = fopen(filename, "w");
	if (!foo)
		return;

	for(pos=db1->cf_info->cfi_chanusers;pos!=-1;pos=curec->cfcu_a_next)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(db1, pos);
		chrec = SI_CHANFIX_CHAN_PTR(db1, curec->cfcu_chan_offset);

		if (curec->cfcu_chan_offset == -1)
			continue;

		fprintf(foo, "%s %lu %u %s %lu ",
				chrec->cfc_name,
				chrec->cfc_last_seen,
				chrec->cfc_checks,
				curec->cfcu_userhost,
				curec->cfcu_last_seen);

		for(i=0;i<sizeof(curec->cfcu_scores)/sizeof(curec->cfcu_scores[0]);i++)
			fprintf(foo, "%u ", curec->cfcu_scores[i]);
		fprintf(foo, "\n");
	}

	fclose(foo);
}

void chanfixdb_import(SI_CHANFIXDB *db1, char *filename)
{
	FILE	*foo;
	char	*buf;
	char	*ptr;
	char	*uh;
	char	*ls;
	char	*checks;
	char	*uls;
	char	*score;
	int		i;
	int		err;
	u_short	scores[14];
	u_int	tot_score;
	SI_CHANFIX_CHAN			*chan;
	SI_CHANFIX_CHANUSER		*chanuser;


	if (sizeof(scores) != sizeof(chanuser->cfcu_scores))
		return;

	foo = fopen(filename, "r");
	if (!foo)
		return;

	memset(scores, 0, sizeof(scores));

	buf = (char *)malloc(4096);
	if (!buf)
		return;

	buf[4095] = '\0';

	while(((ptr = fgets(buf, 4095, foo)) != NULL) || !feof(foo))
	{
		if (!ptr)
			continue;
		ls = strchr(buf, ' ');
		if (!ls)
			continue;
		*ls++ = '\0';
		checks = strchr(ls, ' ');
		if (!checks)
			continue;
		*checks++ = '\0';
		uh = strchr(checks, ' ');
		if (!uh)
			continue;
		*uh++ = '\0';
		uls = strchr(uh, ' ');
		if (!uls)
			continue;
		*uls++ = '\0';
		ptr = uls;
		score = strchr(ptr, ' ');
		if (score)
			*score++ = '\0';

		tot_score = 0;

		for(i=0;i<14;i++)
		{
			ptr = score;
			score = strchr(score, ' ');
			if (score)
				*score++ = '\0';
			scores[i] = atoi(ptr);
			tot_score += scores[i];
		}

		err = chanfixdb_channel_find(db1, buf, &chan);
		if (err)
		{
			err = chanfixdb_channel_create(db1, buf, &chan);
			if (err)
			{
				printf("Couldn't create channel %s: %d\n", buf, err);
				continue;
			}
			chan->cfc_last_seen = atoi(ls);
			chan->cfc_checks = atoi(checks);
		}

		err = chanfixdb_chanuser_create(db1, &chan, uh, &chanuser);
		if (err)
		{
			printf("Couldn't create chanuser %s/%s: %d\n", buf, uh, err);
			continue;
		}

		chanuser->cfcu_last_seen = atoi(uls);
		chanuser->cfcu_tot_score = tot_score;

		memcpy(chanuser->cfcu_scores, scores, sizeof(scores));

		err = chanfixdb_chanuser_sort(db1, chanuser);
		if (err)
			printf("Error sorting chanuser: %s/%s: %d\n", buf, uh, err);
	}

	free(buf);
}

void chanfixdb_repair(SI_CHANFIXDB *db1, SI_CHANFIXDB *db2)
{
	SI_CHANFIX_CHANUSER	*curec;
	SI_CHANFIX_CHAN		*chrec;
	SI_CHANFIX_CHAN		*chan;
	SI_CHANFIX_CHANUSER	*chanuser;
	u_int				pos;
	u_int				pos2;
	int					err;
	register int		i, ii;

	db2->cf_info->cfi_checks = db1->cf_info->cfi_checks;

#if 0

	for(pos=db1->cf_info->cfi_chanusers;pos!=-1;pos=curec->cfcu_a_next)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(db1, pos);
		chrec = SI_CHANFIX_CHAN_PTR(db1, curec->cfcu_chan_offset);

		err = chanfixdb_channel_find(db2, chrec->cfc_name, &chan);
		if (err)
		{
			err = chanfixdb_channel_create(db2, chrec->cfc_name, &chan);
			if (!err)
			{
				chan->cfc_checks = chrec->cfc_checks;
				chan->cfc_last_seen = chrec->cfc_last_seen;
			}
		}
		if (err)
			continue;

		err = chanfixdb_chanuser_create(db2, &chan,
					curec->cfcu_userhost, &chanuser);
		if (!err)
		{
			u_int		tot_score;
			int			i = sizeof(chanuser->cfcu_scores)/sizeof(chanuser->cfcu_scores[0]);

			memcpy(&(chanuser->cfcu_scores[0]),
						&(curec->cfcu_scores[0]),
						sizeof(chanuser->cfcu_scores));
			chanuser->cfcu_tot_score = 0;
			while(i--)
			{
				chanuser->cfcu_tot_score += curec->cfcu_scores[i];
			}
			chanuser->cfcu_last_seen = curec->cfcu_last_seen;
		}
	}

#else

	for(pos=db1->cf_info->cfi_channels;pos!=-1;pos=chrec->cfc_a_next)
	{
		chrec = SI_CHANFIX_CHAN_PTR(db1, pos);

		err = chanfixdb_channel_find(db2, chrec->cfc_name, &chan);
		if (err)
		{
			err = chanfixdb_channel_create(db2, chrec->cfc_name, &chan);
			if (err)
				continue;
		}

		chan->cfc_checks = chrec->cfc_checks;
		chan->cfc_last_seen = chrec->cfc_last_seen;

		for(i=0;i<SI_CHANFIX_CHANUSER_HASHSIZE;i++)
		{
			ii = 0;
			for(pos2=chrec->cfc_users_offset[i];pos2!=-1;pos2=curec->cfcu_h_next)
			{
				if (++ii > chrec->cfc_users_num)
					break;
				curec = SI_CHANFIX_CHANUSER_PTR(db1, pos2);

				err = chanfixdb_chanuser_create(db2, &chan,
					curec->cfcu_userhost, &chanuser);
				if (!err)
				{
					int			i = sizeof(chanuser->cfcu_scores)/
									sizeof(chanuser->cfcu_scores[0]);

					memcpy(&(chanuser->cfcu_scores[0]),
						&(curec->cfcu_scores[0]),
						sizeof(chanuser->cfcu_scores));
					chanuser->cfcu_tot_score = 0;
					while(i--)
					{
						chanuser->cfcu_tot_score += curec->cfcu_scores[i];
					}
					chanuser->cfcu_last_seen = curec->cfcu_last_seen;
				}
			}
		}
	}
#endif
}

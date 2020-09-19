/*
**   IRC services chanfixdb module -- chanfixdb.h
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

#ifndef __CHANFIXDB_H__
#define __CHANFIXDB_H__

/*
** $Id: chanfixdb.h,v 1.2 2004/03/30 22:23:45 cbehrens Exp $
*/

#include "setup.h"

#define SI_CHANFIXDB_CHAN_MODE_INVITEONLY     0x001
#define SI_CHANFIXDB_CHAN_MODE_MODERATED      0x002   
#define SI_CHANFIXDB_CHAN_MODE_NOPRIVMSGS     0x004
#define SI_CHANFIXDB_CHAN_MODE_PRIVATE        0x008   
#define SI_CHANFIXDB_CHAN_MODE_SECRET         0x010   
#define SI_CHANFIXDB_CHAN_MODE_TOPICLIMIT     0x020
#define SI_CHANFIXDB_CHAN_MODE_LIMIT          0x040
#define SI_CHANFIXDB_CHAN_MODE_KEY            0x080


typedef struct _chanfix_info		SI_CHANFIX_INFO;
typedef struct _chanfix_freelist	SI_CHANFIX_FREELIST;
typedef struct _chanfix_chanmode	SI_CHANFIX_CHANMODE;
typedef struct _chanfix_chan		SI_CHANFIX_CHAN;
typedef struct _chanfix_chanuser	SI_CHANFIX_CHANUSER;
typedef struct _chanfixdb			SI_CHANFIXDB;

struct _chanfix_info
{
	u_int		cfi_size;
	u_int		cfi_next_addpos;

	u_int		cfi_checks;

	u_int		cfi_hashtable;
	u_int		cfi_hashtable_size;

	u_int		cfi_num_channels;
	u_int		cfi_channels;

	u_int		cfi_num_chanusers;
	u_int		cfi_chanusers;

	u_int		cfi_freelist;
	u_int		cfi_freelist_size;

	u_int		cfi_max_servers;

};

struct _chanfix_freelist
{
	u_int		cffl_size;			/* size used on disk */

	u_int		cffl_prev;
	u_int		cffl_next;
};

struct _chanfix_chanmode
{
	u_int		cfcm_modes;
	time_t		cfcm_last_seen;
	u_int		cfcm_tot_score;
	u_short		cfcm_scores[14];
};

struct _chanfix_chan
{
	u_int				cfc_size;			/* size used on disk */

	u_int				cfc_hashv;

	time_t				cfc_last_seen;
	u_int				cfc_checks;

#define SI_CHANFIX_CHANUSER_HASHSIZE		15

#ifndef OLD_FORMAT
	SI_CHANFIX_CHANMODE	cfc_modes[5];

#define SI_CHANFIX_CHAN_FLAGS_FIXING		0x001
#define SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL	0x002
	u_int				cfc_flags;
	time_t				cfc_fixed_time;
	u_int				cfc_fixed_passes;

	u_int				cfc_users_sorted_first;
	u_int				cfc_users_sorted_last;
#endif

	u_int				cfc_users_offset[SI_CHANFIX_CHANUSER_HASHSIZE];
	u_int				cfc_users_num;


	u_int				cfc_h_prev;
	u_int				cfc_h_next;
	u_int				cfc_a_prev;
	u_int				cfc_a_next;

	u_char				cfc_name[1];
};

struct _chanfix_chanuser
{
	u_int		cfcu_size;			/* size used on disk */
	u_int		cfcu_hashv;

	time_t		cfcu_last_seen;

	u_int		cfcu_chan_offset;	/* ptr back to chan struct */

	u_int		cfcu_tot_score;

#ifndef OLD_FORMAT
	u_int		cfcu_s_prev;
	u_int		cfcu_s_next;
#endif

	u_int		cfcu_h_prev;
	u_int		cfcu_h_next;

	u_int		cfcu_a_prev;
	u_int		cfcu_a_next;

	u_short		cfcu_scores[14];

	u_char		cfcu_userhost[1];
};

struct _chanfixdb
{
	int					cf_fd;
	SI_CHANFIX_INFO		*cf_info;
	char				*cf_data;
	u_int				cf_data_size;
#define SI_CHANFIXDB_FLAGS_RDONLY		0x001
	u_int				cf_flags;

};

#define SI_CHANFIX_CHAN_PTR(__cfdb, __pos)			\
		((SI_CHANFIX_CHAN *)((__cfdb)->cf_data+__pos))
#define SI_CHANFIX_CHANUSER_PTR(__cfdb, __pos)		\
		((SI_CHANFIX_CHANUSER *)((__cfdb)->cf_data+__pos))
#define SI_CHANFIX_FREELIST_PTR(__cfdb, __pos)		\
		((SI_CHANFIX_FREELIST *)((__cfdb)->cf_data+__pos))
#define SI_CHANFIX_HASHTABLE_PTR(__cfdb, __hv) 		\
		(((u_int *)((__cfdb)->cf_data+(__cfdb)->cf_info->cfi_hashtable)) + __hv)

void chanfixdb_deinit(SI_CHANFIXDB *dbptr);
int chanfixdb_create(char *filename, u_int hashtable_size);
int chanfixdb_init(char *filename, u_int flags, SI_CHANFIXDB **dbptr);
int chanfixdb_channel_find(SI_CHANFIXDB *cfdb, char *name, SI_CHANFIX_CHAN **chanptr);
void chanfixdb_channel_release(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr);
int chanfixdb_chanuser_find(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chanptr, char *userhost, SI_CHANFIX_CHANUSER **chanuser);
void chanfixdb_chanuser_release(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser);
int chanfixdb_channel_create(SI_CHANFIXDB *cfdb, char *name, SI_CHANFIX_CHAN **chan);
int chanfixdb_chanuser_create(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN **chan, char *userhost, SI_CHANFIX_CHANUSER **chanuser);
int chanfixdb_chanuser_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser);
int chanfixdb_chanuser_sort(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHANUSER *chanuser);
int chanfixdb_channel_remove(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chan);
void chanfixdb_channel_cleanup(SI_CHANFIXDB *cfdb);
void chanfixdb_roll(SI_CHANFIXDB *cfdb);
void chanfixdb_chanuser_roll(SI_CHANFIXDB *cfdb, SI_CHANFIX_CHAN *chan);

#endif

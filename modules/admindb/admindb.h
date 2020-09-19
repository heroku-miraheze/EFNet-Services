/*
**   IRC services admindb module -- admindb.h
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

#ifndef __ADMINDB_H__
#define __ADMINDB_H__

/*
** $Id: admindb.h,v 1.2 2004/03/30 22:23:44 cbehrens Exp $
*/

#include "setup.h"
#ifdef _REENTRANT
#include <pthread.h>
#endif


#define SIADMINDB_VERSION		1

typedef struct _siadmin_info		SI_ADMIN_INFO;
typedef struct _siadmin_freelist	SI_ADMIN_FREELIST;
typedef struct _siadmin_account		SI_ADMIN_ACCOUNT;
typedef struct _siadmin_userhost	SI_ADMIN_USERHOST;
typedef struct _siadmindb			SI_ADMINDB;

struct _siadmin_info
{
	u_int		ai_size;
	u_int		ai_next_addpos;

	u_int		ai_hashtable;
	u_int		ai_hashtable_size;

	u_int		ai_num_accounts;
	u_int		ai_accounts;

	u_int		ai_freelist;
	u_int		ai_freelist_size;

};

struct _siadmin_freelist
{
	u_int		afl_size;			/* size used on disk */

	u_int		afl_prev;
	u_int		afl_next;
};

struct _siadmin_account
{
	u_int		aa_size;			/* size used on disk */
	u_int		aa_ver;

	u_int		aa_hashv;

	time_t		aa_last_login;

	time_t		aa_login_fail_time;
	u_int		aa_login_fail_num;

	time_t		aa_passwd_fail_time;
	u_int		aa_passwd_fail_num;

	time_t		aa_expires;
	u_int		aa_flags;

	char		aa_passwd[64];

	u_int		aa_userhosts;
	u_int		aa_userhosts_num;

	char		aa_pad[128];

	u_int		aa_h_prev;
	u_int		aa_h_next;
	u_int		aa_a_prev;
	u_int		aa_a_next;

	u_char		aa_name[1];
};

struct _siadmin_userhost
{
	u_int		auh_size;			/* size used on disk */
	u_int		auh_ver;			/* version of this rec */

	u_int		auh_account_offset;	/* ptr back to account struct */
	u_int		auh_server_offset;	/* ptr to servername for this uh */

	u_int		auh_a_prev;
	u_int		auh_a_next;

	char		auh_pad[128];	/* future */

	u_char		auh_userhost[1];
};

struct _siadmindb
{
	int					adb_fd;
#ifdef _REENTRANT
	pthread_mutex_t		adb_lock;
#endif

	SI_ADMIN_INFO		*adb_info;

	char				*adb_data;
	u_int				adb_data_size;

#define SI_ADMINDB_FLAGS_RDONLY		0x001
	u_int				adb_flags;

};

#define SI_ADMIN_ACCOUNT_PTR(__adb, __pos)			\
		((SI_ADMIN_ACCOUNT *)((__adb)->adb_data+__pos))
#define SI_ADMIN_USERHOST_PTR(__adb, __pos)		\
		((SI_ADMIN_USERHOST *)((__adb)->adb_data+__pos))
#define SI_ADMIN_PASSWD_PTR(__adb, __pos)		\
		((char *)((__adb)->adb_data+__pos))
#define SI_ADMIN_SERVER_PTR(__adb, __pos)		\
		((char *)((__adb)->adb_data+__pos))
#define SI_ADMIN_FREELIST_PTR(__adb, __pos)		\
		((SI_ADMIN_FREELIST *)((__adb)->adb_data+__pos))
#define SI_ADMIN_HASHTABLE_PTR(__adb, __hv) 		\
		(((u_int *)((__adb)->adb_data+(__adb)->adb_info->ai_hashtable)) + __hv)

void siadmindb_deinit(SI_ADMINDB *adb);
int siadmindb_create(char *filename, u_int hashtable_size);
int siadmindb_init(char *filename, u_int flags, SI_ADMINDB **adbptr);
int siadmindb_account_find(SI_ADMINDB *adb, char *name, SI_ADMIN_ACCOUNT **arecptr);
int siadmindb_userhost_find(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr);
int siadmindb_account_create(SI_ADMINDB *adb, char *name, SI_ADMIN_ACCOUNT **arecptr);
int siadmindb_userhost_create(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT **arecptr, char *userhost, char *server, SI_ADMIN_USERHOST **uhrecptr);
int siadmindb_userhost_remove(SI_ADMINDB *adb, SI_ADMIN_USERHOST *uhrec);
int siadmindb_account_remove(SI_ADMINDB *adb, SI_ADMIN_ACCOUNT *arec);
void siadmindb_account_cleanup(SI_ADMINDB *adb);
int siadmindb_lock(SI_ADMINDB *adb);
int siadmindb_trylock(SI_ADMINDB *adb);
void siadmindb_unlock(SI_ADMINDB *adb);

#endif

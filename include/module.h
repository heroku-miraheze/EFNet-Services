/*
**   IRC services -- module.h
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

#ifndef __MODULE_H__
#define __MODULE_H__

/*
** $Id: module.h,v 1.3 2004/03/30 22:23:43 cbehrens Exp $
*/

#include "hash.h"
#include "mempool.h"

typedef int (*si_modcmd_func_t)(SI_CLIENT *from, SI_CLIENT *to, char *args);

#define SIMODULE_CUR_VERSION		1
#define SIMODULE_MIN_VERSION		1
#define SIMODULE_COMPAT_VERSION		1

typedef struct si_mod_client			SI_MOD_CLIENT;
typedef struct si_mod_command			SI_MOD_COMMAND;
typedef struct si_mod_callbacks			SI_MOD_CALLBACKS;

struct si_mod_client
{
	char				*mcl_name;
	char				*mcl_username;
	char				*mcl_hostname;
	char				*mcl_info;
	u_char				mcl_type;
	u_int				mcl_umodes;
	u_int				mcl_flags;
	SI_MOD_COMMAND		*mcl_commands;
};

struct si_mod_command
{
	char				*mc_name;
	char				*mc_desc;
#define SI_MC_FLAGS_NOHELP		0x001
#define SI_MC_FLAGS_OPERONLY	0x002
#define SI_MC_FLAGS_NOLOG		0x004
	u_int				mc_flags;
	si_modcmd_func_t	mc_func;
};

struct si_mod_callbacks
{
	void				(*cb_client_added)(SI_CLIENT *cptr);
	void				(*cb_server_added)(SI_CLIENT *cptr);
	void				(*cb_client_removed)(SI_CLIENT *cptr, char *comment);
	void				(*cb_server_removed)(SI_CLIENT *cptr, char *comment);
	void				(*cb_channel_added)(SI_CHANNEL *chptr);
	void				(*cb_channel_removed)(SI_CHANNEL *chptr);
	void				(*cb_nick_changed)(SI_CLIENT *cptr, char *old_nick);
	void				(*cb_umode_changed)(SI_CLIENT *cptr, u_int old_modes);
	void				(*cb_chmode_changed)(SI_CHANNEL *chptr, u_int old_modes, char *old_key, u_int old_limit);
	void				(*cb_topic_changed)(SI_CHANNEL *chptr, char *old_topic, char *old_topic_who, time_t old_topic_time);
	void				(*cb_channel_client_added)(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
	void				(*cb_channel_client_removed)(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
	void				(*cb_channel_clmode_changed)(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes);
	void				(*cb_channel_ban_added)(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
	void				(*cb_channel_ban_removed)(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
	void				(*cb_sjoin_done)(SI_CHANNEL *chptr);
};

struct si_module_decl
{
	u_int				mod_version;
	u_int				mod_compat_version;

	char				*mod_descr;
	SI_MEMPOOL			*mod_mempool;
	SI_MOD_CALLBACKS	*mod_callbacks;
	int					(*mod_init)(void *modhandle, char *args);
	void				(*mod_deinit)(void);

};

#define SI_MODULE_DECL(__a, __b, __c, __d, __e) \
	struct si_module_decl simodule_decl = {			\
				SIMODULE_CUR_VERSION,				\
				SIMODULE_COMPAT_VERSION,			\
				__a,								\
				__b,								\
				__c,								\
				__d,								\
				__e									\
			}

int simodule_init(void);
void simodule_deinit(void);
int simodule_load(char *name, char *arg, int debug, const char **errstr);
int simodule_unload(char *name, const char **errstr);

int simodule_data_add(void *modhandle, void *addr, void *data);
int simodule_data_get(void *modhandle, void *addr, void **dataptr);
int simodule_data_del(void *modhandle, void *addr);

int simodule_add_client(void *modhandle, SI_MOD_CLIENT *clist);
void simodule_del_client(void *modhandle, SI_MOD_CLIENT *cptr);
void simodule_client_added(SI_CLIENT *cptr);
void simodule_client_removed(SI_CLIENT *cptr, char *comment);
void simodule_channel_added(SI_CHANNEL *chptr);
void simodule_channel_removed(SI_CHANNEL *chptr);
void simodule_nick_changed(SI_CLIENT *cptr, char *old_nick);
void simodule_umode_changed(SI_CLIENT *cptr, u_int old_modes);
void simodule_chmode_changed(SI_CHANNEL *chptr, u_int old_modes, char *old_key, int old_limit);
void simodule_topic_changed(SI_CHANNEL *chptr, char *old_topic, char *old_topic_who, time_t old_topic_time);
void simodule_channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
void simodule_channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
void simodule_channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes);
void simodule_channel_ban_added(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
void simodule_channel_ban_removed(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
void simodule_sjoin_done(SI_CHANNEL *chptr);
int simodule_process_message(SI_CLIENT *from, SI_CLIENT *to, char *args, int not);
void simodule_debug_printf(void *modhandle, int level, char *format, ...);

#endif


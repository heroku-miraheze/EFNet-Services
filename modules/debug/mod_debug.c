/*
**   IRC services debug module -- mod_debug.c
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

#include "services.h"

RCSID("$Id: mod_debug.c,v 1.2 2004/03/30 22:23:46 cbehrens Exp $");

static void				*SIModHandle;
static SI_MEMPOOL		SIDMemPool;


static void _client_added(SI_CLIENT *cptr);
static void _server_added(SI_CLIENT *cptr);
static void _client_removed(SI_CLIENT *cptr, char *comment);
static void _server_removed(SI_CLIENT *cptr, char *comment);
static void _channel_added(SI_CHANNEL *chptr);
static void _channel_removed(SI_CHANNEL *chptr);
static void _nick_changed(SI_CLIENT *cptr, char *old_nick);
static void _umode_changed(SI_CLIENT *cptr, u_int old_modes);
static void _chmode_changed(SI_CHANNEL *chptr, u_int old_modes, char *old_key, u_int old_limit);
static void _topic_changed(SI_CHANNEL *chptr, char *old_topic, char *old_topic_who, time_t old_topic_time);
static void _channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
static void _channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
static void _channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes);
static void _channel_ban_added(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
static void _channel_ban_removed(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban);
static void _sjoin_done(SI_CHANNEL *chptr);
static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);


static SI_MOD_CALLBACKS		SIDCallbacks = {
	_client_added,
	_server_added,
	_client_removed,
	_server_removed,
	_channel_added,
	_channel_removed,
	_nick_changed,
	_umode_changed,
	_chmode_changed,
	_topic_changed,
	_channel_client_added,
	_channel_client_removed,
	_channel_clmode_changed,
	_channel_ban_added,
	_channel_ban_removed,
	_sjoin_done
};

SI_MODULE_DECL("Debug module",
		&SIDMemPool,
		&SIDCallbacks,
		_simodule_init,
		_simodule_deinit
	);
   

static int _simodule_init(void *modhandle, char *args)
{
	SIModHandle = modhandle;
	return 0;
}

static void _simodule_deinit(void)
{
}

static void _client_added(SI_CLIENT *cptr)
{
	printf("client_added(): %s!%s\n",
			cptr->c_name, cptr->c_userhost);
}

static void _server_added(SI_CLIENT *cptr)
{
	printf("server_added(): %s\n", cptr->c_name);
}

static void _client_removed(SI_CLIENT *cptr, char *comment)
{
	printf("client_removed(): %s!%s: %s\n",
			cptr->c_name, cptr->c_userhost,
			comment ? comment : "");
}

static void _server_removed(SI_CLIENT *cptr, char *comment)
{
	printf("server_removed(): %s: %s\n",
			cptr->c_name, comment?comment:"");
}

static void _channel_added(SI_CHANNEL *chptr)
{
	printf("channel_added(): %s -- %d users\n",
			chptr->ch_name, chptr->ch_numclients);
}

static void _channel_removed(SI_CHANNEL *chptr)
{
	printf("channel_removed(): %s\n", chptr->ch_name);
}

static void _nick_changed(SI_CLIENT *cptr, char *old_nick)
{
	printf("nick_changed(): %s -> %s\n", old_nick, cptr->c_name);
}

static void _umode_changed(SI_CLIENT *cptr, u_int old_modes)
{
	printf("umode_changed(): %s: %u -> %u\n",
		cptr->c_name, old_modes, cptr->c_umodes);
}

static void _chmode_changed(SI_CHANNEL *chptr, u_int old_modes, char *old_key, u_int old_limit)
{
	printf("chmode_changed(): %s: %u -> %u, %s -> %s, %d -> %d\n",
		chptr->ch_name, old_modes, chptr->ch_modes,
		old_key?old_key:"<NULL>",
		chptr->ch_key?chptr->ch_key:"<NULL>",
		old_limit, chptr->ch_limit);
}

static void _topic_changed(SI_CHANNEL *chptr, char *old_topic, char *old_topic_who, time_t old_topic_time)
{
	printf("topic_changed(): %s: %s -> %s\n",
		chptr->ch_name,
		old_topic?old_topic:"",chptr->ch_topic?chptr->ch_topic:"");
}

static void _channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	printf("channel_client_added(): %s <- %s\n",
			chptr->ch_name, cptr->c_name);
}

static void _channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	printf("channel_client_removed(): %s <- %s\n",
			chptr->ch_name, cptr->c_name);
}

static void _channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes)
{
	printf("channel_clmode_changed(): %s -- %s: %u -> %u\n",
			chptr->ch_name, clptr->cl_cptr->c_name,
			old_modes, clptr->cl_modes);
}

static void _channel_ban_added(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban)
{
	printf("channel_ban_added(): %s -- %s by %s",
		chptr->ch_name, ban->b_string, from->c_name);
}

static void _channel_ban_removed(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban)
{
	printf("channel_ban_added(): %s -- %s by %s",
		chptr->ch_name, ban->b_string, from?from->c_name:"<channel going away>");
}

static void _sjoin_done(SI_CHANNEL *chptr)
{
	printf("sjoin_done(): %s\n", chptr->ch_name);
}


int moddebug_func(char *string)
{
	printf("moddebug_func(): %s\n", string?string:"<NULL>");
	return 69;
}

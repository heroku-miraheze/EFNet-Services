/*
**   IRC services -- hash.h
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

#ifndef __HASH_H__
#define __HASH_H__

/*
** $Id: hash.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <sys/poll.h>
#include <sys/types.h>
#include "services.h"
#include "conn.h"

#define SI_SERVER_HASHSIZE	127
#define SI_CLIENT_HASHSIZE	((128*1024)-1)
#define SI_CHANNEL_HASHSIZE	((48*1024)-1)
#define SI_CLIENT_NAME_MAXSIZE		512

typedef struct _si_ban			SI_BAN;
typedef struct _si_client   	SI_CLIENT;
typedef struct _si_clientlist	SI_CLIENTLIST;
typedef struct _si_server   	SI_SERVER;
typedef struct _si_service		SI_SERVICE;
typedef struct _si_channel		SI_CHANNEL;
typedef struct _si_channellist	SI_CHANNELLIST;


struct _si_ban
{
	char			*b_string;
	char			*b_who;
	u_int			b_time;

	SI_BAN			*b_prev;
	SI_BAN			*b_next;
};

#define Con2Client(__x)		((SI_CLIENT *)siconn_arg(__x))
struct _si_client
{
	SI_CONN			*c_conn;	/* NULL if remote */
	SI_CONN			*c_from;
	SI_CLIENT		*c_parent;

	SI_CHANNELLIST	*c_channels;
	u_int			c_numchannels;

	u_int			c_hashv;
	u_int			c_hv;
	char			*c_name;
	char			*c_info;
	char			*c_username;
	char			*c_userhost;
	char			*c_hostname;	/* don't free, ptr to userhost */
	char			*c_servername;

	u_int			c_creation;

#define SI_CLIENT_TYPE_CLIENT		0
#define SI_CLIENT_TYPE_SERVER		1
#define SI_CLIENT_TYPE_SERVICE		2
#define CliIsClient(__x)		((__x)->c_type == SI_CLIENT_TYPE_CLIENT)
#define CliIsServer(__x)		((__x)->c_type == SI_CLIENT_TYPE_SERVER)
#define CliIsService(__x)		((__x)->c_type == SI_CLIENT_TYPE_SERVICE)
	u_char			c_type;
#define SI_CLIENT_FLAGS_HOSTMASK	0x001
#define SI_CLIENT_FLAGS_DOESTS		0x002
#define SI_CLIENT_FLAGS_JUPE		0x004
#define SI_CLIENT_FLAGS_DELJUPE		0x008
#define SI_CLIENT_FLAGS_LOCAL		0x010
#define SI_CLIENT_FLAGS_SENTEOB		0x020
#define SI_CLIENT_FLAGS_GOTEOB		0x040
#define SI_CLIENT_FLAGS_FROMSPLIT	0x080
#define CliSetHostMask(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_HOSTMASK)
#define CliIsHostMask(__x)			((__x)->c_flags & SI_CLIENT_FLAGS_HOSTMASK)
#define CliSetDoesTS(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_DOESTS)
#define CliDoesTS(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_DOESTS)
#define CliSetJupe(__x)				((__x)->c_flags |= SI_CLIENT_FLAGS_JUPE)
#define CliIsJupe(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_JUPE)
#define CliSetDelJupe(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_DELJUPE)
#define CliDelJupe(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_DELJUPE)
#define CliSetLocal(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_LOCAL)
#define CliIsLocal(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_LOCAL)
#define CliSetSentEOB(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_SENTEOB)
#define CliSentEOB(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_SENTEOB)
#define CliSetGotEOB(__x)			((__x)->c_flags |= SI_CLIENT_FLAGS_GOTEOB)
#define CliGotEOB(__x)				((__x)->c_flags & SI_CLIENT_FLAGS_GOTEOB)
#define CliSetFromSplit(__x)		((__x)->c_flags |= SI_CLIENT_FLAGS_FROMSPLIT)
#define CliIsFromSplit(__x)			((__x)->c_flags & SI_CLIENT_FLAGS_FROMSPLIT)
	u_int			c_flags;
	u_int			c_hopcount;
#define CliIsOper(__x)				((__x)->c_umodes & SI_CLIENT_UMODE_OPER)
#define SI_CLIENT_UMODE_OPER		0x001
#define SI_CLIENT_UMODE_LOCOP		0x002
#define SI_CLIENT_UMODE_IMODE		0x004
#define SI_CLIENT_UMODE_SMODE		0x010
#define SI_CLIENT_UMODE_WMODE		0x020
#define SI_CLIENT_UMODE_AMODE		0x040
#define SI_CLIENT_UMODES_REMOTE		\
			(SI_CLIENT_UMODE_OPER|	\
			SI_CLIENT_UMODE_WMODE|	\
			SI_CLIENT_UMODE_IMODE|	\
			SI_CLIENT_UMODE_AMODE)
	u_int			c_umodes;

	SI_CLIENT		*c_clients;		/* children if server */
	SI_CLIENT		*c_servers;		/* children if server */

	SI_CLIENT		*c_o_prev;		/* Opers */
	SI_CLIENT		*c_o_next;		/* Opers */

	SI_CLIENT		*c_cs_prev;		/* ->c_clients/->c_servers list */
	SI_CLIENT		*c_cs_next;		/* ->c_clients/->c_servers list */

	SI_CLIENT		**c_a_bucket;
	SI_CLIENT		*c_a_prev;		/* Servers, Clients, etc */
	SI_CLIENT		*c_a_next;		/* Servers, Clients, etc */

	SI_CLIENT		*c_h_prev;
	SI_CLIENT		*c_h_next;
};

struct _si_clientlist
{
	SI_CLIENT		*cl_cptr;
#define SI_CHANNEL_MODE_CHANOP		0x001
#define SI_CHANNEL_MODE_VOICE		0x002
#define SI_CHANNEL_MODE_DEOPPED		0x004
	u_int			cl_modes;
	SI_CHANNELLIST	*cl_chlptr;

	SI_CLIENTLIST	*cl_prev;
	SI_CLIENTLIST	*cl_next;
};

struct _si_channel
{
	u_int			ch_hashv;
	u_int			ch_hv;

	u_int			ch_creation;

#define SI_CHANNEL_MODE_INVITEONLY	0x001
#define SI_CHANNEL_MODE_MODERATED	0x002
#define SI_CHANNEL_MODE_NOPRIVMSGS	0x004
#define SI_CHANNEL_MODE_PRIVATE		0x008
#define SI_CHANNEL_MODE_SECRET		0x010
#define SI_CHANNEL_MODE_TOPICLIMIT	0x020
	u_int			ch_modes;
	int				ch_limit;
	char			*ch_key;

	char			*ch_topic;
	char			*ch_topic_who;
	time_t			ch_topic_time;

	u_int			ch_numclients;
	SI_CLIENTLIST	*ch_ops;
	SI_CLIENTLIST	*ch_non_ops;

	u_int			ch_numbans;
	SI_BAN			*ch_bans;

	SI_CHANNEL		*ch_h_prev;
	SI_CHANNEL		*ch_h_next;

	SI_CHANNEL		*ch_a_prev;
	SI_CHANNEL		*ch_a_next;

	char			ch_name[1];
};

struct _si_channellist
{
	SI_CHANNEL		*cl_chptr;
	u_int			cl_modes;
	SI_CLIENTLIST	*cl_clptr;
	SI_CHANNELLIST	*cl_prev;
	SI_CHANNELLIST	*cl_next;
};

u_int sihash_compute_client(char *name);
u_int sihash_compute_channel(char *chname);
void siclient_add(SI_CLIENT *from, SI_CLIENT *cptr);
SI_CLIENT *siclient_find(char *name);
SI_CLIENT *siclient_find_incl_hm(char *name);
SI_CLIENT *siclient_find_match_server(char *name);
SI_CHANNELLIST *siclient_find_channel(SI_CLIENT *cptr, SI_CHANNEL *chptr);
SI_CHANNEL *sichannel_find(char *name);
void sichannel_add(SI_CHANNEL *chptr);
void sichannel_del(SI_CHANNEL *chptr);
void siclient_del_channels(SI_CLIENT *pptr);
void siclient_free(SI_CLIENT *cptr, char *comment);
int sichannel_add_user(SI_CHANNEL *chptr, SI_CLIENT *cptr, u_int flags, u_int chan_is_new);
void sichannel_del_user(SI_CHANNEL *chptr, SI_CLIENT *pptr, SI_CHANNELLIST *chlptr);
int sichannel_add_ban(SI_CHANNEL *chptr, SI_CLIENT *from, char *string);
void sichannel_del_ban(SI_CHANNEL *chptr, SI_CLIENT *from, char *string);
void siclient_add_oper(SI_CLIENT *cptr);
void siclient_del_oper(SI_CLIENT *cptr);
void sihash_init(char *myname, char *myinfo);
int siclient_send_privmsg(SI_CLIENT *from, SI_CLIENT *to, char *format, ...);
void siclient_send_jupe(SI_CLIENT *cptr);
void siclient_send_jupes(void);
int siclient_change_nick(SI_CLIENT *cptr, char *nick);
int siclient_is_me(SI_CLIENT *cptr);
void siuplink_drop(void);
int siuplink_printf(char *format, ...);
char *siuplink_name(void);
int siuplink_wallops_printf(SI_CLIENT *from, char *format, ...);
int siuplink_opers_printf(SI_CLIENT *from, char *format, ...);

extern char			*SIMyName;
extern char 		*SIMyInfo;
extern SI_CLIENT	*SIMe;

#endif

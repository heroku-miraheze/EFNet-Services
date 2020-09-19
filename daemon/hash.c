/*
**   IRC services -- hash.c
**   Copyright (C) 2001-2004 Chris Behrens
**
**   hash functions ripped from ircd --
**         Copyright (C) 1990 Jarkko Oikarinen and
**                      University of Oulu, Computing Center
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

RCSID("$Id: hash.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

static SI_CLIENT		*CLIENT_HASH[SI_CLIENT_HASHSIZE+1];
static SI_CHANNEL		*CHANNEL_HASH[SI_CHANNEL_HASHSIZE];
static u_int			*_hashtab = NULL;
static SI_CLIENT		*SIUplink = NULL;
static SI_CLIENT		SIMeBuf;

char					*SIMyName = NULL;
char					*SIMyInfo = NULL;
SI_CLIENT				*SIMe = NULL;
SI_CLIENT				*SIClients = NULL;
SI_CLIENT				*SIOpers = NULL;
SI_CLIENT				*SIServers = NULL;
SI_CHANNEL				*SIChannels = NULL;

static void _siclient_hash_add(SI_CLIENT *cptr);
static void _siclient_hash_del(SI_CLIENT *cptr);


u_int sihash_compute_client(char *nname)
{
	u_char			*name = (u_char *)nname;
	u_int			hash = 1;

	for(;*name;name++)
	{
		hash <<= 1;
		hash += _hashtab[(u_int)*name];
	}
	return hash;
}

u_int sihash_compute_channel(char *cname)
{
	u_char			*name = (u_char *)cname;
	u_int			hash = 1;
	int				i = 20;

	for(;*name && --i;name++)
	{
		hash <<= 1;
		hash += _hashtab[(u_int)*name] + (i << 1);
	}
	return hash;
}

static void _siclient_hash_add(SI_CLIENT *cptr)
{
	cptr->c_hashv = sihash_compute_client(cptr->c_name);
	if (*cptr->c_name == '*')
	{
		cptr->c_hv = SI_CLIENT_HASHSIZE;
		CliSetHostMask(cptr);
	}
	else
		cptr->c_hv = cptr->c_hashv % SI_CLIENT_HASHSIZE;
	si_ll_add_to_list(CLIENT_HASH[cptr->c_hv], cptr, c_h);
}

static void _siclient_hash_del(SI_CLIENT *cptr)
{
	si_ll_del_from_list(CLIENT_HASH[cptr->c_hv], cptr, c_h);
}

void siclient_add(SI_CLIENT *from, SI_CLIENT *cptr)
{
	if (!from && SIMe)
		from = SIMe;

	if (cptr->c_conn)
	{
		SIUplink = cptr;
		CliSetLocal(cptr);
	}
		
	cptr->c_parent = from;
	if (CliIsClient(cptr))
	{
		if (++SIStats.st_numclients > SIStats.st_maxclients)
		{
			SIStats.st_maxclients = SIStats.st_numclients;
			SIStats.st_maxclients_time = Now;
		}
		if (from)
		{
			si_ll_add_to_list(from->c_clients, cptr, c_cs);
		}
		si_ll_add_to_list(SIClients, cptr, c_a);
	}
	else
	{
		if (++SIStats.st_numgservers > SIStats.st_maxgservers)
		{
			SIStats.st_maxgservers = SIStats.st_numgservers;
			SIStats.st_maxgservers_time = Now;
		}
		if (cptr->c_conn)
		{
			if (++SIStats.st_nummservers > SIStats.st_maxmservers)
			{
				SIStats.st_maxmservers = SIStats.st_nummservers;
				SIStats.st_maxmservers_time = Now;
			}
		}
		if (from)
		{
			si_ll_add_to_list(cptr->c_parent->c_servers, cptr, c_cs);
		}
		si_ll_add_to_list(SIServers, cptr, c_a);
	}
	_siclient_hash_add(cptr);
	if (cptr->c_umodes & SI_CLIENT_UMODE_OPER)
	{
		siclient_add_oper(cptr);
	}
	if (cptr->c_umodes & SI_CLIENT_UMODE_IMODE)
	{
		++SIStats.st_numinvis;
	}
#ifdef DEBUG
	printf("Adding client \"%s\"\n", cptr->c_name);
#endif

	if (CliIsJupe(cptr))
	{
		siclient_send_jupe(cptr);
	}

	simodule_client_added(cptr);
}

SI_CLIENT *siclient_find(char *name)
{
	u_int			hashv;
	SI_CLIENT		*cptr;

	hashv = sihash_compute_client(name);
	for(cptr=CLIENT_HASH[hashv % SI_CLIENT_HASHSIZE];
			cptr;
			cptr=cptr->c_h_next)
	{
		if ((cptr->c_hashv == hashv) && !ircd_strcmp(name, cptr->c_name))
			break;
	}
	return cptr;
}

SI_CLIENT *siclient_find_incl_hm(char *name)
{
	SI_CLIENT		*cptr;
	u_int			hashv;

	hashv = sihash_compute_client(name);
	for(cptr=CLIENT_HASH[hashv % SI_CLIENT_HASHSIZE];
		cptr;
		cptr=cptr->c_h_next)
	{
		if ((cptr->c_hashv == hashv) && !ircd_strcmp(name, cptr->c_name))
			return cptr;
	}
	for(cptr=CLIENT_HASH[SI_CLIENT_HASHSIZE];
			cptr;
			cptr=cptr->c_h_next)
	{
		if (!ircd_match(cptr->c_name, name))
			return cptr;
	}
	return NULL;
}

SI_CLIENT *siclient_find_match_server(char *name)
{
	SI_CLIENT		*cptr;

	for(cptr=SIServers;cptr;cptr=cptr->c_a_next)
	{
		if (!ircd_match(name, cptr->c_name))
			break;
	}
	return cptr;
}

SI_CHANNELLIST *siclient_find_channel(SI_CLIENT *cptr, SI_CHANNEL *chptr)
{
	SI_CHANNELLIST	*chlptr;

	for(chlptr=cptr->c_channels;chlptr;chlptr=chlptr->cl_next)
	{
		if (chlptr->cl_chptr == chptr)
			break;
	}

	return chlptr;
}

void siclient_del_channels(SI_CLIENT *pptr)
{
	while(pptr->c_channels)
	{
		assert(pptr->c_numchannels);
		sichannel_del_user(pptr->c_channels->cl_chptr, pptr, pptr->c_channels);
	}
	assert(!pptr->c_numchannels);
}

void siclient_free(SI_CLIENT *cptr, char *comment)
{
	if (cptr->c_conn)
	{
		siconn_del(cptr->c_conn, comment);
		return;
	}

	if (cptr == SIUplink)
		SIUplink = NULL;

	siclient_del_channels(cptr);

	simodule_client_removed(cptr, comment);

	_siclient_hash_del(cptr);

	if (CliIsServer(cptr) && CliSentEOB(cptr) && !CliGotEOB(cptr))
	{
		SIStats.st_serversbursting--;
	}

	if (CliIsJupe(cptr))
	{
		if (CliIsServer(cptr))
		{
			if (!CliDelJupe(cptr))
			{
				if (SIUplink)
					siclient_send_jupe(cptr);
				return;
			}
			else
				siuplink_printf("SQUIT %s :%s", cptr->c_name, comment);
		}
		else if (CliIsClient(cptr))
		{
			if (!CliDelJupe(cptr))
			{
				siclient_send_jupe(cptr);
				return;
			}
			else
				siuplink_printf(":%s QUIT :%s", cptr->c_name, comment);
		}
	}

#ifdef DEBUG
	printf("Freeing client \"%s\"\n", cptr->c_name);
#endif

	if (CliIsClient(cptr))
	{
		--SIStats.st_numclients;
		if (cptr->c_parent)
		{
			si_ll_del_from_list(cptr->c_parent->c_clients, cptr, c_cs);
		}
		si_ll_del_from_list(SIClients, cptr, c_a);
	}
	else
	{
		while(cptr->c_servers)
		{
			CliSetFromSplit(cptr->c_servers);
			siclient_free(cptr->c_servers, comment);
		}
		while(cptr->c_clients)
		{
			CliSetFromSplit(cptr->c_clients);
			siclient_free(cptr->c_clients, comment);
		}

		--SIStats.st_numgservers;
		if (CliIsLocal(cptr))
			--SIStats.st_nummservers;
		if (cptr->c_parent)
		{
			si_ll_del_from_list(cptr->c_parent->c_servers, cptr, c_cs);
		}
		si_ll_del_from_list(SIServers, cptr, c_a);
	}

	if (cptr->c_umodes & SI_CLIENT_UMODE_OPER)
	{
		siclient_del_oper(cptr);
	}
	if (cptr->c_umodes & SI_CLIENT_UMODE_IMODE)
	{
		--SIStats.st_numinvis;
	}

	if (cptr->c_servername)
		simempool_free(cptr->c_servername);
	if (cptr->c_userhost)
		simempool_free(cptr->c_userhost);
	if (cptr->c_username)
		simempool_free(cptr->c_username);
	if (cptr->c_info)
		simempool_free(cptr->c_info);
	if (cptr->c_name)
		simempool_free(cptr->c_name);
	simempool_free(cptr);
}

SI_CHANNEL *sichannel_find(char *name)
{
	SI_CHANNEL	*chptr;
	u_int		hashv;

	hashv = sihash_compute_channel(name);
	for(chptr=CHANNEL_HASH[hashv % SI_CHANNEL_HASHSIZE];
			chptr;
			chptr=chptr->ch_h_next)
	{
		if ((chptr->ch_hashv == hashv) && !ircd_strcmp(name, chptr->ch_name))
			break;
	}
	return chptr;
}


void sichannel_add(SI_CHANNEL *chptr)
{
	chptr->ch_hashv = sihash_compute_channel(chptr->ch_name);
	chptr->ch_hv = chptr->ch_hashv % SI_CHANNEL_HASHSIZE;

	si_ll_add_to_list(CHANNEL_HASH[chptr->ch_hv], chptr, ch_h);
	si_ll_add_to_list(SIChannels, chptr, ch_a);

	if (++SIStats.st_numchannels > SIStats.st_maxchannels)
	{
		SIStats.st_maxchannels = SIStats.st_numchannels;
		SIStats.st_maxchannels_time = Now;
	}
	simodule_channel_added(chptr);
}

void sichannel_del(SI_CHANNEL *chptr)
{
	si_ll_del_from_list(CHANNEL_HASH[chptr->ch_hv], chptr, ch_h);
	si_ll_del_from_list(SIChannels, chptr, ch_a);

	--SIStats.st_numchannels;

	sichannel_del_ban(chptr, NULL, NULL);

	assert(!chptr->ch_numclients);
	assert(!chptr->ch_numbans);

	simodule_channel_removed(chptr);

	if (chptr->ch_topic)
	{
		simempool_free(chptr->ch_topic);
	}
	if (chptr->ch_topic_who)
	{
		simempool_free(chptr->ch_topic_who);
	}
	if (chptr->ch_key)
	{
		simempool_free(chptr->ch_key);
	}

	simempool_free(chptr);
}

int sichannel_add_user(SI_CHANNEL *chptr, SI_CLIENT *cptr, u_int modes, u_int chan_is_new)
{
	SI_CHANNELLIST	*chlptr;
	SI_CLIENTLIST	*clptr;

	chlptr = (SI_CHANNELLIST *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CHANNELLIST));
	if (!chlptr)
		return ENOMEM;
	clptr = (SI_CLIENTLIST *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CLIENTLIST));
	if (!clptr)
	{
		simempool_free(chlptr);
		return ENOMEM;
	}
	chlptr->cl_chptr = chptr;
	clptr->cl_cptr = cptr;
	clptr->cl_modes = modes;

	si_ll_add_to_list(cptr->c_channels, chlptr, cl);
	if (modes & SI_CHANNEL_MODE_CHANOP)
	{
		si_ll_add_to_list(chptr->ch_ops, clptr, cl);
	}
	else
	{
		si_ll_add_to_list(chptr->ch_non_ops, clptr, cl);
	}

	chlptr->cl_clptr = clptr;
	clptr->cl_chlptr = chlptr;
	cptr->c_numchannels++;
	chptr->ch_numclients++;

	if (!chan_is_new)
	{
		simodule_channel_client_added(chptr, cptr, clptr);
	}

	return 0;
}

void sichannel_del_user(SI_CHANNEL *chptr, SI_CLIENT *pptr, SI_CHANNELLIST *chlptr)
{
	SI_CLIENTLIST	*clptr;

	if (!chlptr && (!(chlptr = siclient_find_channel(pptr, chptr))))
		return;
#ifdef DEBUG
	printf("Removed \"%s\" from \"%s\"\n", pptr->c_name, chptr->ch_name);
#endif

	clptr = chlptr->cl_clptr;

	if (clptr->cl_modes & SI_CHANNEL_MODE_CHANOP)
	{
		si_ll_del_from_list(chptr->ch_ops, clptr, cl);
	}
	else
	{
		si_ll_del_from_list(chptr->ch_non_ops, clptr, cl);
	}

	simodule_channel_client_removed(chptr, pptr, clptr);

	if (!--chptr->ch_numclients)
	{
		sichannel_del(chptr);
	}

	si_ll_del_from_list(pptr->c_channels, chlptr, cl);
	pptr->c_numchannels--;

	simempool_free(clptr);
	simempool_free(chlptr);
}

int sichannel_add_ban(SI_CHANNEL *chptr, SI_CLIENT *from, char *string)
{
	SI_BAN	*ban;

	for(ban=chptr->ch_bans;ban;ban=ban->b_next)
	{
		if (!ircd_strcmp(ban->b_string, string))
			break;
	}
	if (ban)
		return 0;
	ban = (SI_BAN *)simempool_alloc(&SIMainPool, sizeof(SI_BAN));
	if (!ban)
		return ENOMEM;
	ban->b_string = simempool_strdup(&SIMainPool, string);
	if (!ban->b_string)
	{
		simempool_free(ban);
		return ENOMEM;
	}
	ban->b_time = Now;
	if (CliIsServer(from))
	{
		ban->b_who = simempool_strdup(&SIMainPool, from->c_name);
	}
	else
	{
		u_int		nlen = strlen(from->c_name);
		u_int		uhlen = strlen(from->c_userhost);

		ban->b_who = (char *)simempool_alloc(&SIMainPool,
								nlen+uhlen+2);
		if (ban->b_who)
		{
			strcpy(ban->b_who, from->c_name);
			ban->b_who[nlen] = '!';
			strcpy(ban->b_who+nlen+1, from->c_userhost);
		}
	}
	si_ll_add_to_list(chptr->ch_bans, ban, b);
	chptr->ch_numbans++;
	simodule_channel_ban_added(chptr, from, ban);
	return 0;
}

void sichannel_del_ban(SI_CHANNEL *chptr, SI_CLIENT *from, char *string)
{
	SI_BAN	*ban;
	SI_BAN	*nextban;

	for(ban=chptr->ch_bans;ban;ban=nextban)
	{
		nextban = ban->b_next;
		if (!string || !ircd_strcmp(string, ban->b_string))
		{
			si_ll_del_from_list(chptr->ch_bans, ban, b);
			chptr->ch_numbans--;
			simodule_channel_ban_removed(chptr, from, ban);
			simempool_free(ban->b_string);
			if (ban->b_who)
				simempool_free(ban->b_who);
			simempool_free(ban);
		}
	}
}

void siclient_add_oper(SI_CLIENT *cptr)
{
#ifdef DEBUG
	printf("Adding oper \"%s\"\n", cptr->c_name);
#endif
	si_ll_add_to_list(SIOpers, cptr, c_o);
	++SIStats.st_numopers;
}

void siclient_del_oper(SI_CLIENT *cptr) 
{
#ifdef DEBUG
	printf("Removing oper \"%s\"\n", cptr->c_name);
#endif
	si_ll_del_from_list(SIOpers, cptr, c_o);
	--SIStats.st_numopers;
}

void sihash_init(char *myname, char *myinfo)
{
	int		i;

	_hashtab = (u_int *)simempool_alloc(&SIMainPool, 256 * sizeof(u_int));
	for (i = 0; i < 256; i++)
		_hashtab[i] = tolower((char)i) * 109;
	memset(CLIENT_HASH, 0, sizeof(CLIENT_HASH));
	memset(CHANNEL_HASH, 0, sizeof(CHANNEL_HASH));
	memset(&SIMeBuf, 0, sizeof(SIMeBuf));
	SIMyName = SIMeBuf.c_name = myname;
	SIMyInfo = SIMeBuf.c_info = myinfo;
	siclient_add(NULL, &SIMeBuf);
	SIMe = &SIMeBuf;
}

int siclient_send_privmsg(SI_CLIENT *from, SI_CLIENT *to, char *format, ...)
{
	int		err;
	char	buffer[SIMAXPACKET];
	va_list	ap;

	if (!from || !to || !format)
		return EINVAL;
	va_start(ap, format);
	err = vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	if (err < 0)
		return errno ? errno : err;
	buffer[sizeof(buffer)-1] = '\0';
	return siconn_printf(to->c_from, ":%s PRIVMSG %s :%s",
			from->c_name, to->c_name, buffer);
}

SI_CLIENT *siclient_get_uplink(void)
{
	SI_CLIENT	*cptr = SIMe->c_servers;

	while(cptr && !cptr->c_conn)
		cptr = cptr->c_cs_next;
	return cptr;
}

void siclient_send_jupe(SI_CLIENT *cptr)
{
	SI_CONN	*conn;

	if (!SIUplink)
		return;
	conn = SIUplink->c_conn;

	if (CliIsClient(cptr))
	{
		siconn_printf(conn, ":%s KILL %s :Nickname in use by service.",
			SIMe->c_name, cptr->c_name);
		siconn_printf(conn, "NICK %s %d %i +%s %s %s %s :%s",
			cptr->c_name, cptr->c_hopcount+1,
			cptr->c_creation,
			umode_build(cptr),
			cptr->c_username,
			cptr->c_hostname,
			cptr->c_servername,
			cptr->c_info);
	}
	else if (CliIsServer(cptr))
	{
		siconn_printf(conn, ":%s SQUIT %s :Server is jupitered.",
			SIMe->c_name, cptr->c_name);
		siconn_printf(conn, "SQUIT %s :Server is jupitered.",
			cptr->c_name);
		siconn_printf(conn, ":%s SERVER %s :%s",
			SIMe->c_name,
			cptr->c_name,
			cptr->c_info);
	}
}

void siclient_send_jupes(void)
{
	SI_CLIENT	*cptr;

	for(cptr=SIMe->c_servers;cptr;cptr=cptr->c_cs_next)
	{
		if (CliIsJupe(cptr))
		{
			siclient_send_jupe(cptr);
		}
	}
	for(cptr=SIMe->c_clients;cptr;cptr=cptr->c_cs_next)
	{
		if (CliIsJupe(cptr))
		{
			siclient_send_jupe(cptr);
		}
	}
}

int siclient_change_nick(SI_CLIENT *cptr, char *nick)
{
	char *newname = simempool_strdup(&SIMainPool, nick);

	if (!newname)
		return ENOMEM;
    _siclient_hash_del(cptr);
    simempool_free(cptr->c_name);
    cptr->c_name = newname;
    _siclient_hash_add(cptr);

	return 0;
}

int siclient_is_me(SI_CLIENT *cptr)
{
	return cptr == SIMe;
}

void siuplink_drop(void)
{
	if (SIUplink)
        siconn_del(SIUplink->c_conn, "Leaving");
}

int siuplink_printf(char *format, ...)
{
	va_list		ap;
	int			err = 0;

	if (!SIUplink)
		return ENOENT;
	va_start(ap, format);
	err = siconn_printfv(SIUplink->c_conn, format, ap);
	va_end(ap);
	return err;
}

char *siuplink_name(void)
{
	return SIUplink && SIUplink->c_name ? SIUplink->c_name : NULL;
}

int siuplink_wallops_printf(SI_CLIENT *from, char *format, ...)
{
	int		err;
	char	buffer[SIMAXPACKET];
	va_list	ap;

	if (!format || !SIUplink)
		return EINVAL;
	va_start(ap, format);
	err = vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	if (err < 0)
		return errno ? errno : err;
	buffer[sizeof(buffer)-1] = '\0';
	return siconn_printf(SIUplink->c_conn, ":%s WALLOPS :%s",
			from?from->c_name:SIMyName, buffer);
}

int siuplink_opers_printf(SI_CLIENT *from, char *format, ...)
{
	int			err;
	char		buffer[SIMAXPACKET];
	SI_CLIENT	*cptr;
	va_list		ap;

	if (!format || !SIUplink)
		return EINVAL;
	va_start(ap, format);
	err = vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	if (err < 0)
		return errno ? errno : err;
	buffer[sizeof(buffer)-1] = '\0';

	for(cptr=SIOpers;cptr;cptr=cptr->c_o_next)
	{
		if (!cptr->c_from)
			continue;
		err =siconn_printf(cptr->c_from, ":%s PRIVMSG %s :%s",
			from?from->c_name:SIMyName, cptr->c_name, buffer);
		if (err)
			return err;
	}
	return 0;
}

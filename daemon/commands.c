/*
**   IRC services -- commands.c
**   Copyright (C) 2001-2004 Chris Behrens
**
**   some stuff ripped from ircd --
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

RCSID("$Id: commands.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");


typedef struct _commandentry	COMMANDENTRY;
typedef struct _si_capab		SI_CAPAB;

struct _commandentry
{
	COMMAND		 *mptr;
	COMMANDENTRY	*next[27];
};

static COMMANDENTRY *_commandtree[27];

struct _si_capab
{				 
	u_int				value;
	char				*name;
	u_int				flags;
};

static char *months[] =
{
	"January",
	"February",
	"March",
	"April",
    "May",
	"June",
	"July",
	"August",
    "September",
	"October",
	"November",
	"December"
};

static char *weekdays[] =
{
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
    "Thursday",
	"Friday",
	"Saturday"
};

static SI_CAPAB	_si_capabs[] =
{
	{	SI_CAPAB_VALUE_NOQUITSTORM,	"QS",	0 },
	{	0, NULL, 0 }
};

static int _user_modes[] =
{
	SI_CLIENT_UMODE_OPER,		'o',
	SI_CLIENT_UMODE_LOCOP,		'O',
	SI_CLIENT_UMODE_IMODE,		'i',
	SI_CLIENT_UMODE_SMODE,		's',
	SI_CLIENT_UMODE_WMODE,		'w',
	SI_CLIENT_UMODE_AMODE,		'a',
	0, 0
};

static int _channel_modes[] =
{
	SI_CHANNEL_MODE_PRIVATE,	'p',
	SI_CHANNEL_MODE_SECRET,		's',
	SI_CHANNEL_MODE_MODERATED,	'm',
	SI_CHANNEL_MODE_NOPRIVMSGS,	'n',
	SI_CHANNEL_MODE_TOPICLIMIT,	't',
	SI_CHANNEL_MODE_INVITEONLY,	'i',
	0, 0
};




static int _command_quit(SI_CONN *lptr, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_pass(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_privmsg(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_notice(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_whois(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_topic(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_version(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_squit(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_info(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_stats(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_error(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_away(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_connect(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_ping(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_pong(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_wallops(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_operwall(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_time(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_admin(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_trace(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_join(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_part(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_lusers(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_motd(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_mode(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_kick(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_svinfo(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_capab(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_kill(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_user(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_nick(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_sjoin(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);
static int _command_server(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args);


static COMMAND  _Commands[] =
{
	{	"PRIVMSG",	_command_privmsg	},
	{	"NOTICE",	_command_notice		},
	{	"WHOIS",	_command_whois		},
	{	"USER",		_command_user		},
	{	"NICK",		_command_nick		},
	{	"TOPIC",	_command_topic		},
	{	"VERSION",	_command_version	},
	{	"SQUIT",	_command_squit		},
	{	"INFO",		_command_info		},
	{	"STATS",	_command_stats		},
	{	"ERROR",	_command_error		},
	{	"AWAY",		_command_away		},
	{	"CONNECT",	_command_connect	},
	{	"PING",		_command_ping		},
	{	"PONG",		_command_pong		},
	{	"WALLOPS",	_command_wallops	},
	{	"TIME",		_command_time		},
	{	"ADMIN",	_command_admin		},
	{	"TRACE",	_command_trace		},
	{	"JOIN",		_command_join		},
	{	"PART",		_command_part		},
	{	"LUSERS",	_command_lusers		},
	{	"MOTD",		_command_motd		},
	{	"MODE",		_command_mode		},
	{	"KICK",		_command_kick		},
	{	"SVINFO",	_command_svinfo		},
	{	"CAPAB",	_command_capab		},
	{	"OPERWALL",	_command_operwall	},
	{	"KILL",		_command_kill		},
	{	"PASS",		_command_pass		},
	{	"SJOIN",	_command_sjoin		},
	{ 	"SERVER",	_command_server		},
	{	"QUIT",		_command_quit		},
	{   NULL,	   NULL				}
};

static void _command_tree_free(COMMANDENTRY *ce);
static void _command_tree_add(char *name, COMMAND *cptr);
static int _command_umode(SI_CLIENT *lptr, SI_CLIENT *pptr, char *target, char *prefix, char *args);
static int _command_message(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args, int not);
static char *datestr(int thetime, char *buf);


static void _command_tree_free(COMMANDENTRY *ce)
{
	int			 i;
	
	if (!ce)
		return;
	
	for(i=0;i<27;i++)
	{   
		if (ce->next[i])
		{   
			_command_tree_free(ce->next[i]);
			simempool_free(ce->next[i]);
			ce->next[i] = NULL;
		}
	}
}

static void _command_tree_add(char *name, COMMAND *cptr)
{
	char			*ptr = name;
	COMMANDENTRY	*lastentry = NULL;
	COMMANDENTRY	*entry;
	unsigned int	num;
	
	if (!*ptr)
		return;
	do
	{   
		if (!*ptr)
			num = 0;
		else if (islower((int)*ptr))
			num = toupper(*ptr) - 'A' + 1;
		else
			num = *ptr - 'A' + 1;
		if (num >= 27)
			return;
		if (!lastentry)
			entry = _commandtree[num];
		else
			entry = lastentry->next[num];
		if (!entry)
		{   
			entry = (COMMANDENTRY *)simempool_calloc(&SIMainPool, 1, sizeof(COMMANDENTRY));
			if (!lastentry)
				_commandtree[num] = entry;
			else
				lastentry->next[num] = entry;
		}
		lastentry = entry;
	} while(*ptr++);
	if (entry->mptr)
		return;
	entry->mptr = cptr;
}

COMMAND *command_tree_find(char *name)
{
	char			*ptr = name;
	COMMANDENTRY	*lastentry = NULL;
	COMMANDENTRY	*entry;
	unsigned int	num;
	
	if (!*ptr)
		return NULL;
	do
	{   
		if (!*ptr)
			num = 0;
		else if (islower((int)*ptr))
			num = toupper(*ptr) - 'A' + 1;
		else
			num = *ptr - 'A' + 1;
		if (num >= 27)
			return NULL;
		if (!lastentry)
			entry = _commandtree[num];
		else
			entry = lastentry->next[num];
		lastentry = entry;
		if (!entry)
			return NULL;
	} while(*ptr++);
	return entry->mptr;
}

void command_tree_init(void)
{
	COMMAND *cptr = _Commands;
	int	 i;
	
	for(i=0;i<27;i++)
		_commandtree[i] = NULL;
	for(;cptr->command;cptr++)
		_command_tree_add(cptr->command, cptr);
}

void command_tree_deinit(void)
{
	int	i;

	for(i=0;i<27;i++)
		_command_tree_free(_commandtree[i]);
}

void send_capabilities(SI_CONN *con)
{
	SI_CAPAB	*cptr;
	char		buffer[256];
	char		*ptr;
	char		*sptr;
	char		*dptr;

	ptr = dptr = buffer;
	for(cptr=_si_capabs;cptr->name;cptr++)
	{
		if (ptr != buffer)
		{
			*ptr++ = ' ';
			dptr++;
		}
		sptr = cptr->name;
		while(((dptr - buffer) < sizeof(buffer)) && (*dptr = *sptr++))
			dptr++;
		if ((dptr - buffer) == sizeof(buffer))
		{
			*(ptr-1) = '\0'; /* -1 to kill the space that was added */
			siconn_printf(con, "CAPAB :%s", buffer);
			dptr = ptr = buffer;
			sptr = cptr->name;
			while((*dptr = *sptr++))
				dptr++;
		}
		ptr = dptr;
	}
	if (ptr != buffer)
	{
		*ptr = '\0';
		siconn_printf(con, "CAPAB :%s", buffer);
	}
}

char *umode_build(SI_CLIENT *cptr)
{
	static char	buf[sizeof(_user_modes)/sizeof(_user_modes[0])];
	int			*i;
	char		*ptr = buf;

	for(i=_user_modes;*i;i+=2)
	{
		if (cptr->c_umodes & *i & SI_CLIENT_UMODES_REMOTE)
		{
			*ptr++ = *(i+1);
		}
	}
	*ptr = '\0';
	return buf;
}

static int _command_quit(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char	*comment = si_nextarg(&args, 0);

	if (ConIsUnknown(con))
	{
		siconn_del(con, "Goodbye.");
		return -1;
	}
	assert(pptr != NULL);

	if (pptr->c_conn)
	{
		if (pptr->c_conn == con)
		{
			siconn_del(pptr->c_conn, comment);
			return -1;
		}
		siconn_del(pptr->c_conn, comment);
		return 0;
	}

	siclient_free(pptr, comment);

	return 0;
}

static int _command_pass(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*passwd = si_nextarg(&args, 0);
	char		*ts = si_nextarg(&args, 0);
	char		*tsver = si_nextarg(&args, 0);
	char		*servernum = si_nextarg(&args, 0);

	if (!passwd || !*passwd)
	{
		return 0;
	}

	if (!ConIsUnknown(con) || pptr)
	{
		return 0;
	}

	if (con->con_passwd)
		simempool_free(con->con_passwd);
	con->con_passwd = simempool_strdup(&SIMainPool, passwd);
	if (!con->con_passwd)
	{
		siconn_del(con, "Out of memory");
		return -1;
	}

	if (ts && !strcmp(ts, "TS"))
	{
		con->con_flags |= SI_CON_FLAGS_TS;
	}

	return 0;
}

static int _command_message(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args, int not)
{
	char		*nick;
	char		*tostr = si_nextarg(&args, 0);
	char		*message = si_nextarg(&args, 0);
	SI_CHANNEL	*chptr = NULL;
	SI_CLIENT	*nptr = NULL;
	SI_CLIENT	*lptr = Con2Client(con);
	char		*server;

	if (!lptr)
	{
		siconn_del(con, "You must register, first.");
		return -1;
	}

	if (!message || !*message)
	{
		return 0;
	}

	for(;;)
	{
		nick = si_next_subarg_comma(&tostr);
		if (!nick)
			break;
		if (((*nick == '#') || (*nick == '&')) &&
			((chptr = sichannel_find(nick)) != NULL))
		{
			;
		}
		else if ((nptr = siclient_find(nick)) != NULL)
		{
			if (!CliIsJupe(nptr))
				continue;
		}
		else if ((pptr->c_umodes & SI_CLIENT_UMODE_OPER) && ((*nick == '$') || (*nick == '#')))
		{
			char *s;

			if ((s = strrchr(nick, '.')) == NULL)
			{
				if (CliIsClient(pptr))
				siconn_printf(pptr->c_from,
					":%s 413 %s %s :No toplevel domain specified",
					SIMyName, pptr->c_name, nick);
				continue;
			}
			while(*++s)
			{
				if (*s == '*' || *s == '?')
					break;
			}
			if (*s)
			{
				if (CliIsClient(pptr))
				siconn_printf(pptr->c_from,
					":%s 414 %s %s :Wildcard in toplevel Domain",
					SIMyName, pptr->c_name, nick);
				continue;
			}
			/* ignore these for now */
			continue;
		}
		else if (((server = strchr(nick, '@')) != NULL) &&
			((nptr = siclient_find_incl_hm(server+1)) != NULL))
		{
			if (!siclient_is_me(nptr))
			{
				continue;	/* i'm not a HUB, shouldn't happen */
			}
			*server++ = '\0';
			if ((nptr = siclient_find(nick)) == NULL)
			{
				if (CliIsClient(pptr))
					siconn_printf(pptr->c_from,
						":%s 401 %s %s :No such nick/channel",
						SIMyName, pptr->c_name, nick);
				continue;
			}
			if (!CliIsJupe(nptr))
			{
				continue;
			}
		}
		else
		{
			if (CliIsClient(pptr))
				siconn_printf(pptr->c_from,
					":%s 401 %s %s :No such nick/channel",
					SIMyName, pptr->c_name, nick);
		}

		if (chptr)
		{
			SI_CLIENTLIST	*clptr;

			for(clptr=chptr->ch_ops;clptr;clptr=clptr->cl_next)
			{
				if (CliIsJupe(clptr->cl_cptr))
				{
					simodule_process_message(pptr, clptr->cl_cptr, message, not);
				}
			}
			for(clptr=chptr->ch_non_ops;clptr;clptr=clptr->cl_next)
			{
				if (CliIsJupe(clptr->cl_cptr))
				{
					simodule_process_message(pptr, clptr->cl_cptr, message, not);
				}
			}
			continue;
		}
		else if (nptr)
		{
			if (siclient_is_me(nptr))
				continue;
			simodule_process_message(pptr, nptr, message, not);
		}

	}
	return 0;
}

static int _command_privmsg(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return _command_message(con, pptr, prefix, args, 0);
}

static int _command_notice(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return _command_message(con, pptr, prefix, args, 1);
}

static int _command_whois(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_info(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}
static int _command_stats(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_error(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	siconn_del(con, "Got ERROR");
	return -1;
}
static int _command_away(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_connect(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_wallops(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_operwall(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_admin(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_trace(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_lusers(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_motd(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	return 0;
}

static int _command_mode(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	SI_CLIENT		*tptr;
	SI_CHANNEL		*chptr;
	SI_CHANNELLIST	*chlptr;
	char			*channel = si_nextarg(&args, 0);
	char			*modes = si_nextarg(&args, 0);
	char			*modeargs = si_nextarg(&args, SI_NEXTARG_FLAGS_LASTARG);
	char			*ptr;
	char			plusminus = '+';
	int				tlimit;
	int				*i;
	u_int			oldmodes;
	u_int			oldclimodes;
	int				oldlimit;
	char			*oldkey;
	char			*target;
	SI_CLIENT		*lptr = Con2Client(con);

	if (!lptr)
	{
		siconn_del(con, "You must register, first.");
		return -1;
	}

	if (!channel || !*channel)
		return 0;

	chptr = sichannel_find(channel);
	if (!chptr)
	{
		return _command_umode(lptr, pptr, channel, prefix, modes);
	}

	if (!modes || !*modes)
	{
		return 0;
	}

	if (!CliIsServer(pptr) && !siclient_find_channel(pptr, chptr))
	{
		si_log(SI_LOG_NOTICE, "Got MODE for %s from non-member %s: %s %s",
			pptr->c_name, chptr->ch_name,
			modes?modes:"", modeargs?modeargs:"");
	}

	oldmodes = chptr->ch_modes;
	oldlimit = chptr->ch_limit;
	oldkey = chptr->ch_key;

	for(ptr=modes;ptr && *ptr;)
	{
		switch(*ptr)
		{
			case '+':
			case '-':
				plusminus = *ptr;
				break;
			case 'o':
				target = si_next_subarg_space(&modeargs);
				if (!target || !*target)
				{
					break;
				}
				tptr = siclient_find(target);
				if (!tptr)
					break;
				if (!(chlptr = siclient_find_channel(tptr, chptr)))
					break;
				oldclimodes = chlptr->cl_clptr->cl_modes;
				if (plusminus == '+')
				{
					if (!(chlptr->cl_clptr->cl_modes & SI_CHANNEL_MODE_CHANOP))
					{
						si_ll_del_from_list(chptr->ch_non_ops, chlptr->cl_clptr, cl);
						chlptr->cl_clptr->cl_modes |= SI_CHANNEL_MODE_CHANOP;
						si_ll_add_to_list(chptr->ch_ops, chlptr->cl_clptr, cl);
					}
				}
				else if (chlptr->cl_clptr->cl_modes & SI_CHANNEL_MODE_CHANOP)
				{
					si_ll_del_from_list(chptr->ch_ops, chlptr->cl_clptr, cl);
					chlptr->cl_clptr->cl_modes &= ~SI_CHANNEL_MODE_CHANOP;
					si_ll_add_to_list(chptr->ch_non_ops, chlptr->cl_clptr, cl);
				}
				if (oldclimodes != chlptr->cl_clptr->cl_modes)
					simodule_channel_clmode_changed(chptr, chlptr->cl_clptr, oldclimodes);
				break;
			case 'v':
				target = si_next_subarg_space(&modeargs);
				if (!target || !*target)
				{
					break;
				}
				tptr = siclient_find(target);
				if (!tptr)
					break;
				if (!(chlptr = siclient_find_channel(tptr, chptr)))
					break;
				oldclimodes = chlptr->cl_clptr->cl_modes;
				if (plusminus == '+')
					chlptr->cl_clptr->cl_modes |= SI_CHANNEL_MODE_VOICE;
				else
					chlptr->cl_clptr->cl_modes &= ~SI_CHANNEL_MODE_VOICE;
				if (oldclimodes != chlptr->cl_clptr->cl_modes)
					simodule_channel_clmode_changed(chptr, chlptr->cl_clptr, oldclimodes);
				break;
			case 'k':
				target = si_next_subarg_space(&modeargs);
				if (!target || !*target)
				{
					break;
				}
				if (plusminus == '+')
				{
					if (chptr->ch_key && (chptr->ch_key != oldkey))
					{
						/* we got more than one +k, use last one  */
						simempool_free(chptr->ch_key);
					}
					chptr->ch_key = simempool_strdup(&SIMainPool, target);
					if (!chptr->ch_key)
					{
						chptr->ch_key = oldkey;
						siconn_del(con, "Out of memory");
						return -1;
					}
				}
				else
				{
					if (chptr->ch_key && (chptr->ch_key != oldkey))
					{
						/* got +k and -k in same line */
						simempool_free(chptr->ch_key);
					}
					chptr->ch_key = NULL;
				}
				break;
			case 'b':
				target = si_next_subarg_space(&modeargs);
				if (!target || !*target)
				{
					/* return banlist */
					break;
				}
				if (plusminus == '+')
				{
					if (sichannel_add_ban(chptr, pptr, target))
					{
						if (chptr->ch_key && (chptr->ch_key != oldkey))
						{
							simempool_free(chptr->ch_key);
						}
						chptr->ch_key = oldkey;
						siconn_del(con, "Out of memory");
						return -1;
					}
				}
				else
				{
					sichannel_del_ban(chptr, pptr, target);
				}
				break;
			case 'l':
				tlimit = 0;
				if (plusminus == '+')
				{
					target = si_next_subarg_space(&modeargs);
					if (!target || !*target || !(tlimit = atoi(target)))
					{
						break;
					}
				}
				chptr->ch_limit = tlimit;
				break;
			case 'i':
			case 's':
			case 'p':
			case 't':
			case 'n':
			case 'm':
				for(i=_channel_modes;*i;i+=2)
				{
					if (*(i+1) != *ptr)
						continue;
					if (plusminus == '+')
						chptr->ch_modes |= *i;
					else
						chptr->ch_modes &= ~(*i);
				}
				break;
			default:
				break;
		}
		if (!*++ptr)
			ptr = si_next_subarg_space(&modeargs);
	}

	if ((oldmodes != chptr->ch_modes) ||
		(oldlimit != chptr->ch_limit) ||
		(oldkey != chptr->ch_key) ||
		(oldkey && strcasecmp(oldkey, chptr->ch_key)))
	{
		simodule_chmode_changed(chptr, oldmodes, oldkey, oldlimit);
		if (oldkey && (oldkey != chptr->ch_key))
			simempool_free(oldkey);
	}

	return 0;
}

static int _command_svinfo(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*ts_cur = si_nextarg(&args, 0);
	char		*ts_min = si_nextarg(&args, 0);
	char		*standalone = si_nextarg(&args, 0);
	char		*thetime = si_nextarg(&args, 0);
	SI_CLIENT	*lptr  = Con2Client(con);

	if (!lptr)
	{
		siconn_del(con, "You must register, first.");
		return -1;
	}

	if ((pptr != lptr) || !thetime || !*thetime)
	{
		return 0;
	}

	standalone = NULL; /* standalone is not used, and this stops gcc warning */

	/* pptr == lptr */

	if (!ConDoesTS(con))
	{
		siconn_del(con, "non-TS server");
		return -1;
	}

	if (TS_CURRENT < atoi(ts_min) || atoi(ts_cur) < TS_MIN)
	{
		siconn_del(con, "wrong TS protocol");
		return -1;
	}
	return 0;
}

static int _command_capab(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*capab = si_nextarg(&args, SI_NEXTARG_FLAGS_LASTARG);
	char		*ptr;
	SI_CAPAB	*cptr;

	if (!capab || !*capab)
		return 0;
	while((ptr = si_next_subarg_space(&capab)) != NULL)
	{
		for(cptr=_si_capabs;cptr->name;cptr++)
			if (!strcmp(cptr->name, ptr))
				con->con_capab_flags |= cptr->value;
	}
	return 0;
}

static int _command_user(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	siconn_del(con, "Sorry, no clients allowed.");
	return -1;
}

/*
** m_sjoin
** parv[0] - sender
** parv[1] - TS
** parv[2] - channel
** parv[3] - modes + n arguments (key and/or limit)
** parv[4+n] - flags+nick list (all in one parameter)
*/
static int _command_sjoin(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char			*ts = si_nextarg(&args, 0);
	char			*channel = si_nextarg(&args, 0);
	char			*modes = si_nextarg(&args, 0);
	char			*modeargs = args;
	char			*newkey = NULL;
	int				newlimit = 0;
	char			*tempptr;
	char			*ptr;
	int				newts;
	int				mergemodes = 1;
	u_int			newmodes;
	u_int			oldmodes;
	char			*oldkey;
	int				oldlimit;
	int				cr = 0;
	int				err;
	SI_CLIENT		*acptr;
	SI_CHANNEL		*chptr;
	SI_CLIENTLIST	*clptr;
	char			*name;
	int				flags;
	SI_CLIENT		*lptr = Con2Client(con);

	if (!lptr)
	{
		siconn_del(con, "You must register, first.");
		return -1;
	}

	if (!CliIsServer(pptr))
		return 0;

	if (!modeargs || !*modeargs)
		return 0;

	if (*channel == '&')
		return 0;

	newts = atoi(ts);
	chptr = sichannel_find(channel);

	newmodes = 0;

	for(ptr=modes;*ptr;ptr++)
	{
		switch(*ptr)
		{
			case 'i':
				newmodes |= SI_CHANNEL_MODE_INVITEONLY;
				break;
			case 'm':
				newmodes |= SI_CHANNEL_MODE_MODERATED;
				break;
			case 'n':
				newmodes |= SI_CHANNEL_MODE_NOPRIVMSGS;
				break;
			case 'p':
				newmodes |= SI_CHANNEL_MODE_PRIVATE;
				break;
			case 's':
				newmodes |= SI_CHANNEL_MODE_SECRET;
				break;
			case 't':
				newmodes |= SI_CHANNEL_MODE_TOPICLIMIT;
				break;
			case 'k':
				newkey = si_next_subarg_space(&modeargs);
				if (newkey && !*newkey)
					newkey = NULL;
				break;
			case 'l':
				tempptr = si_next_subarg_space(&modeargs);
				if (tempptr)
					newlimit = atoi(tempptr);
				break;
		}
	}	

	mergemodes = 1;
	 
	if (!chptr)
	{
		chptr = (SI_CHANNEL *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CHANNEL)+strlen(channel));
		if (!chptr)
		{
			siconn_del(con, "Out of memory");
			return -1;
		}
		cr = 1;
		strcpy(chptr->ch_name, channel);
		if (++SIStats.st_numchannels > SIStats.st_maxchannels)
		{
			SIStats.st_maxchannels = SIStats.st_numchannels;
			SIStats.st_maxchannels_time = Now;
		}
		chptr->ch_creation = newts;
	}	
	else if (!chptr->ch_numclients)
	{
		mergemodes = 0;	/* take the new modes if we have 0 clients */
		chptr->ch_creation = newts;
	}

	if (modeargs && (*modeargs == ':'))
		modeargs++;

	oldmodes = chptr->ch_modes;
	oldkey = chptr->ch_key;
	oldlimit = chptr->ch_limit;

	if (!newts || !chptr->ch_creation)
	{
		newts = chptr->ch_creation = 0;
	}
	else if (newts < chptr->ch_creation)
	{
		/*
		** TS5 says it doesn't matter if ops are coming in or not
		**  -- we need to remove our current ops
		*/
		int				doesop = ((*modeargs == '@') ||
									(*(modeargs+1) == '@'));
		SI_CLIENTLIST	*lastcl = NULL;
		u_int			oldclimodes;

		clptr = chptr->ch_non_ops;
		if (clptr)
		{
			/* ditch voices from non-ops */
			for(;clptr;
				lastcl=clptr,clptr=clptr->cl_next)
			{
				if (clptr->cl_modes & SI_CHANNEL_MODE_VOICE)
				{
					clptr->cl_modes &= ~SI_CHANNEL_MODE_VOICE;
					simodule_channel_clmode_changed(chptr, clptr,
						clptr->cl_modes & SI_CHANNEL_MODE_VOICE);
				}
			}
			/* put ops list at end of non-ops list */
			if ((lastcl->cl_next = chptr->ch_ops) != NULL)
					chptr->ch_ops->cl_prev = lastcl;
		}
		else
		{
			chptr->ch_non_ops = chptr->ch_ops;
		}
				
		/* go thru old ops now and remove voice/chanop */
		for(clptr=chptr->ch_ops;
				clptr;
				clptr=clptr->cl_next)
		{
			oldclimodes = clptr->cl_modes;
			clptr->cl_modes &= ~(SI_CHANNEL_MODE_VOICE|
						SI_CHANNEL_MODE_CHANOP);

			/*
			**  this if() is because the last clmode_changed needs to
			**   see that there are no more ops in the channel if
			**   if we're not adding any below
			*/
			if (!clptr->cl_next && !doesop)
				chptr->ch_ops = NULL;
			simodule_channel_clmode_changed(chptr, clptr, oldclimodes);
		}

		/*
		** might have been done above already, but let's make sure.
		**  the reason we don't move this up above is that we don't
		**  want clmode_changed() to see no ops when we will add some
		**  back below
		*/
		chptr->ch_ops = NULL;
		chptr->ch_creation = newts;
		mergemodes = 0;
	}
	else if (newts > chptr->ch_creation)
	{
		newmodes = oldmodes;
		newkey = chptr->ch_key;
		newlimit = chptr->ch_limit;
		mergemodes = 0;
	}

	if (mergemodes)
	{
		newmodes |= oldmodes;
		if (chptr->ch_limit > newlimit)
			newlimit = chptr->ch_limit;
		if (strcmp(newkey ? newkey : "", chptr->ch_key ? chptr->ch_key : "") < 0)
			newkey = chptr->ch_key;
	}
	 
	chptr->ch_modes = newmodes;

	if (newkey != chptr->ch_key)
	{
		chptr->ch_key = !newkey ? NULL : simempool_strdup(&SIMainPool, newkey);
	}
	chptr->ch_limit = newlimit;

	for(name = si_next_subarg_space(&modeargs);name;
					name = si_next_subarg_space(&modeargs))
	{
		flags = 0;
		if (*name == '@' || *(name+1) == '@')
			flags |= SI_CHANNEL_MODE_CHANOP;
		if (*name == '+' || *(name+1) == '+')
			flags |= SI_CHANNEL_MODE_VOICE;

		if (flags && ((*++name == '@') || (*name == '+')))
			name++;

		/*
		** if we're keeping our modes (new ts is greater), we
		**  ignore chanop/voice for the remote clients
		*/
		if (!mergemodes && (newts > chptr->ch_creation))
			flags = 0;
		 
		if (!(acptr = siclient_find(name)) || (acptr->c_from != con))
			continue;

		if (!siclient_find_channel(acptr, chptr))
		{
			err = sichannel_add_user(chptr, acptr, flags, cr);
			if (err)
			{
				siconn_del(con, "Out of memory");
				return -1;
			}
#ifdef DEBUG
			printf("Added \"%s\" to channel \"%s\"\n",
					acptr->c_name, chptr->ch_name);
#endif
		}
		else
		{
			si_log(SI_LOG_ERROR,
				"Received SJOIN for client already on channel from %s:%d (%s): \":%s %s\"",
				siconn_ipport(con),
				siconn_name(con),
				pptr->c_name, acptr->c_name);
		}
	}

	if (cr)
	{
		sichannel_add(chptr);
	}
	else if ((oldmodes != chptr->ch_modes) ||
		(oldlimit != chptr->ch_limit) ||
		(oldkey != chptr->ch_key) ||
		(oldkey && !strcasecmp(oldkey, chptr->ch_key)))
	{
		simodule_chmode_changed(chptr, oldmodes, oldkey, oldlimit);
		if (oldkey && (oldkey != chptr->ch_key))
			simempool_free(oldkey);
	}

	simodule_sjoin_done(chptr);
	 
	return 0;
}

static int _command_server(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
/*
** :prefix SERVER servername serverinfo/hopcount serverinfo
*/
	char			*servername = si_nextarg(&args, 0);
	char			*hopcount = si_nextarg(&args, 0);
	char			*serverinfo = si_nextarg(&args, 0);
	char			*ptr;
	int			 	hc = 0;
	SI_CLIENT		*servptr;
	SI_CLIENT		*lptr = Con2Client(con);
	SI_SERVERCONF	*sc;

	if (!servername)
	{
		si_log(SI_LOG_ERROR,
			"Not enough arguments to \"SERVER\" from %s:%d (%s:%s)",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>");
		siconn_del(con, "Received SERVER with no servername");
		return -1;
	}

	if (!serverinfo || !hopcount || !(hc = atoi(hopcount)))
	{
		serverinfo = hopcount;
		hopcount = NULL;
	}

	if (lptr && !CliIsServer(lptr))
	{
		si_log(SI_LOG_ERROR,
			"SERVER not expected from %s:%d (%s:%s)",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>");
		siconn_del(con, "SERVER received when not expected");
		return -1;
	}

	ptr = strchr(servername, '.');
	if (!ptr)
	{
		si_log(SI_LOG_ERROR,
			"Invalid server name from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "Invalid server name received");
		return -1;
	}

	servptr = siclient_find(servername);
	if (servptr != NULL)
	{
		si_log(SI_LOG_ERROR,
			"Server already exists from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "Server exists");
		return -1;
	}

	if (lptr) /* introducing new server behind con/lptr */
	{
		SI_CLIENT		*nservptr;

		if (!CliIsServer(pptr))	/* weird */
		{
			si_log(SI_LOG_ERROR,
				"SERVER from non-server from %s:%d (%s:%s): %s",
				siconn_ipport(con),
				siconn_name(con),
				prefix ? prefix : "<NULL>",
				servername);
			siconn_del(con, "Non-server tried to intruduce server");
		}

		if (!serverinfo)
			serverinfo = "*Unknown*";

		/* check leaf + hub stuff */
		nservptr = (SI_CLIENT *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CLIENT));
		nservptr->c_type = SI_CLIENT_TYPE_SERVER;
		nservptr->c_conn = NULL;
		nservptr->c_from = con;
		nservptr->c_hopcount = hc;
		nservptr->c_name = simempool_strdup(&SIMainPool, servername);
		if (!nservptr->c_name)
		{
			simempool_free(nservptr);
			siconn_del(con, "Out of memory");
			return -1;
		}
		nservptr->c_info = simempool_strdup(&SIMainPool, serverinfo);
		if (!nservptr->c_info)
		{
			simempool_free(nservptr->c_name);
			simempool_free(nservptr);
			siconn_del(con, "Out of memory");
			return -1;
		}

		nservptr->c_creation = Now;
		siclient_add(pptr, nservptr);

		SIStats.st_serversbursting++;
		CliSetSentEOB(nservptr);

		printf("Sending PING to %s\n", nservptr->c_name);
		siconn_printf(con, ":%s PING %s :%s",
					SIMyName, SIMyName, nservptr->c_name);

		return 0;
	}
	 
	if (serverinfo == NULL || !*serverinfo)
	{
		si_log(SI_LOG_ERROR,
			"No server info from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "No server info specified");
		return -1;
	}

	/* new local server */

	if (!*siconn_passwd(con))
	{
		si_log(SI_LOG_ERROR,
			"No password from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "No password given");
		return -1;
	}
	if (!ConDoesTS(con))
	{
		si_log(SI_LOG_ERROR,
			"Non-TS link from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "Non-TS server");
		return -1;
	}

	lptr = (SI_CLIENT *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CLIENT));
	if (!lptr)
	{
		siconn_del(con, "Out of memory");
		return -1;
	}
	lptr->c_type = SI_CLIENT_TYPE_SERVER;
	lptr->c_conn = con;
	lptr->c_from = con;
	con->con_arg = lptr;
	con->con_type = SI_CON_TYPE_CLIENT;
	lptr->c_name = simempool_strdup(&SIMainPool, servername);
	if (!lptr->c_name)
	{
		siconn_del(con, "Out of memory");
		return -1;
	}
	lptr->c_creation = Now;
	siclient_add(NULL, lptr);

	lptr->c_hopcount = hc;

	sc = _siconf_find_server(servername, &con->con_rem_sin);
	if (!sc)
	{
		si_log(SI_LOG_ERROR,
			"Unauthorized link from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "No authorization");
		return -1;
	}
	 
	if (!sc->sc_npasswd || !*sc->sc_npasswd || !*con->con_passwd ||
				strcmp(sc->sc_npasswd, con->con_passwd))
	{
		si_log(SI_LOG_ERROR,
			"Wrong password from %s:%d (%s:%s): %s",
			siconn_ipport(con),
			siconn_name(con),
			prefix ? prefix : "<NULL>",
			servername);
		siconn_del(con, "Wrong password");
		sc->sc_tryconnecttime = 0;
		return -1;
	}

	memset(con->con_passwd, 0, strlen(con->con_passwd));
	simempool_free(con->con_passwd);
	con->con_passwd = NULL;

	if (!(con->con_flags & SI_CON_FLAGS_CONNECTED))
	{
		if (sc->sc_cpasswd && *sc->sc_cpasswd)
			siconn_printf(con, "PASS %s :TS", sc->sc_cpasswd);
		send_capabilities(con);
		siconn_printf(con, "SERVER %s 1 :%s",
			SIMyName, SIMyInfo);
	}

	siconn_printf(con, "SVINFO %d %d %d :%d",
			TS_CURRENT, TS_MIN, 0, Now);

	siclient_send_jupes();

	SIStats.st_serversbursting++;
	CliSetSentEOB(lptr);
	printf("Sending PING to %s\n", lptr->c_name);
	siconn_printf(con, ":%s PING %s :%s",
				SIMyName, SIMyName, lptr->c_name);

	si_log(SI_LOG_NOTICE,
		"Connection established to server %s:%d (%s).",
		siconn_ipport(con),
		siconn_name(con));

	return 0;
}

static int _command_umode(SI_CLIENT *lptr, SI_CLIENT *pptr, char *nick, char *prefix, char *args)
{
	char			*modes = si_nextarg(&args, 0);
	char			plusminus = '+';
	SI_CLIENT		*acptr;
	int				oldmodes;
	char			*ptr;
	int				*i;

	acptr = siclient_find(nick);
	if (!acptr || CliIsServer(acptr))
	{
		return 0;
	}

	if (acptr != pptr)
	{
		return 0;
	}

	if (!modes || !*modes)
	{
		return 0;
	}

	oldmodes = pptr->c_umodes;

	for(ptr=modes;*ptr;ptr++)
	{
		switch(*ptr)
		{
			case '+':
			case '-':
				plusminus = *ptr;
				break;
			default:
				for(i=_user_modes;*i;i+=2)
					if (*ptr == *(i+1))
						break;
				if (!*i)
				{
					continue;
				}
				if (plusminus == '+')
					pptr->c_umodes |= *i;
				else
					pptr->c_umodes &= ~(*i);
				break;
		}
	}

	/* no local clients on here... */
	pptr->c_umodes &= ~SI_CLIENT_UMODE_LOCOP;

	if (pptr->c_umodes == oldmodes)
		return 0;

	if (!(oldmodes & SI_CLIENT_UMODE_OPER) &&
					(pptr->c_umodes & SI_CLIENT_UMODE_OPER))
	{
		if (CliIsServer(lptr))
			siclient_add_oper(pptr);
		else
			pptr->c_umodes &= ~SI_CLIENT_UMODE_OPER;
	}
	else if (!(pptr->c_umodes & SI_CLIENT_UMODE_OPER) &&
				(oldmodes & SI_CLIENT_UMODE_OPER))
	{
		siclient_del_oper(pptr);
	}


	if (!(oldmodes & SI_CLIENT_UMODE_IMODE) &&
			(pptr->c_umodes & SI_CLIENT_UMODE_IMODE))
	{
		++SIStats.st_numinvis;
	}
	else if (!(pptr->c_umodes & SI_CLIENT_UMODE_IMODE) &&
			(oldmodes & SI_CLIENT_UMODE_IMODE))
	{
		--SIStats.st_numinvis;
	}

	if (pptr->c_umodes != oldmodes)
	{
		simodule_umode_changed(pptr, oldmodes);
	}

	return 0;
}

static char *datestr(int thetime, char *buf)
{
	struct tm	gmbuf;
	struct tm	*tmptr;
	time_t		temptime;
	char		plusminus;
	int			minswest;

	if (!thetime)
		thetime = (int)time(NULL);
	temptime = (time_t)thetime;
	tmptr = gmtime(&temptime);
	if (tmptr)
		memcpy(&gmbuf, tmptr, sizeof(struct tm));
	else
	{
		strcpy(buf, "Error getting date");
		return buf;
	}
	tmptr = localtime(&temptime);

	if (tmptr->tm_yday == gmbuf.tm_yday)
		minswest = (gmbuf.tm_hour - tmptr->tm_hour) * 60 +
			(gmbuf.tm_min - tmptr->tm_min);
	else if (tmptr->tm_yday > gmbuf.tm_yday)
		minswest = (gmbuf.tm_hour - (tmptr->tm_hour + 24)) * 60;
	else
		minswest = ((gmbuf.tm_hour + 24) - tmptr->tm_hour) * 60;

	if (minswest > 0)
		plusminus = '-';
	else
	{
		plusminus = '+';
		minswest = -minswest;
	}

	sprintf(buf, "%s %s %d %04d -- %02d:%02d %c%02d:%02d",
		weekdays[tmptr->tm_wday], months[tmptr->tm_mon], tmptr->tm_mday,
		tmptr->tm_year + 1900, tmptr->tm_hour, tmptr->tm_min,
		plusminus, minswest/60, minswest%60);
	return buf;
}

/*
** command_nick
**  parv[0] = sender prefix
**  parv[1] = nickname
**  parv[2] = optional hopcount when new user; TS when nick change
**  parv[3] = optional TS
**  parv[4] = optional umode
**  parv[5] = optional username
**  parv[6] = optional hostname
**  parv[7] = optional server
**  parv[8] = optional ircname
*/
int _command_nick(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*nick = si_nextarg(&args, 0);
	char		*hopcount = si_nextarg(&args, 0);
	char		*ts = si_nextarg(&args, 0);
	char		*umode = si_nextarg(&args, 0);
	char		*username = si_nextarg(&args, 0);
	char		*hostname = si_nextarg(&args, 0);
	char		*servername = si_nextarg(&args, 0);
	char		*ircname = si_nextarg(&args, 0);
	char		*userhost;
	int			newts = 0;
	int			hc = 0;
	int			ulen;
	int			hlen;
	SI_CLIENT	*acptr;
	SI_CLIENT	*lptr = Con2Client(con);

	if (!lptr)
	{
		siconn_del(con, "Sorry, clients not supported.");
		return -1;
	}

	if (!nick)
	{
		return 0;
	}

	if (CliIsServer(pptr)) /* remote new nick */
	{
		if (!ircname)
			return 0;
		hc = atol(hopcount);
		newts = atol(ts);

		ulen = strlen(username);
		hlen = strlen(hostname);

		userhost = (char *)simempool_alloc(&SIMainPool, ulen+hlen+2);
		if (!userhost)
		{
			siconn_del(con, "Out of memory");
			return -1;
		}
		strcpy(userhost, username);
		userhost[ulen] = '@';
		strcpy(userhost+ulen+1, hostname);
	}
	else /* remote nick change */
	{
		if (hopcount)
			newts = atol(hopcount);
		userhost = pptr->c_userhost;
	}

	/* check to see if nick exists */

	acptr = siclient_find(nick);
	if (acptr)
	{
		if (CliIsJupe(acptr))
		{
			if (!CliIsServer(pptr) && (pptr != acptr))
			{
				siclient_free(pptr, "Jupitered Nick");
			}
			siclient_send_jupe(acptr);
			if (userhost != pptr->c_userhost)
				simempool_free(userhost);
			return 0;
		}

		if (acptr == pptr) /* same nick */
		{
			if (!strcmp(acptr->c_name, nick))
			{
				if (userhost != pptr->c_userhost)
					simempool_free(userhost);
				return 0; /* trying to change to same nick */
			}
			/* case change */
		}
		else
		{
			if (CliIsClient(pptr))
				si_log(SI_LOG_NOTICE,
					"Collision: Received \"%s!%s(%d) NICK %s!%s(%d)\"",
						pptr->c_name, pptr->c_userhost,
						pptr->c_creation,
						nick, acptr->c_userhost,
						acptr->c_creation);
			else
				si_log(SI_LOG_NOTICE,
					"Collision: Received \"%s(%d) NICK %s!%s(%d)\"",
						pptr->c_name, newts,
						nick, acptr->c_userhost,
						acptr->c_creation);
			
			if (!newts || (newts == acptr->c_creation))
			{
				siclient_free(acptr, "Nick collision");
			}
			else
			{
				int sameuser = !ircd_strcmp(acptr->c_userhost,
										userhost);
				if ((sameuser && (newts < acptr->c_creation)) ||
					(!sameuser && (newts > acptr->c_creation)))
				{
					if (CliIsServer(pptr))
					{
						if (userhost != pptr->c_userhost)
							simempool_free(userhost);
						return 0;
					}
					siclient_free(pptr, "Nick collision");
				}
				else
				{
					siclient_free(acptr, "Nick collision");
				}
			}
			/* fall through */
		}
	}

	if (CliIsServer(pptr)) /* new nick from remote server */
	{
		SI_CLIENT	*nptr;
		int			*s;
		int			flag;
		char		*m;

		nptr = (SI_CLIENT *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CLIENT));
		if (!nptr)
		{
			simempool_free(userhost);
			siconn_del(con, "Out of memory");
			return -1;
		}
		nptr->c_type = SI_CLIENT_TYPE_CLIENT;
		nptr->c_from = con;
		nptr->c_hopcount = hc;
		nptr->c_creation = newts ? newts : (newts = Now);
		nptr->c_name = simempool_strdup(&SIMainPool, nick);
		if (!nptr->c_name)
		{
			simempool_free(userhost);
			simempool_free(nptr);
			siconn_del(con, "Out of memory");
			return -1;
		}
		nptr->c_username = simempool_strdup(&SIMainPool, username);
		if (!nptr->c_username)
		{
			simempool_free(userhost);
			simempool_free(nptr->c_name);
			simempool_free(nptr);
			siconn_del(con, "Out of memory");
			return -1;
		}

		nptr->c_userhost = userhost;
		nptr->c_hostname = nptr->c_userhost + ulen + 1;

		nptr->c_servername = simempool_strdup(&SIMainPool, servername);
		if (!nptr->c_servername)
		{
			simempool_free(nptr->c_userhost);
			simempool_free(nptr->c_username);
			simempool_free(nptr->c_name);
			simempool_free(nptr);
			siconn_del(con, "Out of memory");
			return -1;
		}
		nptr->c_info = simempool_strdup(&SIMainPool, ircname);
		if (!nptr->c_info)
		{
			simempool_free(nptr->c_servername);
			simempool_free(nptr->c_userhost);
			simempool_free(nptr->c_username);
			simempool_free(nptr->c_name);
			simempool_free(nptr);
			siconn_del(con, "Out of memory");
			return -1;
		}

		m = umode + 1;
		for(;*m;m++)
		{
			for(s=_user_modes;(flag = *s);s+=2)
			{
				if (*m == *(s+1))
				{
					nptr->c_umodes |= flag;
					break;
				}
			}
		}

		siclient_add(pptr, nptr);

		return 0;
	}

	assert(userhost == pptr->c_userhost);

	pptr->c_creation = newts ? newts : (newts = Now);

	/* nick change */
	if (siclient_change_nick(pptr, nick))
	{
		siconn_del(con, "Out of memory");
		return -1;
	}

	return 0;
}

int _command_topic(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
#define MAX_TOPIC_CHANS	10
	char				*channels = si_nextarg(&args, 0);
	char				*topic = si_nextarg(&args, 0);
	char				*name;
	SI_CHANNEL			*chptr;
	SI_CLIENT			*lptr = Con2Client(con);
	char				*topic_who;
	char				*old_topic;
	char				*old_who;
	time_t				old_time;

	if (!lptr)
	{
		siconn_del(con, "Not registered.");
		return -1;
	}

	if (!CliIsServer(lptr))
	{
		return 0;
	}

	if (!channels || !*channels || !topic)
	{
		return 0;
	}

	for(;;)
	{
		name = si_next_subarg_comma(&channels);
		if (!name)
			break;
		chptr = sichannel_find(name);
		if (!chptr)
			continue;

		if (CliIsServer(pptr))
		{
			topic_who = simempool_strdup(&SIMainPool, pptr->c_name);
		}
		else
		{
			u_int	nlen = strlen(pptr->c_name);
			u_int	ulen = strlen(pptr->c_username);
			u_int	hlen = strlen(pptr->c_hostname);

			topic_who = simempool_alloc(&SIMainPool,
							nlen+ulen+hlen+3);
			if (topic_who)
			{
				strcpy(topic_who, pptr->c_name);
				topic_who[nlen] = '!';
				strcpy(topic_who+nlen+1, pptr->c_username);
				topic_who[nlen+ulen+1] = '@';
				strcpy(topic_who+nlen+2, pptr->c_hostname);
			}
		}

		old_topic = chptr->ch_topic;
		old_who = chptr->ch_topic_who;
		old_time = chptr->ch_topic_time;

		if (*topic)
			chptr->ch_topic = simempool_strdup(&SIMainPool, topic);
		else
			chptr->ch_topic = NULL;

		chptr->ch_topic_who = topic_who;
		chptr->ch_topic_time = Now;

		simodule_topic_changed(chptr, old_topic,
			old_who, old_time);

		if (old_topic)
			simempool_free(old_topic);
		if (old_who)
			simempool_free(old_who);
	}
	return 0;
}

int _command_version(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*dest = si_nextarg(&args, 0);
	SI_CLIENT	*lptr = Con2Client(con);
	SI_CLIENT	*acptr;

	if (!lptr)
	{
		siconn_printf(con, "%s 351 %s. %s :%s", SIMyName,
			VERSION, SIMyName, "EFNet Services");
		siconn_del(con, "Goodbye.");
		return -1;
	}

	if (!CliIsServer(lptr))
		return 0;	/* shouldn't happen */

	if (!dest || !ircd_match(SIMyName, dest) || !ircd_match(dest, SIMyName) || (((acptr = siclient_find_incl_hm(dest)) != NULL) && (acptr->c_conn || CliIsJupe(acptr))))
	{
		if (CliIsClient(pptr))
			siconn_printf(con, ":%s 351 %s %s. %s :%s",
				SIMyName, pptr->c_name, VERSION, SIMyName,
				"EFNet Services");
	}
	else
	{
		if (CliIsClient(pptr))
			siconn_printf(pptr->c_from,
					":%s 401 %s %s :No such nick/channel",
					SIMyName, pptr->c_name, dest);
	}
	return 0;
}

int _command_squit(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char			*server = si_nextarg(&args, 0);
	char			*comment = si_nextarg(&args, SI_NEXTARG_FLAGS_LASTARG);
	SI_CLIENT		*lptr = Con2Client(con);
	SI_CLIENT		*acptr;

	if (!lptr)
	{
		siconn_del(con, "Not registered.");
		return -1;
	}

	if (!CliIsServer(lptr))
	{
		return 0;
	}

	if (server && *server)
	{
		acptr = siclient_find_incl_hm(server);
		if (!acptr)
			acptr = siclient_find_match_server(server);
	}
	else
	{
		acptr = lptr;
		server = lptr->c_name;
	}

	if (!acptr || CliIsClient(acptr))
	{
		if (CliIsClient(pptr))
			siconn_printf(pptr->c_from,
				":%s 401 %s %s :No such nick/channel",
				SIMyName, pptr->c_name, server);
		return 0;
	}

	if (!comment || !*comment)
		comment = lptr->c_name;

	if (acptr->c_conn && CliIsServer(lptr))
	{
		siuplink_wallops_printf(NULL, "Received SQUIT %s from %s (%s)",
			server, pptr->c_name, comment);
	}

	if (acptr == lptr)
	{
		siconn_del(con, comment);
		return -1;
	}

	siclient_free(acptr, comment);

	return 0;
}

int _command_kill(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*victim = si_nextarg(&args, 0);
	char		*comment = si_nextarg(&args, SI_NEXTARG_FLAGS_LASTARG);
	SI_CLIENT	*acptr;
	SI_CLIENT	*lptr = Con2Client(con);

	if (!lptr)
	{
		siconn_del(con, "Not registered.");
		return -1;
	}

	if (!CliIsServer(lptr))
	{
		return 0;
	}

	if (!victim || !*victim)
	{
		return 0;
	}

	if (!comment || !*comment)
		comment = pptr->c_name;

	if (!(acptr = siclient_find(victim)))
	{
		if (CliIsClient(pptr))
		{
			siconn_printf(pptr->c_from,
				":%s 401 %s %s :No such nick/channel",
				SIMyName, pptr->c_name, victim);
		}
		return 0;
	}

	if (CliIsServer(acptr) || siclient_is_me(acptr))
	{
		siconn_printf(pptr->c_from,
			":%s 483 %s :You can't kill a server!",
			SIMyName, pptr->c_name);
		return 0;
	}

	siclient_free(acptr, comment);

	return 0;
}

int _command_ping(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char			*origin = si_nextarg(&args, 0);
	char			*destination = si_nextarg(&args, 0);
	SI_CLIENT		*acptr;
	SI_CLIENT		*lptr = Con2Client(con);

	if (!lptr)
		return 0;

	if (!origin || !*origin)
	{
		return 0;
	}

	acptr = siclient_find_incl_hm(origin);
	if (acptr && acptr != pptr)
		origin = lptr->c_name;
	if (destination && ircd_strcmp(destination, SIMyName))
	{
		acptr = siclient_find_incl_hm(destination);
		if (!acptr || !CliIsServer(acptr))
		{
			if (CliIsClient(pptr))
				siconn_printf(pptr->c_from,
					":%s 401 %s %s :No such nick/channel",
					SIMyName, pptr->c_name, destination);
		}
	}
	else
	{
		siconn_printf(pptr->c_from, ":%s PONG %s :%s", 
			SIMyName,
			(destination) ? destination : SIMyName,
			origin ? origin : "");
	}
	return 0;
}

int _command_pong(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char			*origin = si_nextarg(&args, 0);
	char			*destination = si_nextarg(&args, 0);
	SI_CLIENT		*acptr;
	SI_CLIENT		*lptr = Con2Client(con);

	if (!lptr)
		return 0;

	if (!CliIsServer(lptr))
		return 0;

	origin = NULL;	/* to stop gcc from warning cuz i'm not using it */

	if (CliIsServer(pptr) && CliSentEOB(pptr) && !CliGotEOB(pptr))
	{
		printf("Got EOB from %s\n", pptr->c_name);
		SIStats.st_serversbursting--;
		CliSetGotEOB(pptr);
	}

	if (destination && ircd_strcmp(destination, SIMyName))
	{
		acptr = siclient_find_incl_hm(destination);
		if (!acptr)
		{
			if (CliIsClient(pptr))
				siconn_printf(pptr->c_from,
					":%s 401 %s %s :No such nick/channel",
					SIMyName, pptr->c_name, destination);
		}
	}

	return 0;
}

int _command_join(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		*channel = si_nextarg(&args, 0);
	SI_CLIENT	*lptr = Con2Client(con);

	if (!lptr)
	{
		return 0;
	}

	if (!CliIsServer(lptr) || CliIsServer(pptr))
		return 0;

	if (!channel || (*channel != '0') || (*(channel+1) != '\0'))
	{
		if (channel)
		{
			si_log(SI_LOG_NOTICE,
				"JOIN not valid from user %s on %s: %s",
					pptr->c_name, pptr->c_servername, channel);
		}
		return 0;
	}

	siclient_del_channels(pptr);

	return 0;
}

int _command_part(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	SI_CHANNEL		*chptr;
	char			*channels = si_nextarg(&args, 0);
	char			*name;
	SI_CHANNELLIST	*chlptr;
	SI_CLIENT		*lptr = Con2Client(con);

	if (!lptr)
	{
		return 0;
	}

	if (!CliIsServer(lptr) || CliIsServer(pptr))
		return 0;

	if (!channels || !*channels)
	{
		return 0;
	}

	for(;(name = si_next_subarg_comma(&channels)) != NULL;)
	{
		chptr = sichannel_find(name);
		if (!chptr)
		{
			continue;
		}
		if ((chlptr = siclient_find_channel(pptr, chptr)) == NULL)
		{
			continue;
		}
		sichannel_del_user(chptr, pptr, chlptr);
	}
	return 0;
}

int _command_kick(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char				*channels = si_nextarg(&args, 0);
	char				*nickskeep = si_nextarg(&args, 0);
	char				*comment = si_nextarg(&args, 0);
	char				*channel;
	char				*nick;
	char				nicksbuf[512];
	char				*nicks = nickskeep;
	SI_CHANNEL			*chptr;
	SI_CLIENT			*acptr;
	SI_CHANNELLIST		*chlptr;
	SI_CLIENT			*lptr = Con2Client(con);

	if (!CliIsServer(lptr))
	{
		return 0;
	}

	if (!nicks || !*nicks)
	{
		return 0;
	}

	if (!comment)
		comment = pptr->c_name;

	for(;;)
	{
		if ((channel = si_next_subarg_comma(&channels)) == NULL)
			break;
		chptr = sichannel_find(channel);
		if (!chptr)
		{
			continue;
		}

		si_strncpy(nicksbuf, nickskeep, sizeof(nicksbuf));
		nicks = nicksbuf;

		for(;;)
		{
			if ((nick = si_next_subarg_comma(&nicks)) == NULL)
				break;
			if (!(acptr = siclient_find(nick)))
				continue;
			if ((chlptr = siclient_find_channel(acptr, chptr)) != NULL)
			{
				sichannel_del_user(chptr, acptr, chlptr);
			}
		}
	}

	return 0;
}

static int _command_time(SI_CONN *con, SI_CLIENT *pptr, char *prefix, char *args)
{
	char		datebuf[128];
	char		*dest = si_nextarg(&args, 0);
	SI_CLIENT	*lptr = Con2Client(con);
	SI_CLIENT	*acptr;

	if (!lptr)
	{
		siconn_del(con, "You must register, first.");
		return -1;
	}

	if (!dest || !ircd_match(SIMyName, dest) || !ircd_match(dest, SIMyName) || (((acptr = siclient_find_incl_hm(dest)) != NULL) && (acptr->c_conn || CliIsJupe(acptr))))
	{
		if (CliIsClient(pptr))
			siconn_printf(con, ":%s 391 %s %s :%s",
				SIMyName, pptr->c_name, SIMyName,
				datestr(0, datebuf));
	}
	else
	{
		if (CliIsClient(pptr))
			siconn_printf(pptr->c_from,
					":%s 401 %s %s :No such nick/channel",
					SIMyName, pptr->c_name, dest);
	}
	return 0;
}


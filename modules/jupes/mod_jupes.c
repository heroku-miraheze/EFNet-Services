/*
**   IRC services jupes module -- mod_jupes.c
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
#include "mod_admin.h"

RCSID("$Id: mod_jupes.c,v 1.2 2004/03/30 22:23:46 cbehrens Exp $");

#define SI_MOD_JUPE_TIMEOUT			(30*60)
#define SI_MOD_JUPE_SCORE_ADMIN		5
#define SI_MOD_JUPE_SCORE_OPER		3
#define SI_MOD_JUPE_SCORE_NEEDED	(SI_MOD_JUPE_SCORE_ADMIN*SI_MOD_JUPE_SCORE_OPER)

typedef struct _si_jupe_ignore	SI_JUPE_IGNORE;
typedef struct _si_mod_jupe		SI_MOD_JUPE;
typedef struct _si_mod_svote	SI_MOD_SVOTE;
typedef struct _si_mod_uvote	SI_MOD_UVOTE;
typedef struct _si_jupe_conf	SI_JUPE_CONF;

struct _si_jupe_ignore
{
	SI_JUPE_IGNORE	*ig_next;
	char			ig_string[1];
};

struct _si_jupe_conf
{
	char			*jc_dataname;

#define SI_JC_FLAGS_OPERNOTIFY		0x001
	u_int			jc_flags;

	SI_JUPE_IGNORE	*jc_ouh_ignores;
	SI_JUPE_IGNORE	*jc_oserv_ignores;
};

struct _si_mod_jupe
{
	char			*j_name;
	char			*j_reason;
	char			*j_username;
	char			*j_hostname;

	time_t			j_creation;		/* 0 == from .conf file */
	time_t			j_last_heard;
	time_t			j_expires;

#define SI_MOD_JUPE_FLAGS_ACTIVE		0x001
#define SI_MOD_JUPE_FLAGS_CONFFILE		0x002
	u_int			j_flags;

	u_int			j_score;

	SI_MOD_CLIENT	j_client;

	SI_MOD_SVOTE	*j_svotes;

	SI_MOD_JUPE		*j_prev;
	SI_MOD_JUPE		*j_next;
};

struct _si_mod_svote
{
	char			*sv_server;
	SI_MOD_UVOTE	*sv_uvotes;

	SI_MOD_SVOTE	*sv_prev;
	SI_MOD_SVOTE	*sv_next;
};

struct _si_mod_uvote
{
	char			*uv_nick;
	char			*uv_userhost;
#define SI_MOD_UVOTE_ADMIN		0x001
	u_int			uv_flags;
	u_int			uv_score;

	SI_MOD_UVOTE	*uv_prev;
	SI_MOD_UVOTE	*uv_next;
};


static int _do_jupe(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_unjupe(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_jupes(SI_CLIENT *from, SI_CLIENT *to, char *args);

static SI_MOD_COMMAND	_SCommands[] = {
	{	"jupe",		"Jupe a server",		SI_MC_FLAGS_OPERONLY,	_do_jupe	},
	{	"unjupe",	"Unjupe a server",		SI_MC_FLAGS_OPERONLY,	_do_unjupe	},
	{	"jupes",	"List current jupes",	SI_MC_FLAGS_OPERONLY,	_do_jupes	},
	{	NULL,		NULL,					0,	NULL		}
};

static SI_MOD_CLIENT	_JupeClients[] = {
	{	"JUPES",	"services",	"services.int",	"EFNet Services", SI_CLIENT_TYPE_CLIENT, SI_CLIENT_UMODE_OPER, 0, _SCommands },
	{	NULL, NULL, NULL, NULL, 0, 0, 0, NULL }
};

static SI_MOD_JUPE		*SIModJupes = NULL;
static SI_JUPE_CONF		SIJConf;
static SI_MEMPOOL		SIJMemPool;
static void				*SIModHandle = NULL;

static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);

SI_MODULE_DECL("Server/Nick jupe support",
				&SIJMemPool,
				NULL,
				_simodule_init,
				_simodule_deinit
			);


static int _jupes_ignore(SI_JUPE_IGNORE *ignores, char *string);
static void _uvote_free(SI_MOD_UVOTE *uv);
static void _svote_free(SI_MOD_SVOTE *sv);
static int _jupe_check_expired(SI_MOD_JUPE *jptr);
static SI_MOD_JUPE *_jupe_find(char *name);
static SI_MOD_SVOTE *_jupe_find_svote(SI_MOD_JUPE *jptr, char *sname);
static SI_MOD_UVOTE *_jupe_find_uvote(SI_MOD_JUPE *jptr, char *userhost);
static void _jupe_free(SI_MOD_JUPE *jptr, char *reason);
static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno);
static void _conf_deinit(void);
static int _conf_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_dataname(void *csc_cookie, char *args, void **cb_arg);
static int _conf_ouhignore(void *csc_cookie, char *args, void **cb_arg);
static int _conf_oservignore(void *csc_cookie, char *args, void **cb_arg);

static int _data_jupe_username(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_hostname(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_reason(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_creation(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_expires(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_lastheard(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_flags(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_score(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_vote_nick(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_vote_userhost(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_vote_flags(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_vote_score(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe_vote(void *csc_cookie, char *args, void **cb_arg);
static int _data_jupe(void *csc_cookie, char *args, void **cb_arg);

static int _datafile_load(char *filename);
static int _datafile_save(char *filename);


static CSC_COMMAND	Commands[] = {
	{	"flag",			_conf_flags,		0,		NULL },
	{	"flags",		_conf_flags,		0,		NULL },
	{	"dataname",		_conf_dataname,		0,		NULL },
	{	"ouh_ignore",	_conf_ouhignore,	0,		NULL },
	{	"oserv_ignore",	_conf_oservignore,	0,		NULL },
	{	NULL,			NULL,				0,		NULL }
};

static CSC_COMMAND	VoteCommands[] = {
	{	"nick",			_data_jupe_vote_nick, 	0,	NULL },
	{	"userhost",		_data_jupe_vote_userhost, 0,	NULL },
	{	"flags",		_data_jupe_vote_flags,	 0,		NULL },
	{	"score",		_data_jupe_vote_score,	 0,		NULL },
	{	NULL,			NULL,					0,		NULL }
};

static CSC_COMMAND	JupeCommands[] = {
	{	"username",		_data_jupe_username,	0,		NULL },
	{	"hostname",		_data_jupe_hostname,	0,		NULL },
	{	"reason",		_data_jupe_reason,		0,		NULL },
	{	"creation",		_data_jupe_creation,	0,		NULL },
	{	"expires",		_data_jupe_expires,		0,		NULL },
	{	"lastheard",	_data_jupe_lastheard,	0,		NULL },
	{	"flags",		_data_jupe_flags,		0,		NULL },
	{	"score",		_data_jupe_score,		0,		NULL },
	{	"vote",			_data_jupe_vote,		0,		&VoteCommands },
	{	NULL,			NULL,					0,		NULL }
};

static CSC_COMMAND	DataCommands[] = {
	{	"jupe",		_data_jupe,		0,		&JupeCommands },
	{	NULL,		NULL,			0,		NULL }
};


static int _jupes_ignore(SI_JUPE_IGNORE *ignores, char *string)
{
	for(;ignores;ignores=ignores->ig_next)
		if (!ircd_match(ignores->ig_string, string))
			return 1;
	return 0;
}

static void _uvote_free(SI_MOD_UVOTE *uv)
{
	if (uv)
	{
		if (uv->uv_nick)
			simempool_free(uv->uv_nick);
		if (uv->uv_userhost)
			simempool_free(uv->uv_userhost);
		simempool_free(uv);
	}
}

static void _svote_free(SI_MOD_SVOTE *sv)
{
	SI_MOD_UVOTE	*nextuv;

	if (sv)
	{
		if (sv->sv_server)
			simempool_free(sv);
		if (sv->sv_uvotes)
		{
			while(sv->sv_uvotes)
			{
				nextuv = sv->sv_uvotes->uv_next;
				_uvote_free(sv->sv_uvotes);
				sv->sv_uvotes = nextuv;
			}
		}
		simempool_free(sv);
	}
}

static int _jupe_check_expired(SI_MOD_JUPE *jptr)
{
	if (jptr->j_expires && (Now >= jptr->j_expires))
		return 1;
	return 0;
}

static SI_MOD_JUPE *_jupe_find(char *name)
{
	SI_MOD_JUPE	*j;
	SI_MOD_JUPE	*prevj;
	SI_MOD_JUPE	*nextj;

	for(j=SIModJupes,prevj=NULL;j;prevj=j,j=nextj)
	{
		nextj = j->j_next;
		if (!ircd_strcmp(name, j->j_name))
		{
			if (_jupe_check_expired(j))
			{
				if (prevj)
					prevj->j_next = j->j_next;
				else
					SIModJupes = j->j_next;
				_jupe_free(j, "Expired.");
				j = prevj;
				continue;
			}
			break;
		}
	}
	return j;
}

static SI_MOD_SVOTE *_jupe_find_svote(SI_MOD_JUPE *jptr, char *sname)

{
	SI_MOD_SVOTE	*sv;

	for(sv=jptr->j_svotes;sv;sv=sv->sv_next)
	{
		if (!ircd_strcmp(sv->sv_server, sname))
			return sv;
	}
	return NULL;
}

static SI_MOD_UVOTE *_jupe_find_uvote(SI_MOD_JUPE *jptr, char *userhost)
{
	SI_MOD_SVOTE	*sv;
	SI_MOD_UVOTE	*uv;

	for(sv=jptr->j_svotes;sv;sv=sv->sv_next)
	{
		for(uv=sv->sv_uvotes;uv;uv=uv->uv_next)
		{
			if (!ircd_strcmp(uv->uv_userhost, userhost))
				return uv;
		}
	}
	return NULL;
}

static void _jupe_free(SI_MOD_JUPE *jptr, char *reason)
{
	SI_MOD_SVOTE	*sv;

	if (jptr)
	{
		if (jptr->j_flags & SI_MOD_JUPE_FLAGS_ACTIVE)
		{
			siuplink_wallops_printf(NULL, "%s has been unjuped%s%s%s",
				jptr->j_name, reason?":":".",
				reason?" ":"",
				reason?reason:"");
			simodule_del_client(SIModHandle, &jptr->j_client);
		}
		while(jptr->j_svotes)
		{
			sv = jptr->j_svotes->sv_next;
			simempool_free(jptr->j_svotes->sv_server);
			simempool_free(jptr->j_svotes);
			jptr->j_svotes = sv;
		}
		if (jptr->j_reason)
			simempool_free(jptr->j_reason);
		if (jptr->j_username)
			simempool_free(jptr->j_username);
		if (jptr->j_hostname)
			simempool_free(jptr->j_hostname);
		if (jptr->j_name)
			simempool_free(jptr->j_name);
		simempool_free(jptr);
	}
}

static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno)
{
	int		level = SI_LOG_DEBUG;
	char	*string = "";

	if (severity == CSC_SEVERITY_WARNING)
	{
		level = SI_LOG_WARNING;
		string = "Warning";
	}
	else
	{
		level = SI_LOG_ERROR;
		string = "Error";
	}
	si_log(level, "mod_jupes: %s:%d: %s%s%s%s",
		filename,
		lineno,
		string,
		*string ? ":" : string,
		*string ? " " : string,
		error);
}

static void _conf_deinit(void)
{
	SI_JUPE_IGNORE	*ig;

	while(SIJConf.jc_ouh_ignores)
	{
		ig = SIJConf.jc_ouh_ignores->ig_next;
		simempool_free(SIJConf.jc_ouh_ignores);
		SIJConf.jc_ouh_ignores = ig;
	}
	while(SIJConf.jc_oserv_ignores)
	{
		ig = SIJConf.jc_oserv_ignores->ig_next;
		simempool_free(SIJConf.jc_oserv_ignores);
		SIJConf.jc_oserv_ignores = ig;
	}
	if (SIJConf.jc_dataname)
	{
		simempool_free(SIJConf.jc_dataname);
		SIJConf.jc_dataname = NULL;
	}
}

static int _simodule_init(void *modhandle, char *arg)
{
	SI_MOD_CLIENT	*clptr;
	int				err;
	char			*confname = csconfig_next_arg(&arg);

	if (!modhandle || !confname || !*confname)
	{
		si_log(SI_LOG_ERROR, "mod_jupes: not enough arguments passed to module");
		return EINVAL;
	}

	SIModHandle = modhandle;

	memset(&SIJConf, 0, sizeof(SIJConf));

	err = csconfig_read(confname, Commands, &SIJConf, _conf_error_func);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_jupes: couldn't load \"%s\": %d",
			confname, err);
		_conf_deinit();
		return err;
	}

	if (!SIJConf.jc_dataname)
	{
		si_log(SI_LOG_ERROR, "mod_jupes: no dataname specified in \"%s\"",
			confname);
		_conf_deinit();
		return EINVAL;
	}

	err = _datafile_load(SIJConf.jc_dataname);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_jupes: error loading datafile \"%s\": %d",
				SIJConf.jc_dataname, err);
		_conf_deinit();
		return err;
	}

	for(clptr=_JupeClients;clptr->mcl_name;clptr++)
	{
		err = simodule_add_client(SIModHandle, clptr);
		if (err)
		{
			_conf_deinit();
			return err;
		}
	}

	return 0;
}

static void _simodule_deinit(void)
{
	SI_MOD_JUPE		*j;
	SI_MOD_CLIENT	*clptr;

#if 0
	int				err;

	err = _datafile_save(SIJConf.jc_dataname);
#endif

	while(SIModJupes)
	{
		j = SIModJupes->j_next;
		_jupe_free(SIModJupes, "jupe module unloading");
		SIModJupes = j;
	}
	for(clptr=_JupeClients;clptr->mcl_name;clptr++)
		simodule_del_client(SIModHandle, clptr);
	_conf_deinit();
}

static int _do_jupe(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	char			*tojupe;
	char			*ptr;
	SI_MOD_JUPE		*j;
	u_int			len;
	u_int			is_admin = 0;
	SI_MOD_SVOTE	*sv;
	char			*uplink_name;
	int				ret;

	assert(from && to && CliIsOper(from));

	if (simodadmin_client_is_authed(from))
		is_admin = 1;
	else
	{
		if (_jupes_ignore(SIJConf.jc_ouh_ignores, from->c_userhost) ||
				_jupes_ignore(SIJConf.jc_oserv_ignores,
											from->c_servername))
			return EFAULT;
	}

	tojupe = si_next_subarg_space(&args);
	if (!tojupe || !*tojupe)
	{
		siclient_send_privmsg(to, from, "Usage: JUPE <nick/server> [<reason>]");
		return 0;
	}
	if (args)
	{
		while(*args == ' ')
			args++;
	}


	if (!ircd_match(tojupe, SIMyName) ||
		(((uplink_name = siuplink_name()) != NULL) &&
			(!ircd_match(tojupe, uplink_name) || !ircd_match(uplink_name, tojupe))))
	{
		si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried on me or my uplink: %s!%s on %s [%s]",
			tojupe, args?" ":"",args?args:"", from->c_name,
			from->c_userhost, from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Sorry, can't jupe me or my current uplink");
		return 0;
	}

	len = strlen(tojupe);

	ptr = strchr(tojupe, '.');
	if (!ptr)
	{
#if 1
		si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried for a nickname: %s!%s on %s [%s]",
			tojupe,
			args ? " " : "", args ? args : "",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Sorry, nick juping is disabled.");
		return EINVAL;
#else

		if (len > NICKLEN)
		{
			si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but is too long: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			siclient_send_privmsg(to, from, "Sorry, jupe is too long.");
			return EINVAL;
		}
#endif
	}
	else if (len > SERVERLEN)
	{
		si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but is too long: %s!%s on %s [%s]",
			tojupe,
			args ? " " : "", args ? args : "",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Sorry, jupe is too long.");
		return EINVAL;
	}
	
	j = _jupe_find(tojupe);
	if (j)
	{
		SI_MOD_UVOTE	*uv;

		if (j->j_flags & SI_MOD_JUPE_FLAGS_ACTIVE)
		{
			si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but is already juped: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			siclient_send_privmsg(to, from, "That jupe is already active.");
			return EINVAL;
		}
		uv = _jupe_find_uvote(j, from->c_userhost);
		if (uv)
		{
			si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but user has already voted: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			siclient_send_privmsg(to, from, "You have already voted for this jupe.");
			return EINVAL;
		}
		sv = _jupe_find_svote(j, from->c_servername);
		if (sv)
		{
			si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but server has already voted: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			siclient_send_privmsg(to, from, "Your server has already voted for this jupe.");
			return EINVAL;
		}

		si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" voted for by: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
	}
	else
	{
		if (!args || !*args)
		{
			siclient_send_privmsg(to, from, "A reason must be specified for the first vote for a jupe.");
			si_log(SI_LOG_NOTICE, "\"jupe %s%s%s\" tried, but no reason given: %s!%s on %s [%s]",
				tojupe,
				args ? " " : "", args ? args : "",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			return EINVAL;
		}

		si_log(SI_LOG_NOTICE, "\"jupe %s %s\" called for by: %s!%s on %s [%s]",
				tojupe,
				args,
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);

		j = (SI_MOD_JUPE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_JUPE));
		if (!j)
		{
			siclient_send_privmsg(to, from, "Doh, I'm out of memory!");
			return ENOMEM;
		}
		j->j_name = simempool_strdup(&SIJMemPool, tojupe);
		j->j_reason = (char *)simempool_alloc(&SIJMemPool, 8+strlen(args));
		j->j_username = simempool_strdup(&SIJMemPool, "services");
		j->j_hostname = simempool_strdup(&SIJMemPool, "services.int");
		if (!j->j_name || !j->j_reason || !j->j_username || !j->j_hostname)
		{
			_jupe_free(j, "Out of memory");
			siclient_send_privmsg(to, from, "Doh, I'm out of memory!");
			return ENOMEM;
		}
		strcpy(j->j_reason, "JUPED: ");
		strcpy(j->j_reason+7, args);
		j->j_client.mcl_name = j->j_name;
		j->j_client.mcl_username = j->j_username;
		j->j_client.mcl_hostname = j->j_hostname;
		j->j_client.mcl_info = j->j_reason;
		j->j_client.mcl_type = ptr ? SI_CLIENT_TYPE_SERVER : SI_CLIENT_TYPE_CLIENT;
		if (strlen(j->j_reason) >= 128)
			j->j_reason[128] = '\0';
		j->j_creation = Now;
		j->j_expires = Now + SI_MOD_JUPE_TIMEOUT;
		si_ll_add_to_list(SIModJupes, j, j);

		if (SIJConf.jc_flags & SI_JC_FLAGS_OPERNOTIFY)
		{
			siuplink_opers_printf(to, "*** A jupe of \"%s\" has been called by %s!%s from %s, with reason: %s",
				j->j_name, from->c_name, from->c_userhost,
				from->c_servername, j->j_reason);
			siuplink_opers_printf(to, "/msg %s jupe %s if you agree with the jupe",
				to->c_name, j->j_name);
		}
	}

	j->j_last_heard = Now;

	sv = (SI_MOD_SVOTE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_SVOTE));
	if (sv)
	{
		sv->sv_server = simempool_strdup(&SIJMemPool, from->c_servername);
		sv->sv_uvotes = (SI_MOD_UVOTE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_UVOTE));
		if (sv->sv_uvotes)
		{
			sv->sv_uvotes->uv_nick = simempool_strdup(&SIJMemPool, from->c_name);
			sv->sv_uvotes->uv_userhost = simempool_strdup(&SIJMemPool, from->c_userhost);
		}
	}
	if (!sv || !sv->sv_server || !sv->sv_uvotes ||
			!sv->sv_uvotes->uv_nick || !sv->sv_uvotes->uv_userhost)
	{
		_svote_free(sv);
		siclient_send_privmsg(to, from, "Doh, I'm out of memory!");
		return ENOMEM;
	}

	si_ll_add_to_list(j->j_svotes, sv, sv);

	if (is_admin)
		sv->sv_uvotes->uv_score = SI_MOD_JUPE_SCORE_ADMIN;
	else
		sv->sv_uvotes->uv_score = SI_MOD_JUPE_SCORE_OPER;

	j->j_score += sv->sv_uvotes->uv_score;

	siclient_send_privmsg(to, from, "JUPE for %s now has %d points.",
		tojupe, j->j_score);

	if (j->j_score >= SI_MOD_JUPE_SCORE_NEEDED)
	{
		int err = simodule_add_client(SIModHandle, &j->j_client);

		if (err)
		{
			siclient_send_privmsg(to, from, "Error activating JUPE for %s: %d",
				tojupe, err);
			return 0;
		}
		j->j_flags |= SI_MOD_JUPE_FLAGS_ACTIVE;
		j->j_expires = 0;
		siuplink_wallops_printf(NULL, "%s has been juped: %s",
				j->j_name, j->j_reason);
		siclient_send_privmsg(to, from, "JUPE for %s is now active.",
				j->j_name);
		err = _datafile_save(SIJConf.jc_dataname);
		if (err)
			si_log(SI_LOG_ERROR, "mod_jupes: Couldn't save \"%s\": %d",
					SIJConf.jc_dataname, err);
		return 0;
	}
	siclient_send_privmsg(to, from, "A total of %d points is needed.",
		SI_MOD_JUPE_SCORE_NEEDED);

	ret = _datafile_save(SIJConf.jc_dataname);
	if (ret)
		si_log(SI_LOG_ERROR, "mod_jupes: Couldn't save \"%s\": %d",
			SIJConf.jc_dataname, ret);

	return 0;
}

static int _do_unjupe(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	char			*tojupe;
	SI_MOD_JUPE		*j;
	int				err;
	char			buf[80];

	assert(from && to && CliIsOper(from));

	if (!simodadmin_client_is_authed(from) &&
		(_jupes_ignore(SIJConf.jc_ouh_ignores, from->c_userhost) ||
				_jupes_ignore(SIJConf.jc_oserv_ignores,
											from->c_servername)))
			return EFAULT;

	tojupe = si_next_subarg_space(&args);
	if (!tojupe || !*tojupe)
	{
		si_log(SI_LOG_NOTICE, "\"unjupe\" tried from: %s!%s on %s [%s]",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "A jupe name must be specified");
		return EINVAL;
	}

	j = _jupe_find(tojupe);
	if (!j)
	{
		si_log(SI_LOG_NOTICE, "\"unjupe %s%s%s\" tried on non-existant jupe: %s!%s on %s [%s]",
			tojupe,
			args?" ":"",args?args:"",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Jupe \"%s\" does not exist.",
			tojupe);
		return ENOENT;
	}
	if (j->j_flags & SI_MOD_JUPE_FLAGS_CONFFILE)
	{
		si_log(SI_LOG_NOTICE, "\"unjupe %s%s%s\" tried on permanant jupe: %s!%s on %s [%s]",
			tojupe,
			args?" ":"",args?args:"",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Jupe \"%s\" is a permanant jupe and cannot be removed.", j->j_name);
		return EACCES;
	}
	if (!(j->j_flags & SI_MOD_JUPE_FLAGS_ACTIVE))
	{
		si_log(SI_LOG_NOTICE, "\"unjupe %s%s%s\" tried on non-active jupe: %s!%s on %s [%s]",
			tojupe,
			args?" ":"",args?args:"",
			from->c_name, from->c_userhost,
			from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "%s is not an active jupe and will expire at %s",
			j->j_name, si_timestr(j->j_expires, buf, sizeof(buf)));
		return EACCES;
	}

	si_ll_del_from_list(SIModJupes, j, j);

	si_log(SI_LOG_NOTICE, "\"unjupe %s%s%s\" done by: %s!%s on %s [%s]",
		tojupe,
		args?" ":"",args?args:"",
		from->c_name, from->c_userhost,
		from->c_servername, from->c_info);

	_jupe_free(j, NULL);

	err = _datafile_save(SIJConf.jc_dataname);
	if (err)
		si_log(SI_LOG_ERROR, "mod_jupes: Couldn't save \"%s\": %d",
			SIJConf.jc_dataname, err);

	return 0;
}

static int _do_jupes(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	char			flags[16];
	int				verb = 0;
	SI_MOD_JUPE		*j;
	SI_MOD_JUPE		*prevj;
	SI_MOD_JUPE		*nextj;
	char			*fptr;
	char			buf[80];

	assert(from && to && CliIsOper(from));

	if (args && !strcmp(args, "verbose"))
		verb = 1;

	if (!SIModJupes)
	{
		siclient_send_privmsg(to, from, "There are currently no jupes.");
		return 0;
	}

	siclient_send_privmsg(to, from, "Following are the jupes:");

	for(prevj=NULL,j=SIModJupes;j;prevj=j,j=nextj)
	{
		nextj = j->j_next;

		fptr = flags;

		if (_jupe_check_expired(j))
		{
			if (prevj)
				prevj->j_next = j->j_next;
			else
				SIModJupes = j->j_next;
			_jupe_free(j, "Expired.");
			j = prevj;
			continue;
		}

		if (j->j_flags & SI_MOD_JUPE_FLAGS_ACTIVE)
			*fptr++ = 'A';
		if (!j->j_expires)
			*fptr++ = 'P';
		
		*fptr = '\0';
		if (verb)
		{
			char exp[80];

			if (j->j_expires)
				strcpy(exp, si_timestr(j->j_expires, buf, sizeof(buf)));
			else
				strcpy(exp, "Never");

			siclient_send_privmsg(to, from, "%s -- Score: %d -- Flags: %s -- Created: %s -- Expires: %s",
				j->j_name, j->j_score, flags, si_timestr(j->j_creation, buf, sizeof(buf)), exp);
			if (!j->j_svotes)
				siclient_send_privmsg(to, from, "   No votes for %s recorded",
					j->j_name);
			else
			{
				SI_MOD_SVOTE	*sv = j->j_svotes;
				SI_MOD_UVOTE	*uv;

				for(;sv;sv=sv->sv_next)
				{
					for(uv=sv->sv_uvotes;uv;uv=uv->uv_next)
					{
						siclient_send_privmsg(to, from, "%d -- %s!%s from %s",
							uv->uv_score, uv->uv_nick, uv->uv_userhost,
							sv->sv_server);
					}
				}
			}
		}
		else
			siclient_send_privmsg(to, from, "%s -- Score: %d -- Flags: %s -- %s ",
				j->j_name, j->j_score, flags, j->j_reason);
	}

	siclient_send_privmsg(to, from, "End of list");

	return 0;
}

static int _conf_ouhignore(void *csc_cookie, char *args, void **cb_arg)
{
	SI_JUPE_CONF		*jc = *((SI_JUPE_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);
	u_int				len;
	SI_JUPE_IGNORE		*ig;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no userhost specified");
		return EINVAL;
	}

	len = strlen(arg);

	ig = (SI_JUPE_IGNORE *)simempool_alloc(&SIJMemPool,
					sizeof(SI_JUPE_IGNORE) + len);
    if (!ig)
        return ENOMEM;

	strcpy(ig->ig_string, arg);
	ig->ig_next = jc->jc_ouh_ignores;
	jc->jc_ouh_ignores = ig;

    return 0;
}

static int _conf_oservignore(void *csc_cookie, char *args, void **cb_arg)
{
	SI_JUPE_CONF		*jc = *((SI_JUPE_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);
	u_int				len;
	SI_JUPE_IGNORE		*ig;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no server specified");
		return EINVAL;
	}

	len = strlen(arg);

	ig = (SI_JUPE_IGNORE *)simempool_alloc(&SIJMemPool,
					sizeof(SI_JUPE_IGNORE) + len);
    if (!ig)
        return ENOMEM;

	strcpy(ig->ig_string, arg);
	ig->ig_next = jc->jc_oserv_ignores;
	jc->jc_oserv_ignores = ig;

    return 0;
}

static int _conf_flags(void *csc_cookie, char *args, void **cb_arg)
{
	SI_JUPE_CONF	*jc = *((SI_JUPE_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

    if (!arg || !*arg)
    {
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flag specified");
		return EINVAL;
    }

    for(;;)
    {
        if (!strcasecmp(arg, "opernotify"))
        {
			jc->jc_flags |= SI_JC_FLAGS_OPERNOTIFY;
        }
        else
        {
            csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "unknown flag");
            return EINVAL;
        }
        arg = csconfig_next_arg(&args);
        if (!arg || !*arg)
            break;
    }

	return 0;
}

static int _conf_dataname(void *csc_cookie, char *args, void **cb_arg)
{
	SI_JUPE_CONF	*jc = *((SI_JUPE_CONF **)cb_arg);
	char			*name = csconfig_next_arg(&args);

	if (!name || !*name)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no dataname specified");
		return EINVAL;
	}

	if (jc->jc_dataname)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "dataname already specified");
		return EALREADY;
	}

	jc->jc_dataname = simempool_strdup(&SIJMemPool, name);
	if (!jc->jc_dataname)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	return 0;
}

static int _data_jupe_username(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no username specified");
		return EINVAL;
	}

	if (j->j_username)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "username already specified");
		return EALREADY;
	}

	j->j_username = simempool_strdup(&SIJMemPool, arg);
	if (!j->j_username)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	j->j_client.mcl_username = j->j_username;

	return 0;
}

static int _data_jupe_hostname(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no hostname specified");
		return EINVAL;
	}

	if (j->j_hostname)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "hostname already specified");
		return EALREADY;
	}

	j->j_hostname = simempool_strdup(&SIJMemPool, arg);
	if (!j->j_hostname)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	j->j_client.mcl_hostname = j->j_hostname;

	return 0;
}

static int _data_jupe_reason(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = args;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no reason specified");
		return EINVAL;
	}

	if (j->j_reason)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "reason already specified");
		return EALREADY;
	}

	j->j_reason = simempool_strdup(&SIJMemPool, arg);
	if (!j->j_reason)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	j->j_client.mcl_info = j->j_reason;

	return 0;
}

static int _data_jupe_creation(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no creation specified");
		return EINVAL;
	}

	j->j_creation = atoi(arg);

	return 0;
}

static int _data_jupe_expires(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no expire specified");
		return EINVAL;
	}

	j->j_expires = atoi(arg);

	return 0;
}

static int _data_jupe_lastheard(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no lastheard specified");
		return EINVAL;
	}

	j->j_last_heard = atoi(arg);

	return 0;
}

static int _data_jupe_flags(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flags specified");
		return EINVAL;
	}

	j->j_flags = atoi(arg);

	return 0;
}

static int _data_jupe_score(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no score specified");
		return EINVAL;
	}

	j->j_score = atoi(arg);

	return 0;
}

static int _data_jupe_vote_nick(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_UVOTE	*uv = *((SI_MOD_UVOTE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no nick specified");
		return EINVAL;
	}

	if (uv->uv_nick)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "nickname already specified");
		return EALREADY;
	}

	uv->uv_nick = simempool_strdup(&SIJMemPool, arg);
	if (!uv->uv_nick)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	return 0;
}

static int _data_jupe_vote_userhost(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_UVOTE	*uv = *((SI_MOD_UVOTE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no userhost specified");
		return EINVAL;
	}

	if (uv->uv_userhost)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "userhost already specified");
		return EALREADY;
	}

	uv->uv_userhost = simempool_strdup(&SIJMemPool, arg);
	if (!uv->uv_userhost)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	return 0;
}

static int _data_jupe_vote_flags(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_UVOTE	*uv = *((SI_MOD_UVOTE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flags specified");
		return EINVAL;
	}

	uv->uv_flags = atoi(arg);
	return 0;
}

static int _data_jupe_vote_score(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_UVOTE	*uv = *((SI_MOD_UVOTE **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no score specified");
		return EINVAL;
	}

	uv->uv_score = atoi(arg);

	return 0;
}

static int _data_jupe_vote(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j = *((SI_MOD_JUPE **)cb_arg);
	SI_MOD_SVOTE	*sv;
	SI_MOD_UVOTE	*uv;
	char			*server = csconfig_next_arg(&args);

	if (!server || !*server)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no server specified");
		return EINVAL;
	}

	sv = _jupe_find_svote(j, server);
	if (!sv)
	{
		sv = (SI_MOD_SVOTE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_SVOTE));
		if (!sv)
		{
			csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
			return ENOMEM;
		}
		sv->sv_server = simempool_strdup(&SIJMemPool, server);
		if (!sv->sv_server)
		{
			simempool_free(sv);
			csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
			return ENOMEM;
		}

		sv->sv_next = j->j_svotes;
		j->j_svotes = sv;

	}

	uv = (SI_MOD_UVOTE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_UVOTE));
	if (!uv)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}

	uv->uv_next = sv->sv_uvotes;
	sv->sv_uvotes = uv;

	*cb_arg = (void *)uv;

	return 0;
}

static int _data_jupe(void *csc_cookie, char *args, void **cb_arg)
{
	SI_MOD_JUPE		*j;
	SI_MOD_JUPE		**jbucket = *((SI_MOD_JUPE ***)cb_arg);
	char			*name;
	char			*ptr;

	name = csconfig_next_arg(&args);
	if (!name || !*name)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no name specified");
		return EINVAL;
	}

	ptr = strchr(name, '.');
	if ((ptr && (strlen(name) > SERVERLEN)) || (!ptr && (strlen(name) > NICKLEN)))
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid name specified");
		return EINVAL;
	}

	j = (SI_MOD_JUPE *)simempool_calloc(&SIJMemPool, 1, sizeof(SI_MOD_JUPE));
	if (!j)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
		return ENOMEM;
	}
	j->j_name = simempool_strdup(&SIJMemPool, name);
	if (!j->j_name)
	{
		simempool_free(j);
		return ENOMEM;
	}

	j->j_client.mcl_name = j->j_name;
	if (ptr)
		j->j_client.mcl_type = SI_CLIENT_TYPE_SERVER;
	else
		j->j_client.mcl_type = SI_CLIENT_TYPE_CLIENT;

	j->j_next = *jbucket;
	*jbucket = j;

	*cb_arg = (void *)j;

	return 0;
}


static int _datafile_load(char *filename)
{
	SI_MOD_JUPE		*jupes = NULL;
	SI_MOD_JUPE		*j;
	SI_MOD_JUPE		*nextj;
	SI_MOD_JUPE		*prevj;
	int				err;
	char			*ptr;

	err = csconfig_read(filename, DataCommands, &jupes, _conf_error_func);
	if (err)
	{
		return err == ENOENT ? 0 : err;
	}

	for(prevj=NULL,j=jupes;j;prevj=j,j=nextj)
	{
		nextj = j->j_next;

		if (_jupe_check_expired(j))
		{
			if (prevj)
				prevj->j_next = nextj;
			else
				jupes = nextj;
			_jupe_free(j, "Expired.");
			j = prevj;
			continue;
		}

		if (!j->j_name || !j->j_reason || (!(ptr = strchr(j->j_name, '.')) &&
					(!j->j_username || !j->j_hostname)))
		{
			si_ll_del_from_list(jupes, j, j);
			j->j_flags &= ~SI_MOD_JUPE_FLAGS_ACTIVE;
			_jupe_free(j, NULL);
			continue;
		}
		if (j->j_flags & SI_MOD_JUPE_FLAGS_ACTIVE)
		{
			int err = simodule_add_client(SIModHandle, &j->j_client);
			if (err)
			{
				while(jupes)
				{
					si_ll_del_from_list(jupes, jupes, j);
					jupes->j_flags &= ~SI_MOD_JUPE_FLAGS_ACTIVE;
					_jupe_free(jupes, NULL);
				}
				si_log(SI_LOG_ERROR, "mod_jupes: simodule_add_client() failed during datafile_load(%s): %d",
					filename, err);
				return err;
			}
			siuplink_wallops_printf(NULL, "%s has been juped: %s",
				j->j_name, j->j_reason);
		}
	}

	if (!jupes)
		return 0;

	if (!(j = SIModJupes))
	{
		SIModJupes = jupes;
		return 0;
	}

	while(j->j_next)
		j = j->j_next;

	j->j_next = jupes;
	jupes->j_prev = j;

	return 0;
}

static int _write_var(int fd, char *var, char *value)
{
	int				len = strlen(var);
	int				len2 = strlen(value);
	int				tot = len + len2 + 2;
	int				ret;
	char			buf[1024];
	char			*b = buf;

	if (!len2)
		tot--;

	if (tot > sizeof(buf))
	{
		b = simempool_alloc(&SIJMemPool, tot);
		if (!b)
			return ENOMEM;
	}
	strcpy(b, var);
	if (len2)
	{
		b[len] = ' ';
		strcpy(b+len+1, value);
	}
	b[tot-1] = '\n';
	ret = write(fd, b, tot);

	if (b != buf)
		simempool_free(b);

	if (ret != tot)
	{
		if (ret < 0)
			return errno;
		return EFAULT;
	}
	return 0;
}

static int _datafile_save(char *filename)
{
	int				fd;
	int				err = 0;
	char			buf[40];
	SI_MOD_JUPE		*j;
	SI_MOD_SVOTE	*sv;
	SI_MOD_UVOTE	*uv;

	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0660);
	if (fd < 0)
	{
		return errno;
	}

	for(j=SIModJupes;j;j=j->j_next)
	{
		err = _write_var(fd, "jupe", j->j_name);
		if (err)
			break;
		err = _write_var(fd, "\treason", j->j_reason);
		if (err)
			break;
		err = _write_var(fd, "\tusername", j->j_username);
		if (err)
			break;
		err = _write_var(fd, "\thostname", j->j_hostname);
		if (err)
			break;
		sprintf(buf, "%d", (int)j->j_creation);
		err = _write_var(fd, "\tcreation", buf);
		if (err)
			break;
		sprintf(buf, "%d", (int)j->j_last_heard);
		err = _write_var(fd, "\tlastheard", buf);
		if (err)
			break;
		sprintf(buf, "%d", (int)j->j_expires);
		err = _write_var(fd, "\texpires", buf);
		if (err)
			break;
		sprintf(buf, "%d", j->j_flags);
		err = _write_var(fd, "\tflags", buf);
		if (err)
			break;
		sprintf(buf, "%d", j->j_score);
		err = _write_var(fd, "\tscore", buf);
		if (err)
			break;
		for(sv=j->j_svotes;sv;sv=sv->sv_next)
		{
			err = _write_var(fd, "\tvote", sv->sv_server);
			if (err)
				break;
			for(uv=sv->sv_uvotes;uv;uv=uv->uv_next)
			{
				err = _write_var(fd, "\t\tnick", uv->uv_nick);
				if (err)
					break;
				err = _write_var(fd, "\t\tuserhost", uv->uv_userhost);
				if (err)
					break;
				sprintf(buf, "%d", uv->uv_flags);
				err = _write_var(fd, "\t\tflags", buf);
				if (err)
					break;
				sprintf(buf, "%d", uv->uv_score);
				err = _write_var(fd, "\t\tscore", buf);
				if (err)
					break;
			}
			err = _write_var(fd, "\tend", "");
			if (err)
				break;
		}
		if (err)
			break;
		err = _write_var(fd, "end", "");
		if (err)
			break;
	}

	close(fd);

	return err;
}

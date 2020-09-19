/*
**   IRC services chanjupe module -- mod_chanjupe.c
**   Copyright (C) 2003-2004 Chris Behrens
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

RCSID("$Id: mod_chanjupe.c,v 1.2 2004/03/30 22:23:45 cbehrens Exp $");


#define SI_UHJC_FLAGS_KILL_DISABLE		0x001

typedef struct _si_chanjupe_kill			SI_CHANJUPE_KILL;
typedef struct _si_chanjupe_conf			SI_CHANJUPE_CONF;

struct _si_chanjupe_kill
{
	SI_CHANJUPE_KILL			*cjk_next;
#define SI_IG_FLAGS_MANUAL		0x001
#define SI_IG_FLAGS_AUTO		0x002
	int							cjk_flags;
	char						cjk_string[1];
};

struct _si_chanjupe_conf
{
	u_int						cjc_flags;
	SI_CHANJUPE_KILL			*cjc_chanjupes;
};


static SI_MEMPOOL			SIDKMemPool;
static void					*SIModHandle = NULL;
static SI_CHANJUPE_CONF	SIDKConf;

static SI_CHANJUPE_KILL *_chanjupe_find(SI_CHANJUPE_KILL *uhs, char *string, int flags);
static void _client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
static void _conf_deinit(void);
static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno);
static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);
static int _conf_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_chanjupe(void *csc_cookie, char *args, void **cb_arg);
static int _do_chanjupes(SI_CLIENT *from, SI_CLIENT *to, char *args);


static SI_MOD_CALLBACKS     SIDKCallbacks = {
	NULL,
	NULL,
	NULL,
	NULL,
    NULL,
    NULL,
	NULL,
	NULL,
	NULL,
	NULL,
    _client_added,
    NULL,
    NULL,
	NULL,
	NULL,
    NULL
};

static CSC_COMMAND  Commands[] = {
	{   "flag",			_conf_flags,		0,		NULL },
	{   "flags",		_conf_flags,		0,		NULL },
	{	"chanjupe",		_conf_chanjupe,		0,		NULL },
	{   NULL,       NULL,           0,      NULL } 
};                                                

static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);

SI_MODULE_DECL("Dronekill module",
                &SIDKMemPool,
				&SIDKCallbacks,
                _simodule_init,
                _simodule_deinit
            );



static SI_CHANJUPE_KILL *_chanjupe_find(SI_CHANJUPE_KILL *uhs, char *string, int flags)
{
	for(;uhs;uhs=uhs->cjk_next)
	{
		if (flags && !(flags & uhs->cjk_flags))
			continue;
		if (!ircd_match(uhs->cjk_string, string))
			return uhs;
	}
	return NULL;
}

static void _conf_deinit(void)
{
	SI_CHANJUPE_KILL		*dk;

	while(SIDKConf.cjc_chanjupes)
	{
		dk = SIDKConf.cjc_chanjupes->cjk_next;
		simempool_free(SIDKConf.cjc_chanjupes);
		SIDKConf.cjc_chanjupes = dk;
	}
}

static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno)
{
	int	 level = SI_LOG_DEBUG;
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
	si_log(level, "mod_chanjupe: %s:%d: %s%s%s%s",
		filename,
		lineno,
		string,
		*string ? ":" : string,
		*string ? " " : string,
		error);
}

static int _simodule_init(void *modhandle, char *args)
{
	int		err;
    char	*confname = csconfig_next_arg(&args);

	if (!modhandle || !confname || !*confname)
	{
		si_log(SI_LOG_ERROR, "mod_chanjupe: not enough arguments passed to module");
		return EINVAL;
	}

	SIModHandle = modhandle;

	memset(&SIDKConf, 0, sizeof(SIDKConf));

	err = csconfig_read(confname, Commands, &SIDKConf, _conf_error_func);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_chanjupe: couldn't load \"%s\": %d",
			confname, err);
		_conf_deinit();
		return err;
	}

	return err;
}

static void _simodule_deinit(void)
{
	_conf_deinit();
}

static void _client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	SI_CHANJUPE_KILL	*uh;

	if (CliIsOper(cptr))
		return;

	uh = _chanjupe_find(SIDKConf.cjc_chanjupes, chptr->ch_name, 0);
	if (uh)
	{
		if (!(SIDKConf.cjc_flags & SI_UHJC_FLAGS_KILL_DISABLE))
		{
	        siuplink_printf(":%s KILL %s :Clients joining '%s' are juped",
	            SIMe->c_name, cptr->c_name, uh->cjk_string);
		}
	}
}

static int _conf_flags(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANJUPE_CONF	*cjc = *((SI_CHANJUPE_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flag specified");
		return EINVAL;
	}

	for(;;)
	{
		if (!strcasecmp(arg, "kill_disable"))
		{
			cjc->cjc_flags |= SI_UHJC_FLAGS_KILL_DISABLE;
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

static int _conf_chanjupe(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANJUPE_CONF		*cjc = *((SI_CHANJUPE_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);
#if 0
	char				*flag = csconfig_next_arg(&args);
#endif
	u_int				len;
	SI_CHANJUPE_KILL		*ig;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no userhost specified");
		return EINVAL;
	}

	len = strlen(arg);

	ig = (SI_CHANJUPE_KILL *)simempool_alloc(&SIDKMemPool,
							sizeof(SI_CHANJUPE_KILL) + len);
	if (!ig)
		return ENOMEM;

	strcpy(ig->cjk_string, arg);
	ig->cjk_flags = 0;
	ig->cjk_next = cjc->cjc_chanjupes;
	cjc->cjc_chanjupes = ig;

	return 0;
}

static int _do_chanjupes(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	SI_CHANJUPE_KILL			*uh;

	assert(from && to && CliIsOper(from));

	if (!simodadmin_client_is_authed(from))
	{
		si_log(SI_LOG_NOTICE, "mod_chanjupe: \"chanjupes%s%s\" tried from non-admin: %s!%s on %s [%s]",
            args?" ":"",args?args:"",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Unknown command.");
        return 0;
	}

	siclient_send_privmsg(to, from, "CHANJUPES:");

	for(uh=SIDKConf.cjc_chanjupes;uh;uh=uh->cjk_next)
	{
		siclient_send_privmsg(to, from, "%s",
			uh->cjk_string);
	}

	siclient_send_privmsg(to, from, "End of list");

	si_log(SI_LOG_NOTICE, "mod_chanjupe: \"chanjupes\" from: %s!%s on %s [%s]",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);

	return 0;

}


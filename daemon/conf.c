/*
**   IRC services -- conf.c
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

RCSID("$Id: conf.c,v 1.3 2004/03/30 22:23:43 cbehrens Exp $");

static int _conf_outofmemory(void *csc_cookie);
static int _conf_server_cpasswd(void *csc_cookie, char *args, void **cb_arg);
static int _conf_server_npasswd(void *csc_cookie, char *args, void **cb_arg);
static int _conf_server_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_server_bindaddr(void *csc_cookie, char *args, void **cb_arg);
static int _conf_server_host(void *csc_cookie, char *args, void **cb_arg);
static int _conf_listen_bindaddr(void *csc_cookie, char *args, void **cb_arg);
static int _conf_listen_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_server(void *csc_cookie, char *args, void **cb_arg);
static int _conf_name(void *csc_cookie, char *args, void **cb_arg);
static int _conf_info(void *csc_cookie, char *args, void **cb_arg);
static int _conf_listen(void *csc_cookie, char *args, void **cb_arg);
static int _conf_timeout(void *csc_cookie, char *args, void **cb_arg);
static int _conf_module(void *csc_cookie, char *args, void **cb_arg);


static CSC_COMMAND ServerCommands[] = {
	{ "cpasswd",	_conf_server_cpasswd,	0,		NULL },
	{ "npasswd",	_conf_server_npasswd,	0,		NULL },
	{ "flag",		_conf_server_flags,		0,		NULL },
	{ "flags",		_conf_server_flags,		0,		NULL },
	{ "host",		_conf_server_host,		0,		NULL },
	{ "bindaddr",	_conf_server_bindaddr,	0,		NULL },
	{ NULL,			NULL,					0,		NULL }
};

static CSC_COMMAND ListenCommands[] = {
	{ "bindaddr",	_conf_listen_bindaddr,	0,		NULL },
	{ "flag",		_conf_listen_flags,		0,		NULL },
	{ "flags",		_conf_listen_flags,		0,		NULL },
	{ NULL,			NULL,					0,		NULL }
};

static CSC_COMMAND Commands[] = {
	{ "server",		_conf_server,			0,		&ServerCommands },
	{ "name",		_conf_name,				0,		NULL },
	{ "info",		_conf_info,				0,		NULL },
	{ "listen",		_conf_listen,			0,		&ListenCommands },
	{ "listener",	_conf_listen,			0,		&ListenCommands },
	{ "timeout",	_conf_timeout,			0,		NULL },
	{ "module",		_conf_module,			0,		NULL },
	{ NULL,			NULL,					0,		NULL }
};


static int _conf_outofmemory(void *csc_cookie)
{
	csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "out of memory");
	return ENOMEM;
}

static int _conf_noarg(void *csc_cookie)
{
	csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument given");
	return EINVAL;
}

static int _conf_alreadyset(void *csc_cookie)
{
	csconfig_error(csc_cookie, CSC_SEVERITY_WARNING,
			"option already configured");
	return EALREADY;
}

static int _conf_server_cpasswd(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
	SI_SERVERCONF	*sc = *(SI_SERVERCONF **)cb_arg;

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->sc_cpasswd)
	{
		return _conf_alreadyset(csc_cookie);
	}

	sc->sc_cpasswd = simempool_strdup(&SIConfPool, arg);
	if (!sc->sc_cpasswd)
	{
		return _conf_outofmemory(csc_cookie);
	}
	
	return 0;
}

static int _conf_server_npasswd(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
	SI_SERVERCONF	*sc = *(SI_SERVERCONF **)cb_arg;

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->sc_npasswd)
	{
		return _conf_alreadyset(csc_cookie);
	}

	sc->sc_npasswd = simempool_strdup(&SIConfPool, arg);
	if (!sc->sc_npasswd)
	{
		return _conf_outofmemory(csc_cookie);
	}

	return 0;
}

static int _conf_server_flags(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
	SI_SERVERCONF	*sc = *(SI_SERVERCONF **)cb_arg;

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	for(;;)
	{
		if (!strcasecmp(arg, "autoconn"))
		{
			sc->sc_flags |= SI_SC_FLAGS_AUTOCONN;
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

static int _conf_server_bindaddr(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
	SI_SERVERCONF	*sc = *(SI_SERVERCONF **)cb_arg;

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->sc_sin_src.sin_addr.s_addr != htonl(INADDR_ANY))
	{
		return _conf_alreadyset(csc_cookie);
	}

	if ((sc->sc_sin_src.sin_addr.s_addr = inet_addr(arg)) == -1)
	{
		sc->sc_sin_src.sin_addr.s_addr = htonl(INADDR_ANY);
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR,
			"invalid ip address");
		return EINVAL;
	}

	return 0;
}

static int _conf_server_host(void *csc_cookie, char *args, void **cb_arg)
{
	char			*host = csconfig_next_arg(&args);
	char			*port = csconfig_next_arg(&args);
	SI_SERVERCONF	*sc = *(SI_SERVERCONF **)cb_arg;

	if (!host || !*host)
	{
		return _conf_noarg(csc_cookie);
	}

	if ((sc->sc_sin.sin_addr.s_addr = inet_addr(host)) == -1)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid ip address");
		return EINVAL;
	}
	if (port && *port)
	{
		if (!(sc->sc_sin.sin_port = htons(atoi(port))))
		{
			csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid port");
			return EINVAL;
		}
	}

	return 0;
}

static int _conf_listen_bindaddr(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
	SI_LISTENCONF	*lc = *(SI_LISTENCONF **)cb_arg;

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	if (lc->lc_sin.sin_addr.s_addr != INADDR_ANY)
	{
		return _conf_alreadyset(csc_cookie);
	}

	if ((lc->lc_sin.sin_addr.s_addr = inet_addr(arg)) == -1)
	{
		lc->lc_sin.sin_addr.s_addr = INADDR_ANY;
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR,
			"invalid ip address");
		return EINVAL;
	}

	return 0;
}

static int _conf_listen_flags(void *csc_cookie, char *args, void **cb_arg)
{
	char			*arg = csconfig_next_arg(&args);
#if 0
	SI_LISTENCONF	*lc = *(SI_LISTENCONF **)cb_arg;
#endif

	if (!arg || !*arg)
	{
		return _conf_noarg(csc_cookie);
	}

	for(;;)
	{
#if 0
		if (!strcasecmp(arg, "some_flag"))
		{
		}
		else
#endif
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

static int _conf_server(void *csc_cookie, char *args, void **cb_arg)
{
	SI_SERVERCONF	*sc = (SI_SERVERCONF *)simempool_calloc(&SIConfPool, 1, sizeof(SI_SERVERCONF));
	SI_CONF			*c = *(SI_CONF **)cb_arg;
	char			*name;

	if (!sc)
	{
		return _conf_outofmemory(csc_cookie);
	}

	name = csconfig_next_arg(&args);
	if (!name || !*name)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no server name given");
		simempool_free(sc);
		return EINVAL;
	}

	sc->sc_name = simempool_strdup(&SIConfPool, name);
	if (!sc->sc_name)
	{
		simempool_free(sc);
		return _conf_outofmemory(csc_cookie);
	}
	sc->sc_conf = c;
	sc->sc_prev = NULL;
	if ((sc->sc_next = c->c_servers) != NULL)
		sc->sc_next->sc_prev = sc;
	c->c_servers = sc;
	sc->sc_sin.sin_port = htons(6667);
	sc->sc_sin.sin_family = AF_INET;
	sc->sc_sin_src.sin_family = AF_INET;
	sc->sc_sin_src.sin_addr.s_addr = htonl(INADDR_ANY);

	*cb_arg = (void *)sc;
	return 0;
}

static int _conf_listen(void *csc_cookie, char *args, void **cb_arg)
{
	SI_LISTENCONF	*lc = (SI_LISTENCONF *)simempool_calloc(&SIConfPool, 1, sizeof(SI_LISTENCONF));
	SI_CONF			*sc = *(SI_CONF **)cb_arg;
	char			*arg = csconfig_next_arg(&args);

	if (!lc)
	{
		return _conf_outofmemory(csc_cookie);
	}

	if (!arg || !*arg)
	{
		simempool_free(lc);
		return _conf_noarg(csc_cookie);
	}

	if (!(lc->lc_sin.sin_port = htons(atoi(arg))))
	{
		simempool_free(lc);
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid port");
		return EINVAL;
	}

	lc->lc_sin.sin_addr.s_addr = INADDR_ANY;
	lc->lc_sin.sin_family = AF_INET;
	lc->lc_fd = -1;
	lc->lc_conf = sc;
	lc->lc_prev = NULL;
	if ((lc->lc_next = sc->c_listeners) != NULL)
		lc->lc_next->lc_prev = lc;
	sc->c_listeners = lc;

	*cb_arg = (void *)lc;

	return 0;
}

static int _conf_name(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CONF	*sc = *(SI_CONF **)cb_arg;
	char	*name = csconfig_next_arg(&args);

	if (!name || !*name)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->c_name)
	{
		return _conf_alreadyset(csc_cookie);
	}

	sc->c_name = simempool_strdup(&SIConfPool, name);
	if (!sc->c_name)
	{
		return _conf_outofmemory(csc_cookie);
	}

	return 0;
}

static int _conf_info(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CONF	*sc = *(SI_CONF **)cb_arg;
	char	*info = args;

	if (!info || !*info)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->c_info)
	{
		return _conf_alreadyset(csc_cookie);
	}

	sc->c_info = simempool_strdup(&SIConfPool, info);
	if (!sc->c_info)
	{
		return _conf_outofmemory(csc_cookie);
	}

	return 0;
}

static int _conf_timeout(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CONF	*sc = *(SI_CONF **)cb_arg;
	char	*to = csconfig_next_arg(&args);

	if (!to || !*to)
	{
		return _conf_noarg(csc_cookie);
	}

	if (sc->c_timeout)
	{
		return _conf_alreadyset(csc_cookie);
	}

	sc->c_timeout = atoi(to);

	return 0;
}

static int _conf_module(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CONF			*sc = *(SI_CONF **)cb_arg;
	SI_MODULECONF	*mc;
	char			*name = csconfig_next_arg(&args);
	char			*args_val = NULL;
	char			*opt;
	char			*val;
	u_int			debug = 0;

	if (!name || !*name)
	{
		return _conf_noarg(csc_cookie);
	}

	for(mc=sc->c_modules;mc;mc=mc->mc_next)
	{
		if (!strcmp(mc->mc_name, name))
		{
			csconfig_error(csc_cookie, CSC_SEVERITY_WARNING,
				"module has already been specified");
			return EALREADY;
		}
	}

	while((opt = csconfig_next_arg(&args)) != NULL)
	{
		val = strchr(opt, '=');
		if (val)
			*val++ = '\0';
		if (!strcasecmp(opt, "debug"))
		{
			debug = val ? atoi(val) : 1;
			if (debug < 0 || debug > 10)
			{
				csconfig_error(csc_cookie, CSC_SEVERITY_WARNING,
					"invalid debug level specified for module");
			}
		}
		else if (!strcasecmp(opt, "args"))
		{
			args_val = val ? val : "";
		}
	}

	mc = (SI_MODULECONF *)simempool_calloc(&SIConfPool, 1, sizeof(SI_MODULECONF));
	if (!mc)
		return _conf_outofmemory(csc_cookie);

	mc->mc_name = simempool_strdup(&SIConfPool, name);
	if (!mc->mc_name)
	{
		simempool_free(mc);
		return _conf_outofmemory(csc_cookie);
	}
	if (args_val && *args_val)
		mc->mc_arg = simempool_strdup(&SIConfPool, args_val);

	mc->mc_debug = debug;
	mc->mc_next = NULL;
	mc->mc_prev = sc->c_lastmodule;

	if (sc->c_lastmodule)
		sc->c_lastmodule->mc_next = mc;
	else
		sc->c_modules = mc;
	sc->c_lastmodule = mc;

	return 0;
}

static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno)
{
	int		level = LOG_DEBUG;
	char	*string = "";

	if (severity == CSC_SEVERITY_WARNING)
	{
		level = LOG_WARNING;
		string = "Warning";
	}
	else
	{
		level = LOG_ERR;
		string = "Error";
	}
	syslog(level, "%s:%d: %s%s%s%s",
			filename,
			lineno,
			string,
			*string ? ":" : string,
			*string ? " " : string,
			error);
	fprintf(stderr, "%s:%d: %s%s%s%s\n",
			filename,
			lineno,
			string,
			*string ? ":" : string,
			*string ? " " : string,
			error);
}

void siconf_free(SI_CONF *conf, void (*listener_del)(SI_LISTENCONF *lc))
{
	SI_SERVERCONF	*sc;
	SI_SERVERCONF	*nextsc;
	SI_MODULECONF	*mc;
	SI_MODULECONF	*mcnext;
	SI_LISTENCONF	*lc;
	SI_LISTENCONF	*nextlc;

	for(lc=conf->c_listeners;lc;lc=nextlc)
	{
		nextlc = lc->lc_next;

		listener_del(lc);
		simempool_free(lc);
	}
	for(sc=conf->c_servers;sc;sc=nextsc)
	{
		nextsc = sc->sc_next;

		if (sc->sc_cpasswd)
			simempool_free(sc->sc_cpasswd);
		if (sc->sc_npasswd)
			simempool_free(sc->sc_npasswd);
		if (sc->sc_name)
			simempool_free(sc->sc_name);
		simempool_free(sc);
	}
	for(mc=conf->c_modules;mc;mc=mcnext)
	{
		mcnext = mc->mc_next;

		if (mc->mc_arg)
			simempool_free(mc->mc_arg);
		if (mc->mc_name)
			simempool_free(mc->mc_name);
		simempool_free(mc);
	}
	if (conf->c_name)
		simempool_free(conf->c_name);
	if (conf->c_info)
		simempool_free(conf->c_info);
	simempool_free(conf);
}

int siconf_load(char *filename, SI_CONF **scptr)
{
	int		err;
	SI_CONF	*sc = (SI_CONF *)simempool_calloc(&SIConfPool, 1, sizeof(SI_CONF));

	if (!sc)
		return ENOMEM;

	err = csconfig_read(filename, Commands, sc, _conf_error_func);
	if (err)
	{
		simempool_free(sc);
	}
	else
	{
		*scptr = sc;
	}

	return err;
}

int siconf_parse(SI_CONF *conf, SI_CONF *new_conf, void (*listener_add)(SI_LISTENCONF *lc), void (*listener_del)(SI_LISTENCONF *lc))
{
	SI_SERVERCONF	*sc;
	SI_SERVERCONF	*sc2;
	SI_SERVERCONF	*scnext;
	SI_SERVERCONF	*scprev;
	SI_MODULECONF	*mc;
	SI_MODULECONF	*mc2;
	SI_MODULECONF	*mcnext;
	SI_MODULECONF	*mcprev;
	SI_LISTENCONF	*lc;
	SI_LISTENCONF	*lc2;
	SI_LISTENCONF	*lcnext;
	SI_LISTENCONF	*lcprev;
	const char		*errptr;
	int				err;

	if (!conf)
		return EINVAL;

	if (new_conf != conf)
	{
		/* remove all modules first, but in reverse order... */

		for(mc=conf->c_lastmodule;mc;mc=mcprev)
		{
			mcprev = mc->mc_prev;

#ifdef DEBUG
			printf("unloading %s\n", mc->mc_name);
#endif
			err = simodule_unload(mc->mc_name, &errptr);
			if (err && (err != ENOENT))
			{
				si_log(SI_LOG_ERROR,
					"Couldn't unload module \"%s\": %s",
					mc->mc_name, errptr?errptr:"Unknown error");
			}
			else
			{
				if (err == ENOENT)
				{
					si_log(SI_LOG_ERROR,
						"Module \"%s\" already unloaded: %s",
						mc->mc_name, errptr?errptr:"Unknown error");
				}
				else
					si_log(SI_LOG_NOTICE,
						"Module \"%s\" unloaded.",
						mc->mc_name);

				si_ll_del_from_list(conf->c_modules, mc, mc);

				if (mc->mc_arg)
					simempool_free(mc->mc_arg);
				simempool_free(mc->mc_name);
				simempool_free(mc);
			}
		}
		conf->c_lastmodule = conf->c_modules;
		if (conf->c_lastmodule)
			while(conf->c_lastmodule->mc_next)
				conf->c_lastmodule = conf->c_lastmodule->mc_next;
	}

	/* first find new ones to add */
	if (new_conf)
	{
		for(lc=new_conf->c_listeners;lc;lc=lc->lc_next)
		{
			if (conf == new_conf)
			{
				assert(lc->lc_fd == -1);
#ifdef DEBUG
				printf("adding listener %s:%i -- initial\n",
						inet_ntoa(lc->lc_sin.sin_addr),
						ntohs(lc->lc_sin.sin_port));
#endif
				listener_add(lc);
				continue;
			}
			for(lc2=conf->c_listeners;lc2;lc2=lc2->lc_next)
			{
				if ((lc2->lc_sin.sin_addr.s_addr ==
						lc->lc_sin.sin_addr.s_addr) &&
					(lc2->lc_sin.sin_port == lc->lc_sin.sin_port))
				break;
			}
			if (!lc2) /* add this one */
			{
				lc2 = (SI_LISTENCONF *)simempool_alloc(&SIConfPool, sizeof(SI_LISTENCONF));
				if (!lc2)
				{
					return ENOMEM;
				}
				lc2->lc_fd = -1;
				lc2->lc_sin = lc->lc_sin;
#ifdef DEBUG
				printf("adding new listener %s:%i\n",
						inet_ntoa(lc->lc_sin.sin_addr),
						ntohs(lc->lc_sin.sin_port));
#endif
				listener_add(lc2);
				lc2->lc_prev = NULL;
				if ((lc2->lc_next = conf->c_listeners) != NULL)
					lc2->lc_next->lc_prev = lc2;
				conf->c_listeners = lc2;
				/* fall through */
			}
#ifdef DEBUG
			else if (lc2->lc_fd < 0)
			{
				listener_add(lc2);
				printf("updating listener %s:%i\n",
						inet_ntoa(lc2->lc_sin.sin_addr),
						ntohs(lc2->lc_sin.sin_port));
			}
#endif
			/* exists, copy changes */
			lc2->lc_flags = lc->lc_flags;
			lc2->lc_conf = conf;
		}
		for(sc=new_conf->c_servers;sc;sc=sc->sc_next)
		{
			if (conf == new_conf)
				continue;
			if (!sc->sc_name || !sc->sc_cpasswd || !sc->sc_npasswd)
			{
				continue;
			}
			for(sc2=conf->c_servers;sc2;sc2=sc2->sc_next)
			{
				if (!ircd_strcmp(sc2->sc_name, sc->sc_name))
					break;
			}
			if (!sc2)
			{
				sc2 = (SI_SERVERCONF *)simempool_alloc(&SIConfPool, sizeof(SI_SERVERCONF));
				if (!sc2)
				{
					return ENOMEM;
				}
				sc2->sc_name = simempool_strdup(&SIConfPool, sc->sc_name);
				sc2->sc_cpasswd = NULL;
				sc2->sc_npasswd = NULL;
				sc2->sc_prev = NULL;
				if ((sc2->sc_next = conf->c_servers) != NULL)
					sc2->sc_next->sc_prev = sc2;
				conf->c_servers = sc2;
			}
			if (sc2->sc_cpasswd)
				simempool_free(sc2->sc_cpasswd);
			if (sc2->sc_npasswd)
				simempool_free(sc2->sc_npasswd);
			sc2->sc_cpasswd = simempool_strdup(&SIConfPool, sc->sc_cpasswd);
			sc2->sc_npasswd = simempool_strdup(&SIConfPool, sc->sc_npasswd);
			sc2->sc_sin = sc->sc_sin;
			sc2->sc_flags = sc->sc_flags;
			sc2->sc_conf = conf;
		}
		for(mc=new_conf->c_modules;mc;mc=mcnext)
		{
			mcnext = mc->mc_next;

			if (conf != new_conf)
			{
				for(mc2=conf->c_modules;mc2;mc2=mc2->mc_next)
				{
					if (!strcmp(mc2->mc_name, mc->mc_name))
						break;
				}
				if (mc2)
				{
					continue;	/* this module couldn't be unloaded :-/ */
				}
			}
#ifdef DEBUG
			printf("loading %s\n", mc->mc_name);
#endif
			err = simodule_load(mc->mc_name, mc->mc_arg, mc->mc_debug, &errptr);
			if (err && (err != EALREADY))
			{
				si_log(SI_LOG_ERROR,
					"Couldn't load module \"%s\": %s",
					mc->mc_name, errptr?errptr:"Unknown error");
				if (new_conf == conf)
				{
					if (mc == conf->c_lastmodule)
						conf->c_lastmodule = mc->mc_prev;
					si_ll_del_from_list(conf->c_modules, mc, mc);

					if (mc->mc_arg)
						simempool_free(mc->mc_arg);
					simempool_free(mc->mc_name);
					simempool_free(mc);
				}
				continue;
			}
			else if (err)
			{
				syslog(LOG_ERR,
					"Couldn't load module \"%s\": %s",
					mc->mc_name, errptr?errptr:"Unknown error");
			}
			else
				syslog(LOG_NOTICE, "Module \"%s\" has been loaded.",
						mc->mc_name);

			if (new_conf != conf)
			{
				if (mc == new_conf->c_lastmodule)
					new_conf->c_lastmodule = mc->mc_prev;
				si_ll_del_from_list(new_conf->c_modules, mc, mc);
				mc->mc_next = NULL;
				mc->mc_prev = conf->c_lastmodule;
				if (conf->c_lastmodule)
					conf->c_lastmodule->mc_next = mc;
				else
					conf->c_modules = mc;
				conf->c_lastmodule = mc;
			}
		}
	}

	if (conf == new_conf)
		return 0;

	lc2 = NULL;
	for(lcprev=NULL,lc=conf->c_listeners;lc;lcprev=lc,lc=lcnext)
	{
		lcnext = lc->lc_next;

		if (new_conf)
		{
			for(lc2=new_conf->c_listeners;lc2;lc2=lc2->lc_next)
			{
				if ((lc2->lc_sin.sin_addr.s_addr ==
							lc->lc_sin.sin_addr.s_addr) &&
					(lc2->lc_sin.sin_port == lc->lc_sin.sin_port))
				break;
			}
		}
		if (!lc2)
		{
#ifdef DEBUG
			printf("removing listener %s:%i\n",
						inet_ntoa(lc->lc_sin.sin_addr),
						ntohs(lc->lc_sin.sin_port));
#endif
			listener_del(lc);
			simempool_free(lc);
			if (lcprev)
				lcprev->lc_next = lcnext;
			else
				conf->c_listeners = lcnext;
			lc = lcprev;
			continue;
		}
	}
	sc2 = NULL;
	for(scprev=NULL,sc=conf->c_servers;sc;scprev=sc,sc=scnext)
	{
		scnext = sc->sc_next;

		if (new_conf)
		{
			for(sc2=new_conf->c_servers;sc2;sc2=sc2->sc_next)
			{
				if ((sc2->sc_sin.sin_addr.s_addr ==
							sc->sc_sin.sin_addr.s_addr) &&
					(sc2->sc_sin.sin_port == sc->sc_sin.sin_port))
				break;
			}
		}
		if (!sc2)
		{
			simempool_free(sc->sc_npasswd);
			simempool_free(sc->sc_cpasswd);
			simempool_free(sc->sc_name);
			simempool_free(sc);
			if (scprev)
				scprev->sc_next = scnext;
			else
				conf->c_servers = scnext;
			sc = scprev;
			continue;
		}
	}

	if (new_conf && (new_conf != conf))
		siconf_free(new_conf, listener_del);

	return 0;
}

SI_SERVERCONF *siconf_find_server(SI_CONF *conf, char *name, struct sockaddr_in *sin)
{
	SI_SERVERCONF	*sc;
	
	for(sc=conf->c_servers;sc;sc=sc->sc_next)
	{
		if (!ircd_strcmp(name, sc->sc_name) &&
				(sin->sin_addr.s_addr == sc->sc_sin.sin_addr.s_addr))
			return sc;
	}
	return NULL;
}

SI_SERVERCONF *siconf_find_autoconn_server(SI_CONF *conf, time_t thetime)
{
	SI_SERVERCONF	*sc;
	
	for(sc=conf->c_servers;sc;sc=sc->sc_next)
	{
		if ((sc->sc_flags & SI_SC_FLAGS_AUTOCONN) &&
				(thetime > sc->sc_tryconnecttime))
		{
			sc->sc_tryconnecttime = thetime;
			return sc;
		}
	}
	return NULL;
}


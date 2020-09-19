/*
**   IRC services -- conf.h
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

#ifndef __CONF_H__
#define __CONF_H__

/*
** $Id: conf.h,v 1.3 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hash.h"
#include "module.h"

typedef struct _si_listenconf	SI_LISTENCONF;
typedef struct _si_serverconf	SI_SERVERCONF;
typedef struct _si_moduleconf	SI_MODULECONF;
typedef struct _si_conf			SI_CONF;

struct _si_listenconf
{
	int					lc_fd;
	SI_CONN				*lc_con;

	struct sockaddr_in	lc_sin;
	u_int				lc_flags;

	SI_CONF				*lc_conf;

	SI_LISTENCONF		*lc_next;
	SI_LISTENCONF		*lc_prev;
};

struct _si_serverconf
{
	char				*sc_name;
	char				*sc_cpasswd;
	char				*sc_npasswd;
#define SI_SC_FLAGS_AUTOCONN	0x001
	u_int				sc_flags;
	struct sockaddr_in	sc_sin;
	struct sockaddr_in	sc_sin_src;
	time_t				sc_tryconnecttime;

	SI_CONF				*sc_conf;

	SI_SERVERCONF		*sc_next;
	SI_SERVERCONF		*sc_prev;
};

struct _si_moduleconf
{
	char				*mc_name;
	char				*mc_arg;
#define SI_MC_FLAGS_LOADED	0x001
	u_int				mc_flags;
	u_int				mc_debug;

	SI_MODULECONF		*mc_next;
	SI_MODULECONF		*mc_prev;
};

struct _si_conf
{
	char				*c_name;
	char				*c_info;
	u_int				c_flags;
	u_int				c_timeout;

	SI_LISTENCONF		*c_listeners;
	SI_SERVERCONF		*c_servers;
	SI_MODULECONF		*c_modules;
	SI_MODULECONF		*c_lastmodule;
};


void siconf_free(SI_CONF *conf, void (*listener_del)(SI_LISTENCONF *lc));
int siconf_load(char *filename, SI_CONF **scptr);
int siconf_parse(SI_CONF *conf, SI_CONF *new_conf, void (*listener_add)(SI_LISTENCONF *lc), void (*listener_del)(SI_LISTENCONF *lc));
SI_SERVERCONF *siconf_find_server(SI_CONF *conf, char *name, struct sockaddr_in *sin);
SI_SERVERCONF *siconf_find_autoconn_server(SI_CONF *conf, time_t thetime);


#endif

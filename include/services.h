/*
**   IRC services -- services.h
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

#ifndef __SERVICES_H__
#define __SERVICES_H__

/*
** $Id: services.h,v 1.5 2004/08/26 18:02:01 cbehrens Exp $
*/

#define SIMAXPACKET		511

#include "setup.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include "match.h"
#include "csconfig.h"
#include "conf.h"
#include "queue.h"
#include "commands.h"
#include "misc.h"
#include "hash.h"
#include "module.h"
#include "timedcbs.h"
#include "mempool.h"

#define VERSION "Services-v3.21"

typedef struct _si_stats	SI_STATS;

struct _si_stats
{
	u_int	st_numclients;
	u_int	st_maxclients;
	time_t	st_maxclients_time;

	u_int	st_numgservers;
	u_int	st_maxgservers;
	time_t	st_maxgservers_time;

	u_int	st_nummservers;
	u_int	st_maxmservers;
	time_t	st_maxmservers_time;
	u_int	st_serversbursting;

	u_int	st_numchannels;
	u_int	st_maxchannels;
	time_t	st_maxchannels_time;

	u_int	st_numinvis;
	u_int	st_numopers;
};

#define si_ll_add_to_list(__bucket, __struct, __member)						\
do {																		\
	(__struct) -> __member ## _prev = NULL;									\
	if (((__struct) -> __member ## _next = (__bucket)) != NULL)				\
		(__struct) -> __member ## _next -> __member ## _prev = (__struct);	\
	(__bucket) = (__struct);												\
} while(0)

#define si_ll_del_from_list(__bucket, __struct, __member)					\
do {																		\
	if ((__struct) -> __member ## _prev)									\
		(__struct) -> __member ## _prev-> __member ## _next =				\
			(__struct) -> __member ## _next;								\
	else																	\
		(__bucket) = (__struct) -> __member ## _next;						\
	if ((__struct) -> __member ## _next)									\
		(__struct) -> __member ## _next -> __member ## _prev =				\
			(__struct) -> __member ## _prev;								\
} while(0)

#define TS_CURRENT	5
#define TS_MIN		5
#define NICKLEN		9
#define SERVERLEN	63

extern time_t		Now;
extern SI_STATS		SIStats;
extern time_t		SIAutoconn_time;
extern u_int		SIAutoconn_num;
extern SI_MEMPOOL	SIModulePool;
extern SI_MEMPOOL	SIMainPool;
extern SI_MEMPOOL	SIConfPool;
extern SI_MEMPOOL	SIQueuePool;

SI_SERVERCONF *_siconf_find_server(char *servername, struct sockaddr_in *sin);

#endif

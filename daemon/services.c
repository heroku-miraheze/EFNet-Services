/*
**   IRC services -- services.c
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

RCSID("$Id: services.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

static char					*SIConffile = NULL;
static int					SISigHuped = 0;
static int					SISigTermed = 0;
static SI_CONNINFO			SIConninfo;
static SI_CONF				*SIConf = NULL;

time_t						Now;
time_t						SIAutoconn_time = 0;
u_int						SIAutoconn_num = 0;
SI_STATS					SIStats;
SI_MEMPOOL					SIModulePool;
SI_MEMPOOL					SIMainPool;
SI_MEMPOOL					SIConfPool;
SI_MEMPOOL					SIQueuePool;

/* static function prototypes */

static void _listener_add(SI_LISTENCONF *lc);
static void _listener_del(SI_LISTENCONF *lc);
static void _sig_handler(int signo);
static int _siconn_start_autoconn(SI_SERVERCONF *sc);
static int _io_loop();
static void _usage(int l);


/* start of static functions */

static void _listener_add(SI_LISTENCONF *lc)
{
	int		err;

	if (lc->lc_fd >= 0)
		return;

	lc->lc_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (lc->lc_fd >= 0)
	{
		int		on = 1;

		setsockopt(lc->lc_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
		on = 1;
		setsockopt(lc->lc_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));

		err = bind(lc->lc_fd, (struct sockaddr *)
					&(lc->lc_sin), sizeof(struct sockaddr_in));
		if (!err)
		{
			err = listen(lc->lc_fd, 1024);
			if (err)
			{
				syslog(LOG_ERR,
					"listen() failed for listener (%s:%d): %m",
					inet_ntoa(lc->lc_sin.sin_addr),
					ntohs(lc->lc_sin.sin_port));
				close(lc->lc_fd);
				lc->lc_fd = -1;
			}
			else
			{
				(void)siconn_add(&SIConninfo, lc->lc_fd, POLLIN,
						SI_CON_TYPE_LISTENER, lc, &(lc->lc_con));
			}
		}
		else
		{
			syslog(LOG_ERR,
				"bind() failed for listener (%s:%d): %m",
				inet_ntoa(lc->lc_sin.sin_addr),
				ntohs(lc->lc_sin.sin_port));
			close(lc->lc_fd);
			lc->lc_fd = -1;
		}
	}
	else
	{
		syslog(LOG_ERR, "socket() failed for listener (%s:%d): %m",
			inet_ntoa(lc->lc_sin.sin_addr),
			ntohs(lc->lc_sin.sin_port));
	}
}

static void _listener_del(SI_LISTENCONF *lc)
{
	if (lc->lc_con)
		siconn_del(lc->lc_con, NULL);
}

static void _sig_handler(int signo)
{
	switch(signo)
	{
		case SIGHUP:
			SISigHuped = 1;
			break;
		case SIGTERM:
			SISigTermed = 1;
			return;
		default:
			break;
	}
}

static int _siconn_start_autoconn(SI_SERVERCONF *sc)
{
	struct sockaddr_in	sin;
	struct sockaddr_in	sin2;
	int					s;
	int					ret;
	SI_CONN				*con;

	sin = sc->sc_sin;
	sin2 = sc->sc_sin_src;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return errno;
	ret = bind(s, (struct sockaddr *)&sin2, sizeof(sin2));
	if (ret < 0)
	{
		ret = errno;
		close(s);
		return ret;
	}
	ret = connect(s, (struct sockaddr *)&sin, sizeof(sin));
	if (ret < 0)
	{
		if (errno != EINPROGRESS)
		{
			ret = errno;
			close(s);
			return ret;
		}
	}
	ret = siconn_add(&SIConninfo, s, POLLOUT, SI_CON_TYPE_UNKNOWN, NULL, &con);
	if (ret)
	{
		close(s);
		return ret;
	}
	con->con_flags |= SI_CON_FLAGS_CONNECTING;
	con->con_rem_sin = sin;
	SIAutoconn_num = 1;

	if (sc->sc_cpasswd && *sc->sc_cpasswd)
		siconn_printf(con, "PASS %s :TS", sc->sc_cpasswd);
	send_capabilities(con);
	siconn_printf(con, "SERVER %s 1 :%s",
			SIMyName, SIMyInfo);

	return 0;
}

static int _io_loop()
{
	int				ret;
	int				ms;
	char			read_buffer[1024*1024];
	SI_SERVERCONF	*sc;
	time_t			next_check;

	SIAutoconn_time = Now;

	if ((sc = siconf_find_autoconn_server(SIConf, SIAutoconn_time)) != NULL)
	{
		_siconn_start_autoconn(sc);
	}
	else
		SIAutoconn_time = Now + 30;

	for(;;)
	{
		if (SISigTermed)
			return 0;
		if (SISigHuped)
		{
			SI_CONF	*siconf;
			int		err;

			SISigHuped = 0;

			err = siconf_load(SIConffile, &siconf);
			if (err)
			{
				syslog(LOG_ERR, "Couldn't reload \"%s\", siconf_load() failed: %d\n", SIConffile, err);
			}
			else
			{
				SIConninfo.ci_timeout = siconf->c_timeout;
				err = siconf_parse(SIConf, siconf,
					_listener_add, _listener_del);
				if (err)
				{
					syslog(LOG_ERR, "Couldn't reload \"%s\", siconf_parse() failed: %d\n", SIConffile, err);
					siconf_free(siconf, _listener_del);
				}
				else
				{
					syslog(LOG_ERR, "\"%s\" has been reloaded.",
							SIConffile);
				}
			}
		}
		if (!SIAutoconn_num && (SIStats.st_nummservers == 1))
		{
			if (Now >= SIAutoconn_time)
			{
				if ((sc = siconf_find_autoconn_server(SIConf, SIAutoconn_time)) != NULL)
				{
					_siconn_start_autoconn(sc);
				}
				else
					SIAutoconn_time = Now + 30;
			}
		}

		sitimedcbs_check(&next_check);
		if (Now > next_check)
		{
			ms = 0;
		}
		else
		{
			ms = (next_check - Now) * 1000;
			if (ms > 30000)
				ms = 30000;
		}

		ret = siconn_poll(&SIConninfo, ms, read_buffer, sizeof(read_buffer));
		if (ret < 0)
			continue;
	}
	/* NOT REACHED */
}

SI_SERVERCONF	*_siconf_find_server(char *servername, struct sockaddr_in *sin)
{
	return siconf_find_server(SIConf, servername, sin);
}

static void _usage(int l)
{
	fprintf(stderr, "Usage: services [-b] [-h] [-n] config.file\n");
	if (l)
	{
		fprintf(stderr, "    -b  -- memory bounds checking\n");
		fprintf(stderr, "    -h  -- this usage\n");
		fprintf(stderr, "    -n  -- don't fork into background\n");
    }
}

int main(int argc, char **argv)
{
	struct sigaction	siga;
	int					err;
	char				*name;
	char				*info;
	int					nofork = 0;
	int					memflags = 0;

	simempool_init(&SIMainPool, "Main Pool", memflags);
	simempool_init(&SIQueuePool, "Queue Pool", memflags);
	simempool_init(&SIModulePool, "Module Pool", memflags);
	simempool_init(&SIConfPool, "Conf Pool", memflags);

	while((err = getopt(argc, argv, "bnh")) != EOF)
	{
		switch(err)
		{
			case 'b':
				memflags |= SI_MEMPOOL_FLAGS_BOUNDSCHECK;
				break;
			case 'h':
				_usage(1);
				exit(0);
			case 'n':
				nofork = 1;
				break;
			default:
				_usage(0);
				exit(-1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
	{
		_usage(0);
		exit(-1);
	}

#ifdef DEBUG
	setvbuf(stdout, NULL, _IOLBF, 1024);
#endif

	Now = time(NULL);

	openlog("services.int", LOG_NDELAY|LOG_PID, LOG_LOCAL4);

	memset(&SIConninfo, 0, sizeof(SIConninfo));
	memset(&SIStats, 0, sizeof(SIStats));
	SIStats.st_maxchannels_time = Now;
	SIStats.st_maxclients_time = Now;
	SIStats.st_maxgservers_time = Now;
	SIStats.st_maxmservers_time = Now;
	SIStats.st_numgservers = 1;
	SIStats.st_maxgservers = 1;
	SIStats.st_nummservers = 1;
	SIStats.st_maxmservers = 1;
	command_tree_init();

	err = simodule_init();
	if (err)
	{
		fprintf(stderr, "simodule_init() failed: %d\n", err);
		exit(err);
	}

	SIConffile = argv[0];

	err = siconf_load(SIConffile, &SIConf);
	if (err)
	{
		fprintf(stderr, "siconf_load() failed: %d\n", err);
		exit(err);
	}

	if (!SIConf->c_name || !SIConf->c_info)
	{
		fprintf(stderr, "No name or info specified in conf file.\n");
		exit(-1);
	}

	name = simempool_strdup(&SIMainPool, SIConf->c_name);
	info = simempool_strdup(&SIMainPool, SIConf->c_info);

	if (!name || !info)
	{
		fprintf(stderr, "Out of memory\n");
		exit(-1);
	}

	sihash_init(name, info);

	err = siconf_parse(SIConf, SIConf, _listener_add, _listener_del);
	if (err)
	{
		fprintf(stderr, "siconf_parse() failed: %d\n", err);
		exit(err);
	}

	SIConninfo.ci_timeout = SIConf->c_timeout;

	memset(&siga, 0, sizeof(siga));
	siga.sa_handler = _sig_handler;

	sigaction(SIGTERM, &siga, NULL);
	sigaction(SIGHUP, &siga, NULL);
	sigaction(SIGPIPE, &siga, NULL);

	if (!nofork)
	{
		if ((err = fork()))
		{
			if (err == -1)
			{
				fprintf(stderr, "Couldn't fork into the background\n");
			}
			else
			{   
				printf("Now running in the background\n");
				exit(0);
			}
		}
		else
		{
			closelog();
			openlog("services.int", LOG_NDELAY|LOG_PID, LOG_LOCAL4);
		}
	}    

	{
		FILE	*foo;

		if ((foo = fopen("services.pid", "w")) != NULL)
		{
			fprintf(foo, "%d\n", getpid());
			fclose(foo);
		}
	}

	err = _io_loop();

	syslog(LOG_NOTICE, "Shutting down...");

	/* clean up */
	(void)siconf_parse(SIConf, NULL, _listener_add, _listener_del);

	siuplink_drop();

	command_tree_deinit();

	syslog(LOG_NOTICE, "Shutdown complete.");

	exit(err);
}


/*
**   IRC services -- conn.h
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

#ifndef __CONN_H__
#define __CONN_H__

/*
** $Id: conn.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <sys/poll.h>
#include <sys/types.h>
#include "services.h"
#include "queue.h"

#define SI_SERVER_HASHSIZE	127
#define SI_CLIENT_HASHSIZE	((128*1024)-1)
#define SI_CHANNEL_HASHSIZE	((48*1024)-1)
#define SI_CLIENT_NAME_MAXSIZE		512

typedef struct _si_conntype		SI_CONNTYPE;
typedef struct _si_conn			SI_CONN;
typedef struct _si_conninfo		SI_CONNINFO;

struct _si_conntype
{
	int					(*ct_accept_cb)(SI_CONN *con);
	int					(*ct_connect_cb)(SI_CONN *con);
	int					(*ct_read_cb)(SI_CONN *con);
	int					(*ct_write_cb)(SI_CONN *con);
};

struct _si_conn
{
	int					con_fd;
	u_int				con_pfd_pos;

	struct sockaddr_in	con_rem_sin;

	time_t				con_firsttime;
	time_t				con_lasttime;

	char				con_inbuffer[SIMAXPACKET*2];
	char				*con_addpos;
	char				*con_bufpos;

	char				*con_passwd;

	SIQUEUE				con_outqueue;

#define SI_CON_TYPE_UNKNOWN			0
#define SI_CON_TYPE_LISTENER		1
#define SI_CON_TYPE_CLIENT			2
	u_int				con_type;
#define ConIsUnknown(__x)			((__x)->con_type == SI_CON_TYPE_UNKNOWN)
#define ConIsListener(__x)			((__x)->con_type == SI_CON_TYPE_LISTENER)
#define ConIsClient(__x)			((__x)->con_type == SI_CON_TYPE_CLIENT)

#define SI_CON_FLAGS_DEAD			0x001
#define SI_CON_FLAGS_EAT			0x002 /* eat til newline */
#define SI_CON_FLAGS_CONNECTING		0x004 /* connect() in progress */
#define SI_CON_FLAGS_CONNECTED		0x008
#define SI_CON_FLAGS_TS				0x010
#define SI_CON_FLAGS_PINGSENT		0x020
#define ConIsConnecting(__x)	((__x)->con_flags & SI_CON_FLAGS_CONNECTING)
#define ConDoesTS(__x)			((__x)->con_flags & SI_CON_FLAGS_TS)
	u_int				con_flags;

#define SI_CAPAB_VALUE_NOQUITSTORM      0x001
	u_int       		con_capab_flags;

	SI_CONNINFO			*con_conninfo;

	void				*con_arg;

	SI_CONN				*con_prev;
	SI_CONN				*con_next;
};

struct _si_conninfo
{
	struct pollfd		*ci_pfds;
	u_int				ci_pfds_num;
	u_int				ci_pfds_size;
	SI_CONN				*ci_conns_list;
	SI_CONN				**ci_conns_fd_array;
	u_int				ci_conns_fd_size;
	u_int				ci_timeout;
	time_t				ci_next_check;
};

#define siconn_fd(__x)		(__x)->con_fd
#define siconn_arg(__x)		(__x)->con_arg
#define siconn_passwd(__x)	(__x)->con_passwd

#ifndef NDEBUG
#define siconn_name(__x)	(								\
		ConIsClient(__x) ? 									\
				((SI_CLIENT *)(__x)->con_arg)->c_name :		\
			ConIsListener(__x) ? "*listener*" :				\
			ConIsUnknown(__x) ? "" : NULL)
#else
#define siconn_name(__x)	(								\
		ConIsClient(__x) ? 									\
				((SI_CLIENT *)(__x)->con_arg)->c_name :		\
			ConIsListener(__x) ? "*listener*" : "" )
#endif

#define siconn_ipport(__x)								\
			inet_ntoa((__x)->con_rem_sin.sin_addr),		\
			ntohs((__x)->con_rem_sin.sin_port)


int siconn_add(SI_CONNINFO *conninfo, int fd, int pollevents, u_int type, void *arg, SI_CONN **connptr);
int siconn_del(SI_CONN *con, char *string);
int siconn_poll(SI_CONNINFO *conninfo, u_int timeout, char *read_buffer, u_int read_buffer_len);
int siconn_printfv(SI_CONN *con, char *format, va_list ap);
int siconn_printf(SI_CONN *con, char *format, ...);
int siconn_flush(SI_CONN *con, struct iovec *iov, u_int iov_num);

#endif

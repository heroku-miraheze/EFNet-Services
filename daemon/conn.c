/*
**   IRC services -- conn.c
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
#include "conn.h"

RCSID("$Id: conn.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

static int __conn_parse(SI_CONN *con, char *command);
static int _conn_parse(SI_CONN *con, char *buf, register char *endptr);


/*
** __conn_parse() return values:
**    0 == no error
**  < 0 == 'con' was deleted...don't access it!
**  > 0 == 'con' should be deleted...
*/
static int __conn_parse(SI_CONN *con, char *command)
{
	SI_CLIENT	*lptr;
	SI_CLIENT	*pptr = NULL;
	char		*sender = NULL;
	char		*ptr;
	COMMAND		*mptr;
	int			ret = 0;
	u_int		len;
	u_int		numeric;

	assert(ConIsClient(con) || ConIsUnknown(con));

#ifdef DEBUG
	printf("Parsing: %s\n", command);
#endif

	lptr = Con2Client(con);

	ptr = command;
	while(*ptr == ' ')
		ptr++;

	if (*ptr == ':')
	{
		sender = ++ptr;
		while(*ptr && (*ptr != ' '))
			ptr++;
		if (*ptr)
			*ptr++ = '\0';
		if (*sender && lptr && CliIsServer(lptr))
		{
			/* pptr = find sender */

			pptr = siclient_find_incl_hm(sender);
			if (!pptr)
			{
				si_log(SI_LOG_ERROR,
					"Received message with unknown prefix from %s:%d (%s): \":%s %s\"",
					siconn_ipport(con),
					siconn_name(con),
					sender, ptr);
				siconn_del(con, "Unknown prefix");
				return -1;
			}
		}
		else
			sender = NULL;
	}
	
	if (!*ptr)
	{
		/* drop it and disconnect if it's from a server or service */
		if (lptr && (CliIsServer(lptr) || CliIsService(lptr)))
		{
			si_log(SI_LOG_ERROR,
				"NULL command received from %s:%d (%s:%s)",
				siconn_ipport(con),
				siconn_name(con),
				sender ? sender : "<NULL>");
			siconn_del(con, "NULL command received");
			return -1;
		}
		return 0;
	}
	
	command = ptr;
	while(*ptr && (*ptr != ' '))
		ptr++;
	len = ptr - command;
	numeric = 0;
	if ((len == 3) && *ptr &&
		isdigit((int)*command) &&
		isdigit((int)*(command+1)) &&
		isdigit((int)*(command+2)))
	{   
		numeric = ((*command - '0')*100)+
                        ((*(command + 1)-'0')*10)+(*(command + 2)-'0');
		mptr = NULL;
	}
	else
	{   
		if (*ptr)
			*ptr++ = '\0';
		while(*ptr && (*ptr == ' '))
			ptr++;
		mptr = command_tree_find(command);
		if (!mptr)
		{   
			si_log(SI_LOG_ERROR,
				"Unknown command from %s:%d (%s:%s): \"%s\"",
				siconn_ipport(con),
				siconn_name(con),
				sender ? sender : "<NULL>",
				command);
			return 0;
        }
	}

	if (!pptr)
		pptr = lptr;

	if (mptr)
	{
		ret = mptr->function(con, pptr, sender, ptr);
		/* don't access 'con' if ret < 0 */
	}
	else
	{
		/* numeric */
		if (CliIsServer(pptr) && CliSentEOB(pptr) && !CliGotEOB(pptr))
        {
			printf("Got EOB from %s\n", pptr->c_name);
			SIStats.st_serversbursting--;
			CliSetGotEOB(pptr);
        }
		else
		{
			si_log(SI_LOG_NOTICE,
				"Received numeric from %s:%d (%s:%s): \"%s\"",
				siconn_ipport(con),
				siconn_name(con),
				sender ? sender : "<NULL>",
				command);
		}
	}

	return ret;
}

/*
** _conn_parse() return values:
**    0 == no error
**  < 0 == 'con' was deleted...don't access it!
**  > 0 == 'con' should be deleted...
*/
static int _conn_parse(SI_CONN *con, char *buf, register char *endptr)
{
	register char	*ptr;
	register char	*start;
	int				err;

	for(ptr=buf;;ptr++)
	{
		start = ptr;
		/* find \r\n */
		while((ptr < endptr) && *ptr != '\r' && *ptr != '\n')
			ptr++;
		if (ptr == endptr)
			break;
		if (con->con_flags & SI_CON_FLAGS_EAT) /* eating to newline? */
		{
			con->con_flags &= ~SI_CON_FLAGS_EAT;
			assert(con->con_addpos == con->con_inbuffer);
			continue;
		}
		*ptr = '\0';
		if (con->con_addpos == con->con_inbuffer)
		{
			if (start == ptr)
				continue;
			err = __conn_parse(con, start);
		}
		else
		{
			if (((con->con_addpos - con->con_inbuffer) +
					(ptr - start)) >= sizeof(con->con_inbuffer))
			{
				con->con_flags |= SI_CON_FLAGS_EAT;
				memcpy(con->con_addpos, start,
					sizeof(con->con_inbuffer) -
						(con->con_addpos - con->con_inbuffer));
				con->con_inbuffer[sizeof(con->con_inbuffer)-1] = '\0';
				err = __conn_parse(con, con->con_inbuffer);
				con->con_addpos = con->con_inbuffer;
			}
			else
			{
				if (ptr - start)
				{
					memcpy(con->con_addpos, start, ptr - start);
					con->con_addpos += ptr - start;
				}
				*con->con_addpos = '\0';
				err = __conn_parse(con, con->con_inbuffer);
				con->con_addpos = con->con_inbuffer;
			}
		}
		if (err)
			return err;
	}
	if (start == ptr)
	{
		assert(con->con_addpos == con->con_inbuffer);
		return 0;
	}
	if (!(con->con_flags & SI_CON_FLAGS_EAT))
	{
		if (((con->con_addpos - con->con_inbuffer) +
				(ptr - start)) >= sizeof(con->con_inbuffer))
		{
			*ptr = '\0';
			con->con_flags |= SI_CON_FLAGS_EAT;
			/* line is too long...parse what we have.. */
			if (con->con_addpos == con->con_inbuffer)
			{
				err = __conn_parse(con, start);
			}
			else
			{
				memcpy(con->con_addpos, start,
					sizeof(con->con_inbuffer) -
						(con->con_addpos - con->con_inbuffer));
				con->con_inbuffer[sizeof(con->con_inbuffer)-1] = '\0';
				err = __conn_parse(con, con->con_inbuffer);
				con->con_addpos = con->con_inbuffer;
			}
			if (err)
				return err;
		}
		else
		{
			assert(*start != '\0');
			memcpy(con->con_addpos, start, ptr - start);
			con->con_addpos += ptr - start;
		}
	}
	return 0;
}

int siconn_add(SI_CONNINFO *conninfo, int fd, int pollevents, u_int type, void *arg, SI_CONN **connptr)
{
	SI_CONN		*con;
	u_int		num;
	void		*vptr;

	if ((fd < 0) || !pollevents)
		return EINVAL;

	con = (SI_CONN *)simempool_calloc(&SIMainPool, 1, sizeof(SI_CONN));
	if (!con)
		return ENOMEM;
	con->con_fd = fd;
	con->con_flags = 0;
	con->con_type = type;
	con->con_arg = arg;
	con->con_conninfo = conninfo;
	con->con_firsttime = con->con_lasttime = Now;
	con->con_addpos = con->con_bufpos = con->con_inbuffer;

	if (conninfo->ci_pfds_num == conninfo->ci_pfds_size)
	{
		num = conninfo->ci_pfds_num + 10;
		vptr = (struct pollfd *)simempool_realloc(&SIMainPool, conninfo->ci_pfds,
					sizeof(struct pollfd) * num);
		if (!vptr)
		{
			simempool_free(con);
			return ENOMEM;
		}
		conninfo->ci_pfds = (struct pollfd *)vptr;
		conninfo->ci_pfds_size = num;
	}
	conninfo->ci_pfds[conninfo->ci_pfds_num].fd = fd;
	conninfo->ci_pfds[conninfo->ci_pfds_num].events = pollevents;
	conninfo->ci_pfds[conninfo->ci_pfds_num].revents = 0;
	con->con_pfd_pos = conninfo->ci_pfds_num++;

	if (fd >= conninfo->ci_conns_fd_size)
	{
		num = fd + 10;
		vptr = (SI_CONN **)simempool_realloc(&SIMainPool, conninfo->ci_conns_fd_array,
					sizeof(SI_CONN *) * num);
		if (!vptr)
		{
			conninfo->ci_pfds_num--;
			simempool_free(con);
			return ENOMEM;
		}
		conninfo->ci_conns_fd_array = (SI_CONN **)vptr;
		memset(conninfo->ci_conns_fd_array + conninfo->ci_conns_fd_size,
				0,
				sizeof(SI_CONN *) * (num - conninfo->ci_conns_fd_size));
		conninfo->ci_conns_fd_size = num;
	}
	conninfo->ci_conns_fd_array[fd] = con;

	con->con_prev = NULL;
	if ((con->con_next = conninfo->ci_conns_list) != NULL)
		con->con_next->con_prev = con;
	conninfo->ci_conns_list = con;

	if (connptr)
		*connptr = con;

	return 0;
}

int siconn_del(SI_CONN *con, char *string)
{
	SI_CLIENT	*lptr;

	if (string)
		siconn_printf(con, "ERROR :%s", string);

	if (con->con_flags & (SI_CON_FLAGS_CONNECTING|SI_CON_FLAGS_CONNECTED))
	{
		SIAutoconn_num = 0;
		if (Now - con->con_firsttime < 300)
			SIAutoconn_time = Now + 30;
	}

	siqueue_free(&(con->con_outqueue));

	close(con->con_fd);

	con->con_conninfo->ci_conns_fd_array[con->con_fd] = NULL;

	if (con->con_prev)
		con->con_prev->con_next = con->con_next;
	else
		con->con_conninfo->ci_conns_list = con->con_next;
	if (con->con_next)
		con->con_next->con_prev = con->con_prev;

	if (con->con_pfd_pos != con->con_conninfo->ci_pfds_num-1)
	{
		con->con_conninfo->ci_pfds[con->con_pfd_pos] =
			con->con_conninfo->ci_pfds[con->con_conninfo->ci_pfds_num-1];
		con->con_conninfo->ci_conns_fd_array[con->con_conninfo->ci_pfds[con->con_pfd_pos].fd]->con_pfd_pos = con->con_pfd_pos;
	}
	
	con->con_conninfo->ci_pfds_num--;

	if (ConIsClient(con) && ((lptr = Con2Client(con)) != NULL))
	{
		lptr->c_conn = NULL;
		siclient_free(lptr, string);
	}

	if (con->con_passwd)
		simempool_free(con->con_passwd);
	simempool_free(con);

	return 0;
}

int siconn_poll(SI_CONNINFO *conninfo, u_int timeout, char *read_buffer, u_int read_buffer_len)
{
	int					i;
	int					nfds;
	SI_CONN				*con;
	struct sockaddr_in	sin;
	socklen_t			sinlen;
	int					err;
	struct iovec		iov[8];

	nfds = poll(conninfo->ci_pfds, conninfo->ci_pfds_num, timeout);
	Now = time(NULL);

	if (Now >= conninfo->ci_next_check)
	{
		nfds = conninfo->ci_pfds_num;
		conninfo->ci_next_check += 30;
	}

	if (nfds <= 0)
		return nfds;

	for(i=0;nfds && i < conninfo->ci_pfds_num;i++)
	{
		assert(i < conninfo->ci_pfds_num);
		assert(conninfo->ci_pfds[i].fd < conninfo->ci_conns_fd_size);
		con = conninfo->ci_conns_fd_array[conninfo->ci_pfds[i].fd];

		if (!conninfo->ci_pfds[i].revents)
		{
			if (conninfo->ci_timeout &&
					(ConIsClient(con) || ConIsUnknown(con)))
			{
				if (con->con_flags & SI_CON_FLAGS_PINGSENT)
				{
					if (Now - con->con_lasttime >= (conninfo->ci_timeout*2))
					{
						siconn_del(con, "Ping timeout");
					}
				}
				else if (Now - con->con_lasttime >= conninfo->ci_timeout)
				{
					siconn_printf(con, "PING :%s", SIMe->c_name);
					con->con_flags |= SI_CON_FLAGS_PINGSENT;
				}
			}
			continue;
		}
		nfds--;

		con->con_lasttime = Now;
		con->con_flags &= ~SI_CON_FLAGS_PINGSENT;

		if (conninfo->ci_pfds[i].revents & ~(POLLIN|POLLOUT))
		{
			switch(con->con_type)
			{
				default:
					break;
			}
			siconn_del(con, "poll error");
			i--;
			continue;
		}

		if (conninfo->ci_pfds[i].revents & POLLIN)
		{
			switch(con->con_type)
			{
				case SI_CON_TYPE_LISTENER:
					sinlen = sizeof(sin);
					err = accept(con->con_fd, (struct sockaddr *)&sin, &sinlen);
					if (err < 0)
						continue;
					siconn_add(conninfo, err, POLLIN, SI_CON_TYPE_UNKNOWN,
										NULL, &con);
					con->con_rem_sin = sin;
					continue;
				default:
					err = read(con->con_fd, read_buffer, read_buffer_len);
					if (err <= 0)
					{
						if ((err < 0) && ((errno == EAGAIN) ||
									(errno == EWOULDBLOCK)))
							continue;
						siconn_del(con, err?strerror(errno):"EOF");
						i--;
						continue;
					}
					err = _conn_parse(con, read_buffer, read_buffer+err);
					if (err)
					{
						if (err > 0)
							siconn_del(con, "parse error");
						/* else 'con' already deleted */
						i--;
						continue;
					}
			}
		}

		if (conninfo->ci_pfds[i].revents & POLLOUT)
		{
			if (con->con_flags & SI_CON_FLAGS_CONNECTING)
			{
				con->con_flags &= ~SI_CON_FLAGS_CONNECTING;
				con->con_flags |= SI_CON_FLAGS_CONNECTED;
				conninfo->ci_pfds[i].events |= POLLIN;
			}
			if (siconn_flush(con, iov, sizeof(iov)/sizeof(iov[0])))
			{
				siconn_del(con, "flush error");
				i--;
				continue;
			}
		}

		conninfo->ci_pfds[i].revents = 0;
	}
	return 0;
}

int siconn_printfv(SI_CONN *con, char *format, va_list ap)
{
	char		packet[SIMAXPACKET];
	char		*ptr = packet;
	int			err = 0;
	int			len;

	if (con->con_flags & SI_CON_FLAGS_DEAD)
		return -1;

	len = vsnprintf(packet, sizeof(packet), format, ap);
	if (len <= 0)
	{
		con->con_flags |= SI_CON_FLAGS_DEAD;
		return -1;
	}

	packet[sizeof(packet)-1] = '\0';
	if (len >= sizeof(packet)-2)
		len = sizeof(packet)-3;

	packet[len++] = '\r';
	packet[len++] = '\n';

	if (!siqueue_buflen(&(con->con_outqueue)) &&
			!(con->con_flags & SI_CON_FLAGS_CONNECTING))
	{
		err = write(con->con_fd, ptr, len);
		if (err > 0)
		{
			ptr += err;
			len -= err;
		}
		err = 0;
	}

	if (len)
	{
		err = siqueue_add(&(con->con_outqueue), ptr, len);
		if (!err)
		{
			con->con_conninfo->ci_pfds[con->con_pfd_pos].events |= POLLOUT;
		}
	}

	return err;
}

int siconn_printf(SI_CONN *con, char *format, ...)
{
	va_list		ap;
	int			err;

	va_start(ap, format);
	err = siconn_printfv(con, format, ap);
	va_end(ap);

	return err;
}

int siconn_flush(SI_CONN *con, struct iovec *iov, u_int iov_num)
{
	int		err;

	err = siqueue_get(&(con->con_outqueue), iov, &iov_num);
	if (err)
	{
		if (err == ENOENT)
		{
			if (!siqueue_buflen(&(con->con_outqueue)))
				con->con_conninfo->ci_pfds[con->con_pfd_pos].events &= ~POLLOUT;
			return 0;
		}
		return err;
	}

	err = writev(con->con_fd, iov, iov_num);
	if (err < 0)
		return -1;
	else if (!err)
		return 0;

	err = siqueue_del(&(con->con_outqueue), err);
	if (err)
		return err;

	if (!siqueue_buflen(&(con->con_outqueue)))
		con->con_conninfo->ci_pfds[con->con_pfd_pos].events &= ~POLLOUT;

	return 0;
};


/*
**   IRC services -- queue.h
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

#ifndef __QUEUE_H__
#define __QUEUE_H__

/*
** $Id: queue.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <sys/types.h>

typedef struct _si_queue_entry	SIQUEUE_ENTRY;
typedef struct _si_queue		SIQUEUE;

struct _si_queue_entry
{
	char			qe_buffer[16*1024];
	char			*qe_addpos;
	char			*qe_bufpos;

	SIQUEUE_ENTRY	*qe_next;
};

struct _si_queue
{
	u_int			q_size;

	SIQUEUE_ENTRY	*q_tail;
	SIQUEUE_ENTRY	*q_head;
};

#define siqueue_buflen(__x)			((__x)->q_size)

int siqueue_add(SIQUEUE *q, char *buffer, u_int buflen);
int siqueue_del(SIQUEUE *q, u_int len);
int siqueue_get(SIQUEUE *q, struct iovec *iov, u_int *iov_num);
void siqueue_free(SIQUEUE *q);


#endif

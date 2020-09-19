/*
**   IRC services -- queue.c
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

RCSID("$Id: queue.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

int siqueue_add(SIQUEUE *q, char *buffer, u_int buflen)
{
	register SIQUEUE_ENTRY	*qe;
	register u_int			numleft;

	if (!q || !buffer || !buflen)
		return EINVAL;

	qe = q->q_tail;

	if (qe == NULL)
	{
		qe = (SIQUEUE_ENTRY *)simempool_alloc(&SIQueuePool, sizeof(SIQUEUE_ENTRY));
		if (!qe)
			return ENOMEM;
		qe->qe_addpos = qe->qe_buffer;
		qe->qe_bufpos = qe->qe_buffer;
		qe->qe_next = NULL;
		q->q_tail = q->q_head = qe;
	}

	for(;;)
	{
		numleft = (qe->qe_buffer + sizeof(qe->qe_buffer)) - qe->qe_addpos;

		if (buflen <= numleft)
		{
			memcpy(qe->qe_addpos, buffer, buflen);
			qe->qe_addpos += buflen;
			q->q_size += buflen;
			return 0;
		}

		memcpy(qe->qe_addpos, buffer, numleft);
		qe->qe_addpos += numleft;
		q->q_size += numleft;
		buflen -= numleft;
		qe = (SIQUEUE_ENTRY *)simempool_alloc(&SIQueuePool, sizeof(SIQUEUE_ENTRY));
		if (!qe)
			return ENOMEM;
		qe->qe_addpos = qe->qe_buffer;
		qe->qe_bufpos = qe->qe_buffer;
		qe->qe_next = NULL;
		q->q_tail->qe_next = qe;
		q->q_tail = qe;
	}
	/* NOT REACHED */
}

int siqueue_del(SIQUEUE *q, u_int len)
{
	register SIQUEUE_ENTRY	*qe;
	register SIQUEUE_ENTRY	*qenext;

	assert(len <= q->q_size);

	for(qe=q->q_head;qe;qe=qenext)
	{
		qenext = qe->qe_next;

		if (len <= qe->qe_addpos - qe->qe_bufpos)
		{
			q->q_size -= len;
			qe->qe_bufpos += len;
			if (qe->qe_addpos == qe->qe_bufpos)
			{
				if (!qe->qe_next)
					qe->qe_addpos = qe->qe_bufpos = qe->qe_buffer;
				else
				{
					simempool_free(qe);
					q->q_head = qenext;
				}
			}
			return 0;
		}

		assert(qe->qe_next != NULL);

		q->q_size -= qe->qe_addpos - qe->qe_bufpos;
		len -= qe->qe_addpos - qe->qe_bufpos;
		simempool_free(qe);
		q->q_head = qenext;
	}
	return 0;
}

int siqueue_get(SIQUEUE *q, struct iovec *iov, u_int *iov_num)
{
	register SIQUEUE_ENTRY	*qe;
	u_int					iov_size;
	u_int					num = 0;

	if (!q || !iov || !iov_num)
		return EINVAL;

	iov_size = *iov_num;

	for(qe=q->q_head;qe && (num < iov_size);qe=qe->qe_next)
	{
		iov->iov_base = qe->qe_bufpos;
		iov->iov_len = qe->qe_addpos - qe->qe_bufpos;
		if (!iov->iov_len)
			continue;
		iov++;
		num++;
	}

	*iov_num = num;

	if (!num)
		return ENOENT;

	return 0;
}

void siqueue_free(SIQUEUE *q)
{
	SIQUEUE_ENTRY	*qe;

	for(;q->q_head;q->q_head=qe)
	{
		qe = q->q_head->qe_next;
		simempool_free(q->q_head);
	}
}

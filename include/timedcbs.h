/*
**   IRC services -- timedcbs.h
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

#ifndef __TIMEDCBS_H__
#define __TIMEDCBS_H__

/*
** $Id: timedcbs.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <sys/types.h>

typedef struct _si_timedcb	SI_TIMEDCB;

struct _si_timedcb
{
	u_int		sit_id;

	time_t		sit_time;
	u_int		sit_sleep_secs;

#define SI_SIT_FLAGS_REPEAT		0x001
	u_int		sit_flags;

	void		(*sit_callback)(void *arg);
	void		*sit_arg;

	SI_TIMEDCB	*sit_prev;
	SI_TIMEDCB	*sit_next;
};


int sitimedcb_add(u_int seconds, u_int flags, void (*timedcb_callback)(void *arg), void *arg, u_int *tid);
int sitimedcb_del(u_int tid);
void sitimedcbs_check(time_t *next_check);


#endif

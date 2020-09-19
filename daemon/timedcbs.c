/*
**   IRC services -- timedcbs.c
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

RCSID("$Id: timedcbs.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

static SI_TIMEDCB	*SITimers = NULL;
static u_int		SITimer_id = 0;


int sitimedcb_add(u_int seconds, u_int flags, void (*timedcb_callback)(void *arg), void *arg, u_int *tid)
{
	SI_TIMEDCB		*t;
	SI_TIMEDCB		*tptr;
	SI_TIMEDCB		*tptrprev = NULL;

	if (!seconds || !timedcb_callback)
		return EINVAL;

	t = (SI_TIMEDCB *)simempool_calloc(&SIMainPool, 1, sizeof(SI_TIMEDCB));
	if (!t)
		return ENOMEM;
	if (!(t->sit_id = ++SITimer_id))
		t->sit_id = ++SITimer_id;	/* don't allow id of 0 */
	t->sit_flags = flags;
	t->sit_time = time(NULL);
	t->sit_sleep_secs = seconds;
	t->sit_callback = timedcb_callback;
	t->sit_arg = arg;

	if (tid)
		*tid = t->sit_id;

	if (!SITimers)
	{
		SITimers = t;
		return 0;
	}

	for(tptr=SITimers;tptr;tptrprev=tptr,tptr=tptr->sit_next)
	{
		if ((tptr->sit_time + tptr->sit_sleep_secs) >=
				(t->sit_time + t->sit_sleep_secs))
			break;
	}

	if (!tptrprev)
	{
		SITimers->sit_prev = t;
		t->sit_next = SITimers;
		SITimers = t;
		return 0;
	}

	if ((t->sit_next = tptrprev->sit_next) != NULL)
		t->sit_next->sit_prev = t;
	tptrprev->sit_next = t;
	t->sit_prev = tptrprev;

	return 0;
}

int sitimedcb_del(u_int tid)
{
	SI_TIMEDCB	*t;

	for(t=SITimers;t;t=t->sit_next)
	{
		if (t->sit_id == tid)
			break;
	}
	if (!t)
		return ENOENT;
	si_ll_del_from_list(SITimers, t, sit);
	simempool_free(t);
	return 0;
}

void sitimedcbs_check(time_t *next_check)
{
	time_t		now = time(NULL);
	time_t		nc = now + 60;
	SI_TIMEDCB	*tptr;
	SI_TIMEDCB	*nexttptr;

	for(tptr=SITimers;tptr;tptr=nexttptr)
	{
		nexttptr = tptr->sit_next;

		if (tptr->sit_time + tptr->sit_sleep_secs <= now)
		{
			tptr->sit_callback(tptr->sit_arg);
			if (!(tptr->sit_flags & SI_SIT_FLAGS_REPEAT))
			{
				si_ll_del_from_list(SITimers, tptr, sit);
				continue;
			}
			tptr->sit_time = now;
		}
		if (tptr->sit_time + tptr->sit_sleep_secs < nc)
			nc = tptr->sit_time + tptr->sit_sleep_secs;
	}
	if (next_check)
		*next_check = nc;
}



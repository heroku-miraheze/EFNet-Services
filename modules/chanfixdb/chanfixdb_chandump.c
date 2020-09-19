/*
**   IRC services chanfixdb module -- chanfixdb_chandump.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chanfixdb.h"

RCSID("$Id: chanfixdb_chandump.c,v 1.2 2004/03/30 22:23:45 cbehrens Exp $");

int main(int argc, char **argv)
{
	int					err;
	int					i;
	SI_CHANFIXDB		*cfdb;
	SI_CHANFIX_CHAN		*chan;
	SI_CHANFIX_CHANUSER	*curec;
	u_int				pos;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: chanfix_test filename channel\n");
		exit(-1);
	}

	err = chanfixdb_init(argv[1], 0, &cfdb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
		exit(err);
	}

	err = chanfixdb_channel_find(cfdb, argv[2], &chan);
	if (err)
	{
		printf("Couldn't find channel %s: %d\n", argv[2], err);
		exit(err);
	}

	printf("   Channel: %s\n", chan->cfc_name);
	if (chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING)
	{
		printf("    Status: Being %s fixed (%d pass%s) -- started at %s",
			chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL ?
			"manually" : "automatically",
			chan->cfc_fixed_passes,
			chan->cfc_fixed_passes == 1 ? "" : "es",
			ctime(&chan->cfc_fixed_time));
	}
	else
	{
		printf("    Status: Not being fixed -- last fix at %s",
			!chan->cfc_fixed_time ? "Never\n" :
				ctime(&chan->cfc_fixed_time));
	}
	printf(" Last seen: %s", ctime(&chan->cfc_last_seen));
	printf("Num Checks: %d\n", chan->cfc_checks);
	printf(" Num Users: %d\n", chan->cfc_users_num);

	if (argv[3] && !strcmp(argv[3], "verbose"))
	for(i=0;i<SI_CHANFIX_CHANUSER_HASHSIZE;i++)
	for(pos=chan->cfc_users_offset[i];pos!=-1;pos=curec->cfcu_h_next)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
		printf("%5d - %s -- %s",
				curec->cfcu_tot_score,
				curec->cfcu_userhost,
				ctime(&curec->cfcu_last_seen));
	}

#ifndef OLD_FORMAT
	if (argv[3] && !strcmp(argv[3], "sorted"))
	for(pos=chan->cfc_users_sorted_first;pos!=-1;pos=curec->cfcu_s_next)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
		printf("%5d - %s -- %s",
				curec->cfcu_tot_score,
				curec->cfcu_userhost,
				ctime(&curec->cfcu_last_seen));
	}

	if (argv[3] && !strcmp(argv[3], "rsorted"))
	for(pos=chan->cfc_users_sorted_last;pos!=-1;pos=curec->cfcu_s_prev)
	{
		curec = SI_CHANFIX_CHANUSER_PTR(cfdb, pos);
		printf("%5d - %s -- %s",
				curec->cfcu_tot_score,
				curec->cfcu_userhost,
				ctime(&curec->cfcu_last_seen));
	}

	if (argv[3] && !strcmp(argv[3], "modes"))
	{
		int		i = sizeof(chan->cfc_modes)/sizeof(chan->cfc_modes[0]);

		while(i--)
		{
			if (!chan->cfc_modes[i].cfcm_last_seen)
				continue;
			printf("Modes: +%s%s%s%s%s%s%s%s, Score: %d, Last seen: %s",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_INVITEONLY ? "i" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_MODERATED ? "m" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_NOPRIVMSGS ? "n" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_PRIVATE ? "p" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_SECRET ? "s" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_TOPICLIMIT ? "t" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_LIMIT ? "l" : "",
chan->cfc_modes[i].cfcm_modes & SI_CHANFIXDB_CHAN_MODE_KEY ? "k" : "",
				chan->cfc_modes[i].cfcm_tot_score,
				ctime(&chan->cfc_modes[i].cfcm_last_seen));
		}
	}
#endif

	chanfixdb_deinit(cfdb);

	exit(err);
}

/*
**   IRC services chanfixdb module -- chanfixdb_sort.c
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


RCSID("$Id: chanfixdb_sort.c,v 1.1 2004/04/04 20:29:33 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_CHANFIXDB		*cfdb;
	SI_CHANFIX_CHAN		*chan;
	SI_CHANFIX_CHANUSER	*chanuser;
	u_int				i;
	u_int				pos;
	u_int				upos;
	u_int				num_channels = 0;
	u_int				num_users = 0;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: chanfixdb_sort filename\n");
		exit(-1);
	}

	err = chanfixdb_init(argv[1], 0, &cfdb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
	}

	for(pos=cfdb->cf_info->cfi_channels;pos!=(u_int)-1;pos=chan->cfc_a_next)
	{
		chan = SI_CHANFIX_CHAN_PTR(cfdb, pos);

		num_channels++;

	    for(i=0;i<SI_CHANFIX_CHANUSER_HASHSIZE;i++)
	    for(upos=chan->cfc_users_offset[i];upos!=(u_int)-1;upos=chanuser->cfcu_h_next)
	    {
	        chanuser = SI_CHANFIX_CHANUSER_PTR(cfdb, upos);

			chanfixdb_chanuser_sort(cfdb, chanuser);

			num_users++;
		}
    }

	chanfixdb_deinit(cfdb);

	printf("%d user%s on %d channel%s sorted\n",
			num_users,
			num_users == 1 ? "" : "s",
			num_channels,
			num_channels == 1 ? "" : "s");

	exit(err);
}

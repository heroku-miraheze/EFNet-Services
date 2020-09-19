/*
**   IRC services admindb module -- admindb_del.c
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
#include <errno.h>
#include "admindb.h"
#include "setup.h"

RCSID("$Id: admindb_del.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;

	if ((argc < 3) || ((argc > 3) && (argc < 5)))
	{
		fprintf(stderr, "Usage: admindb_del filename account [userhost server]\n");
		exit(-1);
	}

	err = siadmindb_init(argv[1], 0, &adb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
		exit(err);
	}

	siadmindb_lock(adb);

	err = siadmindb_account_find(adb, argv[2], &arec);
	if (err)
	{
		fprintf(stderr, "Couldn't find account: %s\n",
			err == ENOENT ?
				"Account doesn't exist" : strerror(err));
		siadmindb_unlock(adb);
		siadmindb_deinit(adb);
		exit(err);
	}

	if (argc > 4)
	{
		char	*uh;
		char	*serv;
		/* remove a certain user@host */

		err = siadmindb_userhost_find(adb, arec, argv[3], argv[4], &uhrec);
		if (err)
		{
			fprintf(stderr, "Couldn't find matching u@h and server %s\n",
				err == ENOENT ?
					"Record doesn't exist" : strerror(err));
			siadmindb_unlock(adb);
			siadmindb_deinit(adb);
			return 0;
		}

		uh = strdup(uhrec->auh_userhost);
		serv = strdup(SI_ADMIN_SERVER_PTR(adb, uhrec->auh_server_offset));

		err = siadmindb_userhost_remove(adb, uhrec);
		if (err)
		{
			fprintf(stderr, "Couldn't remove the record: %s\n",
						strerror(err));
		}
		else
		{
			printf("u@h \"%s\" on server \"%s\" removed.\n",
				uh?uh:"<NULL>", serv?serv:"<NULL>");
		}

		siadmindb_unlock(adb);
		siadmindb_deinit(adb);

		return err;
	}

	/* remove account */

	err = siadmindb_account_remove(adb, arec);
	if (err)
	{
		fprintf(stderr, "Couldn't remove the account: %s\n",
					strerror(err));
	}
	else
	{
		printf("Account removed.\n");
	}

	siadmindb_unlock(adb);
	siadmindb_deinit(adb);

	exit(err);
}

/*
**   IRC services admindb module -- admindb_import
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
#include <unistd.h>
#include <errno.h>
#include "admindb.h"
#include "setup.h"

RCSID("$Id: admindb_import.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;
	FILE				*f;
	char				buffer[1024];
	char				*ptr;
	char				*thing;
	char				*server;
	char				*what;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: admindb_import dbfilename filename\n");
		exit(-1);
	}

	err = siadmindb_init(argv[1], 0, &adb);
	if (err)
	{
		fprintf(stderr, "Couldn't open dbfile: %s\n",
				strerror(err));
		exit(err);
	}

	siadmindb_lock(adb);

	f = fopen(argv[2], "r");
	if (!f)
	{
		err = errno;
		fprintf(stderr, "Couldn't open file: %s\n", strerror(err));
		exit(err);
	}

	while((ptr = fgets(buffer, sizeof(buffer), f)) != NULL)
	{
		what = strtok(buffer, " \n");
		if (!strcmp(what, "acct"))
		{
			thing = strtok(NULL, " \n");

			err = siadmindb_account_create(adb, thing, &arec);
			if (err)
			{
				fprintf(stderr, "Couldn't create account %s: %s\n",
					thing,
					strerror(err));
				siadmindb_unlock(adb);
				siadmindb_deinit(adb);
				exit(err);
			}

			thing = strtok(NULL, " \n");
			arec->aa_last_login = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_login_fail_time = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_login_fail_num = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_passwd_fail_time = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_passwd_fail_num = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_expires = atoi(thing);
			thing = strtok(NULL, " \n");
			arec->aa_flags = atoi(thing);
			thing = strtok(NULL, " \n");
			strcpy(arec->aa_passwd, thing);
		}
		else if (!strcmp(what, "uh"))
		{
			thing = strtok(NULL, " \n");
			server = strtok(NULL, " \n");

			err = siadmindb_userhost_create(adb, &arec, thing, server, &uhrec);
			if (err && (err != EEXIST))
			{
				fprintf(stderr, "Couldn't create userhost record %s %s: %s\n",
					thing, server,
					err==EEXIST?"Already exists" : strerror(err));
				siadmindb_unlock(adb);
				siadmindb_deinit(adb);
				exit(err);
			}
		}
	}

	siadmindb_unlock(adb);
	siadmindb_deinit(adb);

	exit(err);
}

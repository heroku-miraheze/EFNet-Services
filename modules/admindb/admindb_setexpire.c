/*
**   IRC services admindb module -- admindb_setexpire.c
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "admindb.h"
#include "setup.h"

RCSID("$Id: admindb_setexpire.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	char				*rest;
	int					num;
	time_t				abs;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: admindb_setexpire filename account expire\n");
		exit(-1);
	}

	rest = argv[3];
	if (*rest == '-')
		rest++;

	if (!*rest || !isdigit((int)*rest))
	{
		fprintf(stderr, "Invalid expire specified\n");
		exit(-1);
	}

	while(*rest && isdigit((int)*rest))
	{
		rest++;
		continue;
	}

	num = atoi(argv[3]);

	if (!num || (*rest == 'a'))
		abs = num;
	else
	{
		switch(*rest)
		{
			case 'w':
			case 'y':
				if (*rest == 'y')
					num *= 365;
				else
					num *= 7;
			case 'd':
				num *= 24;
			case 'h':
				num *= 60;
			case 'm':
				num *= 60;
			case 's':
				break;
			default:
				fprintf(stderr, "Expire modifier must be one of the following: s, m, h, d, w, y\n");
			
				exit(-1);
		}
		abs = time(NULL) + num;
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
			err == ENOENT ? "Account doesn't exist" : strerror(err));
		siadmindb_unlock(adb);
		siadmindb_deinit(adb);
		exit(err);
	}

	arec->aa_expires = abs;

	siadmindb_unlock(adb);
	siadmindb_deinit(adb);

	if (abs)
		printf("Account \"%s\" now expires at %s", argv[2], ctime(&abs));
	else
		printf("Account \"%s\" now never expires\n", argv[2]);

	exit(err);
}


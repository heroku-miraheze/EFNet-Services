/*
**   IRC services admindb module -- admindb_info.c
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
#include "admindb.h"

RCSID("$Id: admindb_info.c,v 1.2 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int				err;
	SI_ADMINDB		*adb;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: admindb_info filename\n");
		exit(-1);
	}

	err = siadmindb_init(argv[1], 0, &adb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
		exit(err);
	}

	printf("     Size of database: %d\n",
				adb->adb_info->ai_next_addpos);
	printf("Disk Size of database: %d\n",
				adb->adb_info->ai_size);
	printf("   Number of accounts: %d\n",
				adb->adb_info->ai_num_accounts);
	printf("     Size of freelist: %d\n",
				adb->adb_info->ai_freelist_size);

	siadmindb_deinit(adb);

	exit(err);
}

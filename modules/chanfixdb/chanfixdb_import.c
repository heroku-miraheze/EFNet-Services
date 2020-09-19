/*
**   IRC services chanfixdb module -- chanfixdb_import.c
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

RCSID("$Id: chanfixdb_import.c,v 1.2 2004/03/30 22:23:45 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_CHANFIXDB		*cfdb;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: chanfix_import filename filename checks\n");
		exit(-1);
	}

	err = chanfixdb_init(argv[1], 0, &cfdb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
		exit(err);
	}

	chanfixdb_import(cfdb, argv[2]);

	cfdb->cf_info->cfi_checks = atoi(argv[3]);

	chanfixdb_deinit(cfdb);

	exit(err);
}

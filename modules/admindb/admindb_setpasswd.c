/*
**   IRC services admindb module -- admindb_setpasswd.c
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

RCSID("$Id: admindb_setpasswd.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	struct timeval		tv;
	char				salt[9];
	int					i;
    char 				saltchars[] = "/.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char				*str;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: admindb_add filename account passwd\n");
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
			err == ENOENT ? "Account doesn't exist" : strerror(err));
		siadmindb_unlock(adb);
		siadmindb_deinit(adb);
		exit(err);
	}

	gettimeofday(&tv, NULL);

	srand(((u_int)srand / (getpid()+2)) * (tv.tv_sec/(tv.tv_usec+1)));
			
	for(i=0;i<8;i++)
		salt[i] = saltchars[(rand() & 0x3f)];
	salt[sizeof(salt)-1] = '\0';
	str = crypt(argv[3], salt);
	if (!str)
	{
		fprintf(stderr, "Couldn't crypt password\n");
		siadmindb_unlock(adb);
		siadmindb_deinit(adb);
		exit(-1);
	}
	else
	{
		strncpy(arec->aa_passwd, str, sizeof(arec->aa_passwd));
		arec->aa_passwd[sizeof(arec->aa_passwd)-1] = '\0';
	}

	printf("Password for account \"%s\" has been set\n", arec->aa_name);

	siadmindb_unlock(adb);
	siadmindb_deinit(adb);

	exit(err);
}

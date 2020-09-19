/*
**   IRC services admindb module -- admindb_lookup.c
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

RCSID("$Id: admindb_lookup.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	u_int				pos;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: admindb_lookup filename account\n");
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
			strerror(err));
		siadmindb_unlock(adb);
		siadmindb_deinit(adb);
		exit(err);
	}

	printf("	Account name: %s\n", arec->aa_name);
	printf("		 Expires: %s",
			arec->aa_expires==0?"Never\n":
			ctime(&(arec->aa_expires)));
	printf("		   Flags: %d\n", arec->aa_flags);
	printf("	  Last login: %s", ctime(&(arec->aa_last_login)));
	printf(" Login Fail Info: %u since %s",
			arec->aa_login_fail_num,
			ctime(&(arec->aa_login_fail_time)));
	printf("Passwd Fail Info: %u since %s",
			arec->aa_passwd_fail_num,
			ctime(&(arec->aa_passwd_fail_time)));
	if (argc > 2 && !strcmp(argv[2], "verbose"))
		printf("		Password: %s\n",
					arec->aa_passwd);

	for(pos=arec->aa_userhosts;pos!=-1;pos=uhrec->auh_a_next)
	{
		uhrec = SI_ADMIN_USERHOST_PTR(adb, pos);
		printf("	 %s on %s\n",
				uhrec->auh_userhost,
				SI_ADMIN_SERVER_PTR(adb, uhrec->auh_server_offset));
	}

	siadmindb_unlock(adb);
	siadmindb_deinit(adb);

	exit(err);
}

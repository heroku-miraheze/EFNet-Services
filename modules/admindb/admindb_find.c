/*
**   IRC services admindb module -- admindb_find.c
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
#include <assert.h>
#include "match.h"
#include "admindb.h"

RCSID("$Id: admindb_find.c,v 1.4 2004/03/30 22:23:44 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_ADMINDB			*adb;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;
	u_int				pos;
	u_int				pos2;
	int					which;
	int					dumpit;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: admindb_find filename [\"account\"|\"server\"|\"userhost\"] mask\n");
		exit(-1);
	}

	if (!strcmp(argv[2], "account"))
	{
		which = 0;
	}
	else if (!strcmp(argv[2], "server"))
	{
		which = 2;
	}
	else if (!strcmp(argv[2], "userhost") || !strcmp(argv[2], "uh"))
	{
		which = 2;
	}
	else
	{
		fprintf(stderr, "Usage: admindb_find filename [\"account\"|\"server\"|\"userhost\"] mask\n");
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

	for(pos=adb->adb_info->ai_accounts;pos!=-1;pos=arec->aa_a_next)
	{
		arec = SI_ADMIN_ACCOUNT_PTR(adb, pos);

		if (!which && ircd_match(arec->aa_name, argv[2]))
			continue;

		dumpit = 0;
		for(pos2=arec->aa_userhosts;pos2!=-1;pos2=uhrec->auh_a_next)
		{
			uhrec = SI_ADMIN_USERHOST_PTR(adb, pos2);

			if ((which == 1) && ircd_match(uhrec->auh_userhost, argv[2]))
				continue;
			if ((which == 2) && ircd_match(SI_ADMIN_SERVER_PTR(adb, uhrec->auh_server_offset), argv[2]))
				continue;

			dumpit = 1;
			break;
		}

		if (!dumpit)
			continue;

		printf("    Account name: %s\n", arec->aa_name);
		printf("         Expires: %s",
				arec->aa_expires==0?"Never\n":
				ctime(&(arec->aa_expires)));
		printf("           Flags: %d\n", arec->aa_flags);
		printf("      Last login: %s", ctime(&(arec->aa_last_login)));
		printf(" Login Fail Info: %u since %s",
				arec->aa_login_fail_num,
				ctime(&(arec->aa_login_fail_time)));
		printf("Passwd Fail Info: %u since %s",
				arec->aa_passwd_fail_num,
				ctime(&(arec->aa_passwd_fail_time)));
		if (argc > 2 && !strcmp(argv[2], "verbose"))
			printf("        Password: %s\n",
						arec->aa_passwd);

		for(pos2=arec->aa_userhosts;pos2!=-1;pos2=uhrec->auh_a_next)
		{
			uhrec = SI_ADMIN_USERHOST_PTR(adb, pos2);
			printf("     %s on %s\n",
					uhrec->auh_userhost,
					SI_ADMIN_SERVER_PTR(adb, uhrec->auh_server_offset));
		}
	}

	siadmindb_unlock(adb);

	siadmindb_deinit(adb);

	exit(err);
}

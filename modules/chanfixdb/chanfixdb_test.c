/*
**   IRC services chanfixdb module -- chanfixdb_test.c
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


RCSID("$Id: chanfixdb_test.c,v 1.2 2004/03/30 22:23:45 cbehrens Exp $");


int main(int argc, char **argv)
{
	int					err;
	SI_CHANFIXDB		*cfdb;
	SI_CHANFIX_CHAN		*chan;
	SI_CHANFIX_CHANUSER	*chanuser1;
	SI_CHANFIX_CHANUSER	*chanuser2;
	int					i;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: chanfixdb_test filename\n");
		exit(-1);
	}

	err = chanfixdb_init(argv[1], 0, &cfdb);
	if (err)
	{
		fprintf(stderr, "Couldn't open file: %s\n",
				strerror(err));
	}

for(i=0;i<2000;i++)
{

	printf("Test 1\n");

	err = chanfixdb_channel_find(cfdb, "#testing123", &chan);
	if (err)
		printf("Couldn't find channel #testing123: %d\n", err);
	else
		printf("Found channel #testing123\n");

	printf("Test 2\n");

	err = chanfixdb_channel_create(cfdb, "#testing123", NULL);
	if (err)
		printf("Couldn't create channel #testing123: %d\n", err);
	else
		printf("Created channel #testing123\n");

	printf("Test 3\n");

	err = chanfixdb_channel_find(cfdb, "#testing123", &chan);
	if (err)
		printf("Couldn't find channel #testing123: %d\n", err);
	else
	{
		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@concentric.net", &chanuser1);
		if (err)
			printf("Couldn't find chanuser cbehrens@concentric.net: %d\n", err);
		else
			printf("Found chanuser cbehrens@concentric.net\n");
		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@codestud.com", &chanuser2);
		if (err)
			printf("Couldn't find chanuser cbehrens@codestud.com: %d\n", err);
		else
			printf("Found chanuser cbehrens@codestud.com\n");

		printf("Test 4\n");

		err = chanfixdb_chanuser_create(cfdb, &chan, "cbehrens@concentric.net", NULL);
		if (err)
			printf("Couldn't create chanuser cbehrens@concentric.net: %d\n", err);
		else
			printf("Created chanuser cbehrens@concentric.net\n");

		err = chanfixdb_chanuser_create(cfdb, &chan, "cbehrens@codestud.com", NULL);
		if (err)
			printf("Couldn't create chanuser cbehrens@codestud.com: %d\n", err);
		else
			printf("Created chanuser cbehrens@codestud.com\n");

		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@concentric.net", &chanuser1);
		if (err)
			printf("Couldn't find chanuser cbehrens@concentric.net: %d\n", err);
		else
			printf("Found chanuser cbehrens@concentric.net\n");
		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@codestud.com", &chanuser2);
		if (err)
			printf("Couldn't find chanuser cbehrens@codestud.com: %d\n", err);
		else
			printf("Found chanuser cbehrens@codestud.com\n");

		chanfixdb_chanuser_remove(cfdb, chanuser2);
		printf("Removed chanuser cbehrens@codestud.com\n");

		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@concentric.net", &chanuser1);
		if (err)
			printf("Couldn't find chanuser cbehrens@concentric.net: %d\n", err);
		else
			printf("Found chanuser cbehrens@concentric.net\n");
		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@codestud.com", &chanuser2);
		if (err)
			printf("Couldn't find chanuser cbehrens@codestud.com: %d\n", err);
		else
			printf("Found chanuser cbehrens@codestud.com\n");

		chanfixdb_chanuser_remove(cfdb, chanuser1);
		printf("Removed chanuser cbehrens@concentric.net\n");

		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@concentric.net", &chanuser1);
		if (err)
			printf("Couldn't find chanuser cbehrens@concentric.net: %d\n", err);
		else
			printf("Found chanuser cbehrens@concentric.net\n");
		err = chanfixdb_chanuser_find(cfdb, chan, "cbehrens@codestud.com", &chanuser2);
		if (err)
			printf("Couldn't find chanuser cbehrens@codestud.com: %d\n", err);
		else
			printf("Found chanuser cbehrens@codestud.com\n");

		err = chanfixdb_channel_remove(cfdb, chan);
		if (err)
			printf("Couldn't remove chan #testing123: %d\n", err);
		else
			printf("Removed chan #testing123\n");
	}

}

	chanfixdb_deinit(cfdb);

	exit(err);
}

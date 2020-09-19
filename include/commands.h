/*
**   IRC services -- commands.h
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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

/*
** $Id: commands.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include "hash.h"

typedef int (*commandfunc_t)(SI_CONN *lptr, SI_CLIENT *sptr, char *prefix, char *args);

typedef struct _command         COMMAND;

struct _command
{
    char            *command;
    commandfunc_t   function;
};


COMMAND *command_tree_find(char *name);
void command_tree_free(void);
void command_tree_init(void);
void command_tree_deinit(void);
void send_capabilities(SI_CONN *con);
char *umode_build(SI_CLIENT *cptr);

#endif

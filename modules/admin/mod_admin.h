/*
**   IRC services admin module -- mod_admin.h
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

#ifndef __MOD_ADMIN_H__
#define __MOD_ADMIN_H__

/*
** $Id: mod_admin.h,v 1.3 2004/03/30 22:23:44 cbehrens Exp $
*/

#include "setup.h"
#include "hash.h"

int simodadmin_client_is_authed(SI_CLIENT *cptr);
int simodadmin_notice_authed_clients(SI_CLIENT *from, char *buffer);


#endif

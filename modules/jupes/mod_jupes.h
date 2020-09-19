/*
**   IRC services jupes module -- mod_jupes.h
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

#ifndef __MOD_JUPES_H__
#define __MOD_JUPES_H__

/*
** $Id: mod_jupes.h,v 1.2 2004/03/30 22:23:46 cbehrens Exp $
*/

#include "setup.h"

#define SI_MOD_JUPE_TIMEOUT			(30*60)
#define SI_MOD_JUPE_SCORE_ADMIN		5
#define SI_MOD_JUPE_SCORE_OPER		3
#define SI_MOD_JUPE_SCORE_NEEDED	(SI_MOD_JUPE_SCORE_ADMIN*SI_MOD_JUPE_SCORE_OPER)

typedef struct _si_mod_jupe		SI_MOD_JUPE;
typedef struct _si_mod_svote	SI_MOD_SVOTE;
typedef struct _si_mod_uvote	SI_MOD_UVOTE;
typedef struct _si_mod_jconfig	SI_MOD_JCONFIG;

struct _si_mod_jconfig
{
	char			*jc_dataname;

#define SI_JC_FLAGS_OPERNOTIFY		0x001
	u_int			jc_flags;
};

struct _si_mod_jupe
{
	char			*j_name;
	char			*j_reason;
	char			*j_username;
	char			*j_hostname;

	time_t			j_creation;		/* 0 == from .conf file */
	time_t			j_last_heard;
	time_t			j_expires;

#define SI_MOD_JUPE_FLAGS_ACTIVE		0x001
#define SI_MOD_JUPE_FLAGS_CONFFILE		0x002
	u_int			j_flags;

	u_int			j_score;

	SI_MOD_CLIENT	j_client;

	SI_MOD_SVOTE	*j_svotes;

	SI_MOD_JUPE		*j_prev;
	SI_MOD_JUPE		*j_next;
};

struct _si_mod_svote
{
	char			*sv_server;
	SI_MOD_UVOTE	*sv_uvotes;

	SI_MOD_SVOTE	*sv_prev;
	SI_MOD_SVOTE	*sv_next;
};

struct _si_mod_uvote
{
	char			*uv_nick;
	char			*uv_user;
	char			*uv_host;
#define SI_MOD_UVOTE_ADMIN		0x001
	u_int			uv_flags;
	u_int			uv_score;

	SI_MOD_UVOTE	*uv_prev;
	SI_MOD_UVOTE	*uv_next;
};

#endif

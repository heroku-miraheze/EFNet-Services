/*
**   IRC services -- misc.h
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

#ifndef __MISC_H__
#define __MISC_H__

/*
** $Id: misc.h,v 1.2 2004/03/30 22:23:43 cbehrens Exp $
*/

#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

char *si_strncpy(char *dest, char *src, int len);
char *si_stristr(char *str1, char *str2);
char *si_timestr(time_t thetime, char *buffer, u_int buflen);
char *si_strerror(int errnum);

#define SI_LOG_NOTICE	0
#define SI_LOG_WARNING	1
#define SI_LOG_ERROR	2
#define SI_LOG_DEBUG	3
void si_log(int level, char *format, ...);
void si_logv(int level, char *format, va_list ap);
#define SI_NEXTARG_FLAGS_LASTARG		0x001
char *si_nextarg(char **src, int flags);
char *si_next_subarg_space(char **src);
char *si_next_subarg_comma(char **src);

#endif

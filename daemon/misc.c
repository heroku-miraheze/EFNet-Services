/*
**   IRC services -- misc.c
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


#include "services.h"

RCSID("$Id: misc.c,v 1.2 2004/03/30 22:23:43 cbehrens Exp $");

char *si_strncpy(char *dest, char *src, int len)
{
	register char *start = dest;
	register char *ending = dest + len - 1;

	while(*src && (dest < ending))
		*dest++ = *src++;
	if (len > 0)
		*dest = '\0';
	return start;
}

char *si_stristr(char *str1, char *str2)
{
    register char	*s1 = str1;
	register char	*s2 = str2;
    register char	*tptr;
    register char	c;

	if (s2 == NULL || *s2 == '\0')
        return str1;

	c = *s2;

	while (*s1)
	{
		if (toupper(*s1++) == toupper(c))
		{
			tptr = s1;
            while ((toupper(c = *++s2) == toupper(*s1++)) && c)
				continue;
			if (!c)
   				return(tptr-1);
			s1 = tptr;
   			s2 = str2;
			c = *s2;
		}
	}
	return NULL;
}

char *si_timestr(time_t thetime, char *buffer, u_int buflen)
{
	struct tm *tmptr;

	if (buflen > 0)
	{
		tmptr = localtime(&thetime);
		strftime(buffer, buflen - 1, "%a %b %e %T %Y %Z", tmptr);
		buffer[buflen-1] = '\0';
	}
	return buffer;
}

char *si_strerror(int errnum)
{
	return (errno <= 0) ? "Undefined error" : strerror(errnum);
}

void si_log(int level, char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	si_logv(level, format, ap);
    va_end(ap);
}

void si_logv(int level, char *format, va_list ap)
{
	switch(level)
	{
		case SI_LOG_DEBUG:
			vsyslog(LOG_DEBUG, format, ap);
			break;
		case SI_LOG_NOTICE:
			vsyslog(LOG_NOTICE, format, ap);
			break;
        case SI_LOG_WARNING:
   			vsyslog(LOG_WARNING, format, ap);
			break;
		case SI_LOG_ERROR:
		default:
			vsyslog(LOG_ERR, format, ap);
			break;
    };
}

char *si_nextarg(char **src, int flags)
{
	char	*tok;

	if (!src || !*src || !**src)
		return NULL;
	while(**src && **src == ' ')
		(*src)++;
	if (**src)
		tok = *src;
	else
		return NULL;
	if (*tok == ':')
	{
		tok++;
		*src = NULL;
	}
	else if (!(flags & SI_NEXTARG_FLAGS_LASTARG))
	{
		while(**src && **src != ' ')
			(*src)++;
		if (**src)
		{
			**src = '\0';
			(*src)++;
			while(**src && **src == ' ')
				(*src)++;
		}
	}
	else
		*src = NULL;
	return tok;
}

char *si_next_subarg_space(char **src)
{
	char	*sptr;
	char	*tok;

	if (!src || !(sptr = *src))
		return NULL;
	while(*sptr == ' ')
		sptr++;
	if (!*sptr)
		return NULL;
	tok = sptr;
	while(*sptr && *sptr != ' ')
		sptr++;
	if (*sptr)
	{
		*sptr++ = '\0';
		while(*sptr == ' ')
			sptr++;
		*src = sptr;
	}
	else
		*src = NULL;
	return tok;
}

char *si_next_subarg_comma(char **src)
{
	char	*sptr;
	char	*tok;
				 
	if (!src || !(sptr = *src))
		return NULL;		   
	while(*sptr == ',')		
		sptr++;		
	if (!*sptr)		
		return NULL;
	tok = sptr;	 
	while(*sptr && *sptr != ',')
		sptr++;				 
	if (*sptr)				  
	{		 
		*sptr++ = '\0';
		while(*sptr == ',')
			sptr++;		
		*src = sptr;
	}			   
	else
		*src = NULL;
	return tok;	 
}


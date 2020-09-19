/*
**   IRC services crc32 module -- crc32.h
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

#ifndef __CRC32_H__
#define __CRC32_H__

/*
** $Id: crc32.h,v 1.2 2004/03/30 22:23:45 cbehrens Exp $
*/

#include <sys/types.h>

typedef unsigned int crc32_t;

crc32_t crc32_compute(const char *string, size_t stringlen);

#endif

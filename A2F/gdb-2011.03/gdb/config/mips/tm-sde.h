/* Target-dependent definitions for MIPS SDE.
   Copyright 2004 Free Software Foundation, Inc.
   Contributed by MIPS Technologies, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef TM_SDE_H
#define TM_SDE_H

#include "mips/tm-mips.h"

/* The original O32 ABI explicitly states that the naked char type is
   unsigned, and the other ABI specs don't contradict it. We stick to
   this for compatibility with previous releases of SDE, and for
   better performance. But this is different from other MIPS
   configurations. */

#define MIPS_CHAR_SIGNED 		0

#endif /* TM_SDE_H */

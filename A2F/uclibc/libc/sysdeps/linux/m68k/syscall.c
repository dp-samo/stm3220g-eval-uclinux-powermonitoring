/* syscall for m68k/uClibc
 *
 * Copyright (C) 2005-2006 by Christian Magnusson <mag@mag.cx>
 * Copyright (C) 2005-2006 Erik Andersen <andersen@uclibc.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Library General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <features.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>

long syscall(long sysnum, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6)
{
  unsigned int _sys_result;

  _sys_result = INTERNAL_SYSCALL_NCS(sysnum, , 6, arg1, arg2, arg3, arg4, arg5, arg6);

  if (__builtin_expect (INTERNAL_SYSCALL_ERROR_P (_sys_result, ), 0))
    {
      __set_errno (INTERNAL_SYSCALL_ERRNO (_sys_result, ));
      _sys_result = (unsigned int) -1;
    }

  return (int) _sys_result;
}

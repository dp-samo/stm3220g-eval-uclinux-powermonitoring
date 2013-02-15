/* GNU/Linux/SH specific low level interface, for the remote server for GDB.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2007,
   2008, 2009, 2010 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "server.h"
#include "linux-low.h"

/* Defined in auto-generated file reg-sh.c.  */
void init_registers_sh (void);

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#include <asm/ptrace.h>

#define sh_num_regs 41
#define sh_num_gregs 23

/* Some ptrace commands from asm/ptrace.h may have been undefined by
   sys/user.h.  */

#ifdef HAVE_PTRACE_GETREGS
#ifndef PTRACE_GETREGS
#define PTRACE_GETREGS   12
#define PTRACE_SETREGS   13
#define PTRACE_GETFPREGS 14
#define PTRACE_SETFPREGS 15
#endif
#endif /* HAVE_PTRACE_GETREGS */

/* Currently, don't check/send MQ.  */
static int sh_regmap[] = {
 0,	4,	8,	12,	16,	20,	24,	28,
 32,	36,	40,	44,	48,	52,	56,	60,

 REG_PC*4,   REG_PR*4,   REG_GBR*4,  -1,
 REG_MACH*4, REG_MACL*4, REG_SR*4,
 REG_FPUL*4, REG_FPSCR*4,

 REG_FPREG0*4+0,   REG_FPREG0*4+4,   REG_FPREG0*4+8,   REG_FPREG0*4+12,
 REG_FPREG0*4+16,  REG_FPREG0*4+20,  REG_FPREG0*4+24,  REG_FPREG0*4+28,
 REG_FPREG0*4+32,  REG_FPREG0*4+36,  REG_FPREG0*4+40,  REG_FPREG0*4+44,
 REG_FPREG0*4+48,  REG_FPREG0*4+52,  REG_FPREG0*4+56,  REG_FPREG0*4+60,
};

static int
sh_cannot_store_register (int regno)
{
  return 0;
}

static int
sh_cannot_fetch_register (int regno)
{
  return 0;
}

static CORE_ADDR
sh_get_pc (struct regcache *regcache)
{
  unsigned long pc;
  collect_register_by_name (regcache, "pc", &pc);
  return pc;
}

static void
sh_set_pc (struct regcache *regcache, CORE_ADDR pc)
{
  unsigned long newpc = pc;
  supply_register_by_name (regcache, "pc", &newpc);
}

/* Correct in either endianness, obviously.  */
static const unsigned short sh_breakpoint = 0xc3c3;
#define sh_breakpoint_len 2

static int
sh_breakpoint_at (CORE_ADDR where)
{
  unsigned short insn;

  (*the_target->read_memory) (where, (unsigned char *) &insn, 2);
  if (insn == sh_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

/* Provide only a fill function for the general register set if ptrace
   does not support getreg/setreg operations.  ps_lgetregs will use this
   for NPTL support.  */

static void
sh_fill_gregset (struct regcache *regcache, void *buf)
{
  int i;

  for (i = 0; i < sh_num_gregs; i++)
    if (sh_regmap[i] != -1)
      collect_register (regcache, i, (char *) buf + sh_regmap[i]);
}

#ifdef HAVE_PTRACE_GETREGS

static void
sh_store_gregset (struct regcache *regcache, const void *buf)
{
  int i;

  for (i = 0; i < sh_num_gregs; i++)
    if (sh_regmap[i] != -1)
      supply_register (regcache, i, (const char *) buf + sh_regmap[i]);
}

static void
sh_fill_fpregset (struct regcache *regcache, void *buf)
{
  int i;
  int buf_offset = sh_regmap[find_regno ("fr0")];

  for (i = sh_num_gregs; i < sh_num_regs; i++)
    if (sh_regmap[i] != -1)
      collect_register (regcache, i, (char *) buf
			+ sh_regmap[i] - buf_offset);
}

static void
sh_store_fpregset (struct regcache *regcache, const void *buf)
{
  int i;
  int buf_offset = sh_regmap[find_regno ("fr0")];

  for (i = sh_num_gregs; i < sh_num_regs; i++)
    if (sh_regmap[i] != -1)
      supply_register (regcache, i, (const char *) buf
		       + sh_regmap[i] - buf_offset);
}

#endif /* HAVE_PTRACE_GETREGS */

struct regset_info target_regsets[] = {
#ifdef HAVE_PTRACE_GETREGS
  { PTRACE_GETREGS, PTRACE_SETREGS, 0, sizeof (elf_gregset_t),
    GENERAL_REGS,
    sh_fill_gregset, sh_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, 0, sizeof (elf_fpregset_t),
    FP_REGS,
    sh_fill_fpregset, sh_store_fpregset },
#else
  { 0, 0, 0, 0, GENERAL_REGS, sh_fill_gregset, NULL },
#endif /* HAVE_PTRACE_GETREGS */
  { 0, 0, 0, -1, -1, NULL, NULL }
};

struct linux_target_ops the_low_target = {
  init_registers_sh,
  sh_num_regs,
  sh_regmap,
  sh_cannot_fetch_register,
  sh_cannot_store_register,
  sh_get_pc,
  sh_set_pc,
  (const unsigned char *) &sh_breakpoint,
  sh_breakpoint_len,
  NULL,
  0,
  sh_breakpoint_at,
};

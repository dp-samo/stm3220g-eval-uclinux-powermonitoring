/* GNU/Linux/MIPS specific low level interface, for the remote server for GDB.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2005, 2006, 2007,
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

#include <sys/ptrace.h>
#include <endian.h>

#include "gdb_proc_service.h"

/* Defined in auto-generated file mips-linux.c.  */
void init_registers_mips_linux (void);
/* Defined in auto-generated file mips64-linux.c.  */
void init_registers_mips64_linux (void);

#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA 25
#endif

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#define mips_num_regs 73

#include <asm/ptrace.h>

union mips_register
{
  unsigned char buf[8];

  /* Deliberately signed, for proper sign extension.  */
  int reg32;
  long long reg64;
};

/* Return the ptrace ``address'' of register REGNO. */

static int mips_regmap[] = {
  -1,  1,  2,  3,  4,  5,  6,  7,
  8,  9,  10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,

  -1, MMLO, MMHI, BADVADDR, CAUSE, PC,

  FPR_BASE,      FPR_BASE + 1,  FPR_BASE + 2,  FPR_BASE + 3,
  FPR_BASE + 4,  FPR_BASE + 5,  FPR_BASE + 6,  FPR_BASE + 7,
  FPR_BASE + 8,  FPR_BASE + 8,  FPR_BASE + 10, FPR_BASE + 11,
  FPR_BASE + 12, FPR_BASE + 13, FPR_BASE + 14, FPR_BASE + 15,
  FPR_BASE + 16, FPR_BASE + 17, FPR_BASE + 18, FPR_BASE + 19,
  FPR_BASE + 20, FPR_BASE + 21, FPR_BASE + 22, FPR_BASE + 23,
  FPR_BASE + 24, FPR_BASE + 25, FPR_BASE + 26, FPR_BASE + 27,
  FPR_BASE + 28, FPR_BASE + 29, FPR_BASE + 30, FPR_BASE + 31,
  FPC_CSR, FPC_EIR,

  0
};

/* We keep list of all watchpoints we should install and calculate the
   watch register values each time the list changes.  This allows for
   easy sharing of watch registers for more than one watchpoint.  */

struct mips_watchpoint
{
  CORE_ADDR addr;
  int len;
  int type;
  struct mips_watchpoint *next;
};

/* Per-process arch-specific data we want to keep.  */

struct arch_process_info
{
  /* -1 if the kernel and/or CPU do not support watch registers.
      1 if watch_readback is valid and we can read style, num_valid
	and the masks.
      0 if we need to read the watch_readback.  */

  int watch_readback_valid;

  /* Cached watch register read values.  */

  struct pt_watch_regs watch_readback;

  /* Current watchpoint requests for this process.  */

  struct mips_watchpoint *current_watches;

  /*  The current set of watch register values for writing the
      registers.  */

  struct pt_watch_regs watch_mirror;
};

/* Per-thread arch-specific data we want to keep.  */

struct arch_lwp_info
{
  /* Non-zero if our copy differs from what's recorded in the thread.  */
  int debug_registers_changed;
};

/* From mips-linux-nat.c.  */

/* Pseudo registers can not be read.  ptrace does not provide a way to
   read (or set) PS_REGNUM, and there's no point in reading or setting
   ZERO_REGNUM.  We also can not set BADVADDR, CAUSE, or FCRIR via
   ptrace().  */

static int
mips_cannot_fetch_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("r0") == regno)
    return 1;

  return 0;
}

static int
mips_cannot_store_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("r0") == regno)
    return 1;

  if (find_regno ("cause") == regno)
    return 1;

  if (find_regno ("badvaddr") == regno)
    return 1;

  if (find_regno ("fir") == regno)
    return 1;

  return 0;
}

static CORE_ADDR
mips_get_pc (struct regcache *regcache)
{
  union mips_register pc;
  collect_register_by_name (regcache, "pc", pc.buf);
  return register_size (0) == 4 ? pc.reg32 : pc.reg64;
}

static void
mips_set_pc (struct regcache *regcache, CORE_ADDR pc)
{
  union mips_register newpc;
  if (register_size (0) == 4)
    newpc.reg32 = pc;
  else
    newpc.reg64 = pc;

  supply_register_by_name (regcache, "pc", newpc.buf);
}

/* Correct in either endianness.  */
static const unsigned int mips_breakpoint = 0x0005000d;
#define mips_breakpoint_len 4

/* We only place breakpoints in empty marker functions, and thread locking
   is outside of the function.  So rather than importing software single-step,
   we can just run until exit.  */
static CORE_ADDR
mips_reinsert_addr (void)
{
  struct regcache *regcache = get_thread_regcache (current_inferior, 1);
  union mips_register ra;
  collect_register_by_name (regcache, "r31", ra.buf);
  return register_size (0) == 4 ? ra.reg32 : ra.reg64;
}

static int
mips_breakpoint_at (CORE_ADDR where)
{
  unsigned int insn;

  (*the_target->read_memory) (where, (unsigned char *) &insn, 4);
  if (insn == mips_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

#define W_BIT 0
#define R_BIT 1
#define I_BIT 2

#define W_MASK (1 << W_BIT)
#define R_MASK (1 << R_BIT)
#define I_MASK (1 << I_BIT)

#define IRW_MASK (I_MASK | R_MASK | W_MASK)

#define MAX_DEBUG_REGISTER 8

/* Assuming usable watch registers, return the irw_mask.  */

static uint32_t
get_irw_mask (struct pt_watch_regs *regs, int set)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      return regs->mips32.watch_masks[set] & IRW_MASK;
    case pt_watch_style_mips64:
      return regs->mips64.watch_masks[set] & IRW_MASK;
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, return the reg_mask.  */

static uint32_t
get_reg_mask (struct pt_watch_regs *regs, int set)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      return regs->mips32.watch_masks[set] & ~IRW_MASK;
    case pt_watch_style_mips64:
      return regs->mips64.watch_masks[set] & ~IRW_MASK;
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, return the num_valid.  */

static uint32_t
get_num_valid (struct pt_watch_regs *regs)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      return regs->mips32.num_valid;
    case pt_watch_style_mips64:
      return regs->mips64.num_valid;
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, return the watchlo.  */

static CORE_ADDR
get_watchlo (struct pt_watch_regs *regs, int set)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      return regs->mips32.watchlo[set];
    case pt_watch_style_mips64:
      return regs->mips64.watchlo[set];
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, set a watchlo value.  */

static void
set_watchlo (struct pt_watch_regs *regs, int set, CORE_ADDR value)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      /*  The cast will never throw away bits as 64 bit addresses can
	  never be used on a 32 bit kernel.  */
      regs->mips32.watchlo[set] = (uint32_t)value;
      break;
    case pt_watch_style_mips64:
      regs->mips64.watchlo[set] = value;
      break;
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, return the watchhi.  */

static uint32_t
get_watchhi (struct pt_watch_regs *regs, int n)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      return regs->mips32.watchhi[n];
    case pt_watch_style_mips64:
      return regs->mips64.watchhi[n];
    default:
      error ("Unrecognized watch register style");
    }
}

/* Assuming usable watch registers, set a watchhi value.  */

static void
set_watchhi (struct pt_watch_regs *regs, int n, uint16_t value)
{
  switch (regs->style)
    {
    case pt_watch_style_mips32:
      regs->mips32.watchhi[n] = value;
      break;
    case pt_watch_style_mips64:
      regs->mips64.watchhi[n] = value;
      break;
    default:
      error ("Unrecognized watch register style");
    }
}

/* Return 1 if watch registers are usable.  Cached information is used
   unless force is true.  */

static int
mips_linux_read_watch_registers (struct process_info *proc, int force)
{
  struct arch_process_info *private = proc->private->arch_private;
  int tid;

  if (force || private->watch_readback_valid == 0)
    {
      tid = lwpid_of (proc);
      if (ptrace (PTRACE_GET_WATCH_REGS, tid, &private->watch_readback) == -1)
	{
	  private->watch_readback_valid = -1;
	  return 0;
	}
      switch (private->watch_readback.style)
	{
	case pt_watch_style_mips32:
	  if (private->watch_readback.mips32.num_valid == 0)
	    {
	      private->watch_readback_valid = -1;
	      return 0;
	    }
	  break;
	case pt_watch_style_mips64:
	  if (private->watch_readback.mips64.num_valid == 0)
	    {
	      private->watch_readback_valid = -1;
	      return 0;
	    }
	  break;
	default:
	  private->watch_readback_valid = -1;
	  return 0;
	}
      /* Watch registers appear to be usable.  */
      private->watch_readback_valid = 1;
    }
  return (private->watch_readback_valid == 1) ? 1 : 0;
}

/* Convert GDB's type to an IRW mask.  */

static unsigned
type_to_irw (int type)
{
  switch (type)
    {
    case '2':
      return W_MASK;
    case '3':
      return R_MASK;
    case '4':
      return (W_MASK | R_MASK);
    default:
      return 0;
    }
}

/* Set any low order bits in mask that are not set.  */

static CORE_ADDR
fill_mask (CORE_ADDR mask)
{
  CORE_ADDR f = 1;
  while (f && f < mask)
    {
      mask |= f;
      f <<= 1;
    }
  return mask;
}

/* Try to add a single watch to the specified registers.  Return 1 on
   success, 0 on failure.  */

static int
try_one_watch (struct pt_watch_regs *regs, CORE_ADDR addr,
	       int len, unsigned irw)
{
  CORE_ADDR base_addr, last_byte, break_addr, segment_len;
  CORE_ADDR mask_bits, t_low;
  uint16_t t_hi;
  int i, free_watches;
  struct pt_watch_regs regs_copy;

  if (len <= 0)
    return 0;

  last_byte = addr + len - 1;
  mask_bits = fill_mask (addr ^ last_byte) | IRW_MASK;
  base_addr = addr & ~mask_bits;

  /* Check to see if it is covered by current registers.  */
  for (i = 0; i < get_num_valid (regs); i++)
    {
      t_low = get_watchlo (regs, i);
      if (t_low != 0 && irw == ((unsigned)t_low & irw))
	{
	  t_hi = get_watchhi (regs, i) | IRW_MASK;
	  t_low &= ~(CORE_ADDR)t_hi;
	  if (addr >= t_low && last_byte <= (t_low + t_hi))
	    return 1;
	}
    }
  /* Try to find an empty register.  */
  free_watches = 0;
  for (i = 0; i < get_num_valid (regs); i++)
    {
      t_low = get_watchlo (regs, i);
      if (t_low == 0 && irw == (get_irw_mask (regs, i) & irw))
	{
	  if (mask_bits <= (get_reg_mask (regs, i) | IRW_MASK))
	    {
	      /* It fits, we'll take it.  */
	      set_watchlo (regs, i, base_addr | irw);
	      set_watchhi (regs, i, mask_bits & ~IRW_MASK);
	      return 1;
	    }
	  else
	    {
	      /* It doesn't fit, but has the proper IRW capabilities.  */
	      free_watches++;
	    }
	}
    }
  if (free_watches > 1)
    {
      /* Try to split it across several registers.  */
      regs_copy = *regs;
      for (i = 0; i < get_num_valid (&regs_copy); i++)
	{
	  t_low = get_watchlo (&regs_copy, i);
	  t_hi = get_reg_mask (&regs_copy, i) | IRW_MASK;
	  if (t_low == 0 && irw == (t_hi & irw))
	    {
	      t_low = addr & ~(CORE_ADDR)t_hi;
	      break_addr = t_low + t_hi + 1;
	      if (break_addr >= addr + len)
		segment_len = len;
	      else
		segment_len = break_addr - addr;
	      mask_bits = fill_mask (addr ^ (addr + segment_len - 1));
	      set_watchlo (&regs_copy, i, (addr & ~mask_bits) | irw);
	      set_watchhi (&regs_copy, i, mask_bits & ~IRW_MASK);
	      if (break_addr >= addr + len)
		{
		  *regs = regs_copy;
		  return 1;
		}
	      len = addr + len - break_addr;
	      addr = break_addr;
	    }
	}
    }
  /* It didn't fit anywhere, we failed.  */
  return 0;
}

/* Target to_region_ok_for_hw_watchpoint implementation.  Return 1 if
   the specified region can be covered by the watch registers.  */

static int
update_debug_registers_callback (struct inferior_list_entry *entry,
				 void *pid_p)
{
  struct lwp_info *lwp = (struct lwp_info *) entry;
  int pid = *(int *) pid_p;

  /* Only update the threads of this process.  */
  if (pid_of (lwp) == pid)
    {
      /* The actual update is done later just before resuming the lwp,
	 we just mark that the registers need updating.  */
      lwp->arch_private->debug_registers_changed = 1;

      /* If the lwp isn't stopped, force it to momentarily pause, so
	 we can update its debug registers.  */
      if (!lwp->stopped)
	linux_stop_lwp (lwp);
    }

  return 0;
}

/* Called when a new process is created.  */

static struct arch_process_info *
mips_linux_new_process (void)
{
  struct arch_process_info *info = xcalloc (1, sizeof (*info));

  return info;
}

/* Write the mirrored watch register values for the new thread.  */

static struct arch_lwp_info *
mips_linux_new_thread (void)
{
  struct arch_lwp_info *info = xcalloc (1, sizeof (*info));

  info->debug_registers_changed = 1;

  return info;
}

/* Called when resuming a thread.
   If the debug regs have changed, update the thread's copies.  */

static void
mips_linux_prepare_to_resume (struct lwp_info *lwp)
{
  ptid_t ptid = ptid_of (lwp);
  struct process_info *proc = find_process_pid (ptid_get_pid (ptid));
  struct arch_process_info *private = proc->private->arch_private;

  if (lwp->arch_private->debug_registers_changed)
    {
      /* Only update the debug registers if we have set or unset a
	 watchpoint already.  */
      if (get_num_valid (&private->watch_mirror) > 0)
	{
	  /* Write the mirrored watch register values.  */
	  int tid = ptid_get_lwp (ptid);

	  if (ptrace (PTRACE_SET_WATCH_REGS, tid, &private->watch_mirror) == -1)
	    perror_with_name ("Couldn't write debug register");
	}

      lwp->arch_private->debug_registers_changed = 0;
    }
}

/* Fill in the watch registers with the currently cached watches.  */

static void
populate_regs_from_watches (struct mips_watchpoint *current_watches,
			    struct pt_watch_regs *regs)
{
  struct mips_watchpoint *w;
  int i;

  /* Clear them out.  */
  for (i = 0; i < get_num_valid (regs); i++)
    {
      set_watchlo (regs, i, 0);
      set_watchhi (regs, i, 0);
    }

  w = current_watches;
  while (w)
    {
      i = try_one_watch (regs, w->addr, w->len, type_to_irw (w->type));
      /* They must all fit, because we previously calculated that they
	 would.  */
      gdb_assert (i);
      w = w->next;
    }
}

static int
mips_insert_point (char type, CORE_ADDR addr, int len)
{
  struct process_info *proc = current_process ();
  struct arch_process_info *private = proc->private->arch_private;
  struct pt_watch_regs regs;
  struct mips_watchpoint *new_watch;
  struct mips_watchpoint **pw;
  int pid;

  /* Breakpoint/watchpoint types:
       '0' - software-breakpoint (not supported)
       '1' - hardware-breakpoint (not supported)
       '2' - write watchpoint (supported)
       '3' - read watchpoint (supported)
       '4' - access watchpoint (supported).  */

  if (type < '2' || type > '4')
    {
      /* Unsupported.  */
      return 1;
    }

  if (!mips_linux_read_watch_registers (proc, 0))
    return -1;

  if (len <= 0)
    return -1;

  regs = private->watch_readback;
  /* Add the current watches.  */
  populate_regs_from_watches (private->current_watches, &regs);

  /* Now try to add the new watch.  */
  if (!try_one_watch (&regs, addr, len, type_to_irw (type)))
    return -1;

  /* It fit.  Stick it on the end of the list.  */
  new_watch = (struct mips_watchpoint *)
    xmalloc (sizeof (struct mips_watchpoint));
  new_watch->addr = addr;
  new_watch->len = len;
  new_watch->type = type;
  new_watch->next = NULL;

  pw = &private->current_watches;
  while (*pw != NULL)
    pw = &(*pw)->next;
  *pw = new_watch;

  private->watch_mirror = regs;

  /* Only update the threads of this process.  */
  pid = pid_of (proc);
  find_inferior (&all_lwps, update_debug_registers_callback, &pid);

  return 0;
}

static int
mips_remove_point (char type, CORE_ADDR addr, int len)
{
  struct process_info *proc = current_process ();
  struct arch_process_info *private = proc->private->arch_private;

  int deleted_one;
  int pid;

  struct mips_watchpoint **pw;
  struct mips_watchpoint *w;

  /* Breakpoint/watchpoint types:
       '0' - software-breakpoint (not supported)
       '1' - hardware-breakpoint (not supported)
       '2' - write watchpoint (supported)
       '3' - read watchpoint (supported)
       '4' - access watchpoint (supported).  */

  if (type < '2' || type > '4')
    {
      /* Unsupported.  */
      return 1;
    }

  /* Search for a known watch that matches.  Then unlink and free
     it.  */
  deleted_one = 0;
  pw = &private->current_watches;
  while ((w = *pw))
    {
      if (w->addr == addr && w->len == len && w->type == type)
	{
	  *pw = w->next;
	  free (w);
	  deleted_one = 1;
	  break;
	}
      pw = &(w->next);
    }

  if (!deleted_one)
    return -1;  /* We don't know about it, fail doing nothing.  */

  /* At this point watch_readback is known to be valid because we
     could not have added the watch without reading it.  */
  gdb_assert (private->watch_readback_valid == 1);

  private->watch_mirror = private->watch_readback;
  populate_regs_from_watches (private->current_watches,
			      &private->watch_mirror);

  /* Only update the threads of this process.  */
  pid = pid_of (proc);
  find_inferior (&all_lwps, update_debug_registers_callback, &pid);
  return 0;
}

/* Return 1 if stopped by watchpoint.  The watchhi R and W bits indicate
   the watch register triggered. */

static int
mips_stopped_by_watchpoint (void)
{
  struct process_info *proc = current_process ();
  struct arch_process_info *private = proc->private->arch_private;
  int n;
  int num_valid;

  if (!mips_linux_read_watch_registers (proc, 1))
    return 0;

  num_valid = get_num_valid (&private->watch_readback);

  for (n = 0; n < MAX_DEBUG_REGISTER && n < num_valid; n++)
    if (get_watchhi (&private->watch_readback, n) & (R_MASK | W_MASK))
      return 1;

  return 0;
}

/* Set the address where the watch triggered (if known).
   Return 1 if the address was known.  */

static CORE_ADDR
mips_stopped_data_address (void)
{
  struct process_info *proc = current_process ();
  struct arch_process_info *private = proc->private->arch_private;
  int n;
  int num_valid;

  /* On mips we don't know the low order 3 bits of the data address.
     GDB does not support remote targets that can't report the
     watchpoint address.  So, make our best guess; return the starting
     address of a watchpoint request which overlaps the one that
     triggered.  */
  
  if (!mips_linux_read_watch_registers (proc, 0))
    return 0;

  num_valid = get_num_valid (&private->watch_readback);

  for (n = 0; n < MAX_DEBUG_REGISTER && n < num_valid; n++)
    if (get_watchhi (&private->watch_readback, n) & (R_MASK | W_MASK))
      {
	CORE_ADDR t_low, t_hi;
	int t_irw;
	struct mips_watchpoint *watch;

	t_low = get_watchlo (&private->watch_readback, n);
	t_irw = t_low & IRW_MASK;
	t_hi = get_watchhi (&private->watch_readback, n) | IRW_MASK;
	t_low &= ~(CORE_ADDR)t_hi;

	for (watch = private->current_watches; watch != NULL; watch = watch->next)
	  {
	    CORE_ADDR addr = watch->addr;
	    CORE_ADDR last_byte = addr + watch->len - 1;
	    if ((t_irw & type_to_irw (watch->type)) == 0)
	      /* Different type.  */
	      continue;
	    /* Check for overlap of even a single byte.  */
	    if (last_byte >= t_low && addr <= t_low + t_hi)
	      return addr;
	  }
      }

  /* Shouldn't happen.  */
  return 0;
}

/* Fetch the thread-local storage pointer for libthread_db.  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph,
		    lwpid_t lwpid, int idx, void **base)
{
  if (ptrace (PTRACE_GET_THREAD_AREA, lwpid, NULL, base) != 0)
    return PS_ERR;

  /* IDX is the bias from the thread pointer to the beginning of the
     thread descriptor.  It has to be subtracted due to implementation
     quirks in libthread_db.  */
  *base = (void *) ((char *)*base - idx);

  return PS_OK;
}

#ifdef HAVE_PTRACE_GETREGS

static void
mips_collect_register (struct regcache *regcache,
		       int use_64bit, int regno, union mips_register *reg)
{
  union mips_register tmp_reg;

  if (use_64bit)
    {
      collect_register (regcache, regno, &tmp_reg.reg64);
      *reg = tmp_reg;
    }
  else
    {
      collect_register (regcache, regno, &tmp_reg.reg32);
      reg->reg64 = tmp_reg.reg32;
    }
}

static void
mips_supply_register (struct regcache *regcache,
		      int use_64bit, int regno, const union mips_register *reg)
{
  int offset = 0;

  /* For big-endian 32-bit targets, ignore the high four bytes of each
     eight-byte slot.  */
  if (__BYTE_ORDER == __BIG_ENDIAN && !use_64bit)
    offset = 4;

  supply_register (regcache, regno, reg->buf + offset);
}

static void
mips_collect_register_32bit (struct regcache *regcache,
			     int use_64bit, int regno, unsigned char *buf)
{
  union mips_register tmp_reg;
  int reg32;

  mips_collect_register (regcache, use_64bit, regno, &tmp_reg);
  reg32 = tmp_reg.reg64;
  memcpy (buf, &reg32, 4);
}

static void
mips_supply_register_32bit (struct regcache *regcache,
			    int use_64bit, int regno, const unsigned char *buf)
{
  union mips_register tmp_reg;
  int reg32;

  memcpy (&reg32, buf, 4);
  tmp_reg.reg64 = reg32;
  mips_supply_register (regcache, use_64bit, regno, &tmp_reg);
}

static void
mips_fill_gregset (struct regcache *regcache, void *buf)
{
  union mips_register *regset = buf;
  int i, use_64bit;

  use_64bit = (register_size (0) == 8);

  for (i = 1; i < 32; i++)
    mips_collect_register (regcache, use_64bit, i, regset + i);

  mips_collect_register (regcache, use_64bit,
			 find_regno ("lo"), regset + 32);
  mips_collect_register (regcache, use_64bit,
			 find_regno ("hi"), regset + 33);
  mips_collect_register (regcache, use_64bit,
			 find_regno ("pc"), regset + 34);
  mips_collect_register (regcache, use_64bit,
			 find_regno ("badvaddr"), regset + 35);
  mips_collect_register (regcache, use_64bit,
			 find_regno ("status"), regset + 36);
  mips_collect_register (regcache, use_64bit,
			 find_regno ("cause"), regset + 37);

  mips_collect_register (regcache, use_64bit,
			 find_regno ("restart"), regset + 0);
}

static void
mips_store_gregset (struct regcache *regcache, const void *buf)
{
  const union mips_register *regset = buf;
  int i, use_64bit;

  use_64bit = (register_size (0) == 8);

  for (i = 0; i < 32; i++)
    mips_supply_register (regcache, use_64bit, i, regset + i);

  mips_supply_register (regcache, use_64bit, find_regno ("lo"), regset + 32);
  mips_supply_register (regcache, use_64bit, find_regno ("hi"), regset + 33);
  mips_supply_register (regcache, use_64bit, find_regno ("pc"), regset + 34);
  mips_supply_register (regcache, use_64bit,
			find_regno ("badvaddr"), regset + 35);
  mips_supply_register (regcache, use_64bit,
			find_regno ("status"), regset + 36);
  mips_supply_register (regcache, use_64bit,
			find_regno ("cause"), regset + 37);

  mips_supply_register (regcache, use_64bit,
			find_regno ("restart"), regset + 0);
}

static void
mips_fill_fpregset (struct regcache *regcache, void *buf)
{
  union mips_register *regset = buf;
  int i, use_64bit, first_fp, big_endian;

  use_64bit = (register_size (0) == 8);
  first_fp = find_regno ("f0");
  big_endian = (__BYTE_ORDER == __BIG_ENDIAN);

  /* See GDB for a discussion of this peculiar layout.  */
  for (i = 0; i < 32; i++)
    if (use_64bit)
      collect_register (regcache, first_fp + i, regset[i].buf);
    else
      collect_register (regcache, first_fp + i,
			regset[i & ~1].buf + 4 * (big_endian != (i & 1)));

  mips_collect_register_32bit (regcache, use_64bit,
			       find_regno ("fcsr"), regset[32].buf);
  mips_collect_register_32bit (regcache, use_64bit, find_regno ("fir"),
			       regset[32].buf + 4);
}

static void
mips_store_fpregset (struct regcache *regcache, const void *buf)
{
  const union mips_register *regset = buf;
  int i, use_64bit, first_fp, big_endian;

  use_64bit = (register_size (0) == 8);
  first_fp = find_regno ("f0");
  big_endian = (__BYTE_ORDER == __BIG_ENDIAN);

  /* See GDB for a discussion of this peculiar layout.  */
  for (i = 0; i < 32; i++)
    if (use_64bit)
      supply_register (regcache, first_fp + i, regset[i].buf);
    else
      supply_register (regcache, first_fp + i,
		       regset[i & ~1].buf + 4 * (big_endian != (i & 1)));

  mips_supply_register_32bit (regcache, use_64bit,
			      find_regno ("fcsr"), regset[32].buf);
  mips_supply_register_32bit (regcache, use_64bit, find_regno ("fir"),
			      regset[32].buf + 4);
}
#endif /* HAVE_PTRACE_GETREGS */

struct regset_info target_regsets[] = {
#ifdef HAVE_PTRACE_GETREGS
  { PTRACE_GETREGS, PTRACE_SETREGS, 0, 38 * 8, GENERAL_REGS,
    mips_fill_gregset, mips_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, 0, 33 * 8, FP_REGS,
    mips_fill_fpregset, mips_store_fpregset },
#endif /* HAVE_PTRACE_GETREGS */
  { 0, 0, 0, -1, -1, NULL, NULL }
};

struct linux_target_ops the_low_target = {
#ifdef __mips64
  init_registers_mips64_linux,
#else
  init_registers_mips_linux,
#endif
  mips_num_regs,
  mips_regmap,
  mips_cannot_fetch_register,
  mips_cannot_store_register,
  mips_get_pc,
  mips_set_pc,
  (const unsigned char *) &mips_breakpoint,
  mips_breakpoint_len,
  mips_reinsert_addr,
  0,
  mips_breakpoint_at,
  mips_insert_point,
  mips_remove_point,
  mips_stopped_by_watchpoint,
  mips_stopped_data_address,
  NULL,
  NULL,
  NULL, /* siginfo_fixup */
  mips_linux_new_process,
  mips_linux_new_thread,
  mips_linux_prepare_to_resume
};

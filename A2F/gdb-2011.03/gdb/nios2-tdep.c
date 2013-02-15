/* Target-machine dependent code for Nios2, for GDB.
   Copyright (C) 2003-2005
   by Peter Brookes (pbrookes@altera.com)
   and Andrew Draper (adraper@altera.com)

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

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "dwarf2-frame.h"
#include "symtab.h"
#include "inferior.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "dis-asm.h"
#include "regcache.h"
#include "value.h"
#include "symfile.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "gdb_assert.h"
#include "solib-svr4.h"
#include "glibc-tdep.h"
#include "infcall.h"
#include "regset.h"
#include "tramp-frame.h"
#include "linux-tdep.h"

/* To get entry_point_address.  */
#include "objfiles.h"

/* Nios II ISA specific encodings and macros */
#include "opcode/nios2.h"

/* Macros */

#define Z_REGNUM 0    /* Zero */
#define R2_REGNUM 2    /* used for return value */
#define R3_REGNUM 3    /* used for return value */
/* used for hidden zero argument to store ptr to struct return value */
#define R4_REGNUM 4     
#define R7_REGNUM 7
#define GP_REGNUM 26  /* Global Pointer */
#undef SP_REGNUM       
#define SP_REGNUM 27  /* Stack Pointer */
#undef FP_REGNUM     
#define FP_REGNUM 28  /* Frame Pointer */
#define EA_REGNUM 29  /* Exception address */
#define BA_REGNUM 30  /* Breakpoint return address */
#define RA_REGNUM 31  /* Return address */
#undef PC_REGNUM
#define PC_REGNUM 32
#define STATUS_REGNUM 33  /* Ctrl registers */
#define ESTATUS_REGNUM 34
#define BSTATUS_REGNUM 35
#define IENABLE_REGNUM 36
#define IPENDING_REGNUM 37
#define CPUID_REGNUM 38
#define EXCEPTION_REGNUM 40
#define PTEADDR_REGNUM 41
#define TLBACC_REGNUM 42
#define TLBMISC_REGNUM 43
#define FSTATUS_REGNUM 44
#define BADADDR_REGNUM 45
#define CONFIG_REGNUM 46
#define MPUBASE_REGNUM 47
#define MPUACC_REGNUM 48
#define FIRST_ARGREG R4_REGNUM
#define LAST_ARGREG R7_REGNUM
/* Number of all registers */
#define NIOS2_NUM_REGS (49)
/* The maximum register number displayed to the user, */
/* as a result of typing "info reg" at the gdb prompt */
#define NIOS2_MAX_REG_DISPLAYED_REGNUM (48)

#define NIOS2_OPCODE_SIZE 4

/* Structures */
struct register_info
{
  int size;
  char *name;
};

/* The current value in the register is the value in r[base] at the start of
 * the function + offset ; unless base < 0 in which case it's unknown.
 */
typedef struct
{
  int reg;
  unsigned int offset;

} REG_VALUE;


typedef struct
{
  int basereg;
  CORE_ADDR addr;

} REG_SAVED;

struct nios2_unwind_cache
{
  /* The frame's base, optionally used by the high-level debug info.  */
  CORE_ADDR base;

  /* The previous frame's inner most stack address.  Used as this
     frame ID's stack_addr.  */
  CORE_ADDR cfa;

  /* The address of the first instruction in this function */
  CORE_ADDR pc;

  /* Which register holds the return address for the frame. */
  int return_regnum;

  /* Table indicating what changes have been made to each register */
  REG_VALUE reg_value[NIOS2_NUM_REGS];

  /* Table indicating where each register has been saved.  */
  REG_SAVED reg_saved[NIOS2_NUM_REGS];
};


/* Function prototypes */
CORE_ADDR nios2_saved_pc_after_call (struct frame_info *fi);

/* nios2_register_info_table[i] is the number of bytes of storage in
   GDB's register array occupied by register i.  */
static struct register_info nios2_register_info_table[] = {
  /*  0 */ {4, "zero"},
  /*  1 */ {4, "at"},
  /*  2 */ {4, "r2"},
  /*  3 */ {4, "r3"},
  /*  4 */ {4, "r4"},
  /*  5 */ {4, "r5"},
  /*  6 */ {4, "r6"},
  /*  7 */ {4, "r7"},
  /*  8 */ {4, "r8"},
  /*  9 */ {4, "r9"},
  /* 10 */ {4, "r10"},
  /* 11 */ {4, "r11"},
  /* 12 */ {4, "r12"},
  /* 13 */ {4, "r13"},
  /* 14 */ {4, "r14"},
  /* 15 */ {4, "r15"},
  /* 16 */ {4, "r16"},
  /* 17 */ {4, "r17"},
  /* 18 */ {4, "r18"},
  /* 19 */ {4, "r19"},
  /* 20 */ {4, "r20"},
  /* 21 */ {4, "r21"},
  /* 22 */ {4, "r22"},
  /* 23 */ {4, "r23"},
  /* 24 */ {4, "et"},
  /* 25 */ {4, "bt"},
  /* 26 */ {4, "gp"},
  /* 27 */ {4, "sp"},
  /* 28 */ {4, "fp"},
  /* 29 */ {4, "ea"},
  /* 30 */ {4, "ba"},
  /* 31 */ {4, "ra"},
  /* 32 */ {4, "pc"},
  /* 33 */ {4, "status"},
  /* 34 */ {4, "estatus"},
  /* 35 */ {4, "bstatus"},
  /* 36 */ {4, "ienable"},
  /* 37 */ {4, "ipending"},
  /* 38 */ {4, "cpuid"}, 
  /* 39 */ {4, "ctl6"}, 
  /* 40 */ {4, "exception"}, 
  /* 41 */ {4, "pteaddr"},
  /* 42 */ {4, "tlbacc"},  
  /* 43 */ {4, "tlbmisc"},
  /* 44 */ {4, "ctl11"},
  /* 45 */ {4, "badaddr"},
  /* 46 */ {4, "config"},
  /* 47 */ {4, "mpubase"},
  /* 48 */ {4, "mpuacc"}
};

/* This array is a mapping from Dwarf-2 register 
   numbering to GDB's */
static int nios2_dwarf2gdb_regno_map[] = {
  0, 1, 2, 3,			
  4, 5, 6, 7,			
  8, 9, 10, 11,			
  12, 13, 14, 15,		
  16, 17, 18, 19,
  20, 21, 22, 23,
  24, 25, 
  GP_REGNUM,        /* 26 */
  SP_REGNUM,        /* 27 */
  FP_REGNUM,        /* 28 */ 
  EA_REGNUM,        /* 29 */	
  BA_REGNUM,        /* 30 */ 
  RA_REGNUM,        /* 31 */
  PC_REGNUM,        /* 32 */
  STATUS_REGNUM,    /* 33 */
  ESTATUS_REGNUM,   /* 34 */
  BSTATUS_REGNUM,   /* 35 */
  IENABLE_REGNUM,   /* 36 */
  IPENDING_REGNUM,  /* 37 */
  CPUID_REGNUM,     /* 38 */
  39, /* CTL6 */    /* 39 */
  EXCEPTION_REGNUM, /* 40 */
  PTEADDR_REGNUM,   /* 41 */
  TLBACC_REGNUM,    /* 42 */
  TLBMISC_REGNUM,   /* 43 */
  FSTATUS_REGNUM,   /* 44 */
  BADADDR_REGNUM,   /* 45 */
  CONFIG_REGNUM,    /* 46 */
  MPUBASE_REGNUM,   /* 47 */
  MPUACC_REGNUM     /* 48 */
};

/* Dwarf-2 <-> GDB register numbers mapping.  */
int
nios2_dwarf_reg_to_regnum (struct gdbarch *gdbarch, int dw_reg)
{
  if (dw_reg < 0 || dw_reg > NIOS2_NUM_REGS)
    {
      warning ("Dwarf-2 uses unmapped register #%d\n", dw_reg);
      return dw_reg;
    }

  return nios2_dwarf2gdb_regno_map[dw_reg];
}

/* Find the name for the specified NIOS2 regno */
static const char *
nios2_register_name (struct gdbarch *gdbarch, int regno)
{
  /* Don't display any registers after NIOS2_MAX_REG_DISPLAYED_REGNUM */
  if (regno < 0)
    return NULL;
  else if (regno > NIOS2_MAX_REG_DISPLAYED_REGNUM)
    return NULL;
  return nios2_register_info_table[regno].name;
}

/* nios2_register_byte_table[i] is the offset into the register file of the
   start of register number i.  We initialize this from
   nios2_register_info_table.  */
int nios2_register_byte_table[NIOS2_NUM_REGS];

/* Index within `registers' of the first byte of the space for register REGNO.  */
int
nios2_register_byte (int regno)
{
  return nios2_register_byte_table[regno];
}

/* Number of bytes of storage in the actual machine representation for
   register 'regno'.  */
static int
nios2_register_raw_size (int regno)
{
  return nios2_register_info_table[regno].size;
}

/* Return the GDB type object for the "standard" data type
   of data in register 'regno'.  */
static struct type *
nios2_register_type (struct gdbarch *gdbarch, int regno)
{
  gdb_assert (nios2_register_info_table[regno].size == 4);
  
  return builtin_type (gdbarch)->builtin_uint32;
}

/* Given a return value in `regcache' with a type `valtype', 
   extract and copy its value into `valbuf'. */
void
nios2_extract_return_value (struct type *valtype, struct regcache *regcache,
			    gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (valtype);

  /* pointer types are returned in register r2,
     up to 32-bit types in r2
     up to 64-bit types in r2,r3  */
  if (len <= nios2_register_raw_size (R2_REGNUM))
    regcache_cooked_read (regcache, R2_REGNUM, valbuf);
  else if (len <= (nios2_register_raw_size (R2_REGNUM)
		  + nios2_register_raw_size (R3_REGNUM)))
    {
      regcache_cooked_read (regcache, R2_REGNUM, valbuf);
      regcache_cooked_read (regcache, R3_REGNUM, valbuf+4);
    }
}

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format. */
void
nios2_store_return_value (struct type *valtype, struct regcache *regcache,
			  const gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (valtype);

  /* return values of up to 8 bytes are returned in $r2 $r3 */

  if (len <= nios2_register_raw_size (R2_REGNUM))
    regcache_cooked_write (regcache, R2_REGNUM, valbuf);
  else if (len <= (nios2_register_raw_size (R2_REGNUM)
		   + nios2_register_raw_size (R3_REGNUM)))
    {
      regcache_cooked_write (regcache, R2_REGNUM, valbuf);
      regcache_cooked_write (regcache, R3_REGNUM, valbuf+4);
    }
}

/* This function analyzes the function prologue and tries to work
   out where registers are saved and how long the prologue is.
   The prologue will consist of the following parts:
     1) Optional profiling instrumentation.  The old version uses six
        instructions.  We step over this if there is an exact match.
	  nextpc r8
	  mov	 r9, ra
	  movhi	 r10, %hiadj(.LP2)
	  addi	 r10, r10, %lo(.LP2)
	  call	 mcount
	  mov	 ra, r9
	The new version uses two or three instructions (the last of
	these might get merged in with the STW which saves RA to the
	stack).  We interpret these.
	  mov	 r8, ra
	  call	 mcount
	  mov	 ra, r8

     2) Optional interrupt entry decision.  Again, we step over
        this if there is an exact match.
	  rdctl  et,estatus
	  andi   et,et,1
	  beq    et,zero, <software_exception>
	  rdctl  et,ipending
	  beq    et,zero, <software_exception>

     3) A stack adjustment or stack which, which will be one of:
	  addi   sp, sp, -constant
	or:
	  movi   r8, constant
	  sub    sp, sp, r8
	or
	  movhi  r8, constant
	  addi   r8, r8, constant
	  sub    sp, sp, r8
	or
	  movhi  rx, %hiadj(newstack)
	  addhi  rx, rx, %lo(newstack)
	  stw    sp, constant(rx)
	  mov    sp, rx

     4) An optional stack check, which can take either of these forms:
	  bgeu   sp, rx, +8
	  break  3
	or
	  bltu   sp, rx, .Lstack_overflow
	  ...
	.Lstack_overflow:
	  break  3

     5) Saving any registers which need to be saved.  These will
        normally just be stored onto the stack:
	  stw    rx, constant(sp)
	but in the large frame case will use r8 as an offset back
	to the cfa:
	  add    r8, r8, sp
	  stw    rx, -constant(r8)

	Saving control registers looks slightly different:
	  rdctl  rx, ctlN
	  stw    rx, constant(sp)

     6) An optional FP setup, either if the user has requested a
        frame pointer or if the function calls alloca.
        This is always:
	  mov    fp, sp

    The prologue instructions may be interleaved, and the register
    saves and FP setup can occur in either order.

    To cope with all this variability we decode all the instructions
    from the start of the prologue until we hit a branch, call or 
    return.  For each of the instructions mentioned in 3, 4 and 5 we
    handle the limited cases of stores to the stack and operations
    on constant values.
 */

typedef struct
{
  unsigned int insn;
  unsigned int mask;
} wild_insn;

static const wild_insn profiler_insn[] =
{
  { 0x0010e03a, 0x00000000 }, // nextpc r8
  { 0xf813883a, 0x00000000 }, // mov    r9,ra
  { 0x02800034, 0x003FFFC0 }, // movhi  r10,257
  { 0x52800004, 0x003FFFC0 }, // addi   r10,r10,-31992
  { 0x00000000, 0xFFFFFFC0 }, // call   <mcount>
  { 0x483f883a, 0x00000000 }  // mov    ra,r9
};

static const wild_insn irqentry_insn[] =
{
  { 0x0031307a, 0x00000000 }, // rdctl  et,estatus
  { 0xc600004c, 0x00000000 }, // andi   et,et,1
  { 0xc0000026, 0x003FFFC0 }, // beq    et,zero, <software_exception>
  { 0x0031313a, 0x00000000 }, // rdctl  et,ipending
  { 0xc0000026, 0x003FFFC0 }  // beq    et,zero, <software_exception>
};

static void
nios2_setup_default(struct nios2_unwind_cache *cache)
{
  int i;

  for (i = 0; i < NIOS2_NUM_REGS; i++)
  {
    /* All registers start off holding their previous values */
    cache->reg_value[i].reg    = i;
    cache->reg_value[i].offset = 0;

    /* All registers start off not saved */
    cache->reg_saved[i].basereg = -1;
    cache->reg_saved[i].addr    = 0;
  }
}

static int
nios2_match_sequence (struct gdbarch *gdbarch, CORE_ADDR start_pc,
		      const wild_insn *sequence, int count)
{
  CORE_ADDR pc = start_pc;
  int i;
  unsigned int insn;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  for (i = 0 ; i < count ; i++)
    {
      insn = read_memory_unsigned_integer (pc, NIOS2_OPCODE_SIZE, byte_order);
      if ((insn & ~sequence[i].mask) != sequence[i].insn)
	return 0;

      pc += NIOS2_OPCODE_SIZE;
    }

  return 1;
}


CORE_ADDR
nios2_analyze_prologue (struct gdbarch *gdbarch, const CORE_ADDR start_pc,
			const CORE_ADDR current_pc,
			struct nios2_unwind_cache *cache,
			struct frame_info *this_frame)
{
  /* Maximum lines of prologue to check */
  /* Note that this number should not be too large, else we can potentially */
  /* end up iterating through unmapped memory */
  CORE_ADDR limit_pc = start_pc + 200;
  int regno;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  /* Does the frame set up the FP register? */
  int base_reg = 0;

  REG_VALUE * value = cache->reg_value;
  REG_VALUE temp_value[NIOS2_NUM_REGS];

  int i;

  /* Save the starting PC so we can correct the pc after running */
  /* through the prolog, using symbol info */
  CORE_ADDR pc = start_pc;

  /* Is this an exception handler? */
  int exception_handler = 0;

  /* What was the original value of SP (or fake original value for
   * functions which switch stacks?
   */
  CORE_ADDR frame_high;

  /* Is this the end of the prologue? */
  int within_prologue = 1;

  CORE_ADDR prologue_end;

  /* Is this the innermost function? */
  int innermost = (frame_relative_level (this_frame) == 0);

#ifdef DEBUG_PRINT
    fprintf_unfiltered (gdb_stdlog,
	    "{ nios2_analyze_prologue start=%s, current=%s ",
	    paddress (gdbarch, start_pc), paddress (gdbarch, current_pc));
#endif

  /* Set up the default values of the registers. */
  nios2_setup_default(cache);

  /* If the first few instructions are the profile entry then skip over them. */
  /* Newer versions of the compiler use more efficient profiling code. */
  if (nios2_match_sequence (gdbarch, pc, profiler_insn,
			    sizeof (profiler_insn) / sizeof (profiler_insn[0])))
    pc += (sizeof (profiler_insn) / sizeof (profiler_insn[0]))
	  * NIOS2_OPCODE_SIZE;

  /* If the first few are an interrupt entry then skip over them too */
  if (nios2_match_sequence (gdbarch, pc, irqentry_insn,
			    sizeof (irqentry_insn) / sizeof (irqentry_insn[0])))
    {
      pc += (sizeof (irqentry_insn) / sizeof(irqentry_insn[0]))
	    * NIOS2_OPCODE_SIZE;
      exception_handler = 1;
    }

  prologue_end = start_pc;

  /* Find the prologue instructions.  */
  /* Fortunately we're in 32bit paradise */
  while (pc < limit_pc && within_prologue)
    {
      /* Present instruction.  */
      unsigned int insn;

      int prologue_insn = 0;

      if (pc == current_pc)
      {
	/*
	 * When we reach the current PC we must save the current register
	 * state (for the backtrace) but keep analysing because there might
	 * be more to find out (eg. is this an exception handler).
	 */
	memcpy(temp_value, value, sizeof(temp_value));
	value = temp_value;
#ifdef DEBUG_PRINT
	fprintf_unfiltered (gdb_stdlog, "*");
#endif
      }

      insn = read_memory_unsigned_integer (pc, NIOS2_OPCODE_SIZE, byte_order);
      pc += NIOS2_OPCODE_SIZE;

#ifdef DEBUG_PRINT
      fprintf_unfiltered (gdb_stdlog, "[%08X]", insn);
#endif

      /* The following instructions can appear in the prologue */

      if ((insn & 0x0001FFFF) == 0x0001883A)
	{
	  /* ADD   rc, ra, rb  (also used for MOV) */

	  int ra = GET_IW_A(insn);
	  int rb = GET_IW_B(insn);
	  int rc = GET_IW_C(insn);

	  if (rc == SP_REGNUM && rb == 0 && value[ra].reg == cache->reg_saved[SP_REGNUM].basereg)
	    {
	      /* If the previous value of SP is available somewhere near the new
	       * stack pointer value then this is a stack switch. 
	       */

	      /* If any registers were saved on the stack before then we can't backtrace
	       * into them now.
	       */
	      for (i = 0 ; i < NIOS2_NUM_REGS ; i++)
		{
		  if (cache->reg_saved[i].basereg == SP_REGNUM)
		    cache->reg_saved[i].basereg = -1;
		  if (value[i].reg == SP_REGNUM)
		    value[i].reg = -1;
		}

	      /* Create a fake "high water mark" 4 bytes above where SP was stored
	       * and fake up the registers to be consistent with that.
	       */
	      value[SP_REGNUM].reg = SP_REGNUM;
	      value[SP_REGNUM].offset = value[ra].offset - cache->reg_saved[SP_REGNUM].addr - 4;

	      cache->reg_saved[SP_REGNUM].basereg = SP_REGNUM;
	      cache->reg_saved[SP_REGNUM].addr = -4;
	    }

	  else if (rc != 0)
	    {
	      if (value[rb].reg == 0)
		value[rc].reg = value[ra].reg;
	      else if (value[ra].reg == 0)
		value[rc].reg = value[rb].reg;
	      else
		value[rc].reg = -1;
  
	      value[rc].offset = value[ra].offset + value[rb].offset;
	    }
	  prologue_insn = 1;
	}

      else if ((insn & 0x0001FFFF) == 0x0001983A)
	{
	  /* SUB   rc, ra, rb */

	  int ra = GET_IW_A(insn);
	  int rb = GET_IW_B(insn);
	  int rc = GET_IW_C(insn);

	  if (rc != 0)
	    {
	      if (value[rb].reg == 0)
		value[rc].reg = value[ra].reg;
	      else
		value[rc].reg = -1;

	      value[rc].offset = value[ra].offset - value[rb].offset;
	    }
	}

      else if ((insn & 0x0000003F) == 0x00000004)
	{
	  /* ADDI  rb, ra, immed   (also used for MOVI) */
	  short immed = GET_IW_IMM16(insn);
	  int ra = GET_IW_A(insn);
	  int rb = GET_IW_B(insn);

	  if (rb == SP_REGNUM)
	    {
	      /* The first stack adjustment is part of the prologue.  Any subsequent
	         stack adjustments are either down to alloca or the epilogue so stop
		 analysing when we hit them. */
	      if (value[rb].offset != 0 || value[ra].reg != SP_REGNUM)
		break;
	    }

	  if (rb != 0)
	    {
	      value[rb].reg    = value[ra].reg;
	      value[rb].offset = value[ra].offset + immed;
	    }

	  prologue_insn = 1;
	}

      else if ((insn & 0x0000003F) == 0x00000034)
	{
	  /* ORHI  rb, ra, immed   (also used for MOVHI) */
	  unsigned int immed = GET_IW_IMM16(insn);
	  int ra = GET_IW_A(insn);
	  int rb = GET_IW_B(insn);

	  if (rb != 0)
	    {
  	      value[rb].reg    = (value[ra].reg == 0) ? 0 : -1;
	      value[rb].offset = value[ra].offset | (immed << 16);
	    }
	}

      else if (((insn & IW_OP_MASK) == OP_STW || (insn & IW_OP_MASK) == OP_STWIO))
        {
	  /* STW rb, immediate(ra) */

	  short immed16 = GET_IW_IMM16(insn);
	  int ra = GET_IW_A(insn);
	  int rb = GET_IW_B(insn);

	  /* Are we storing the original value of a register?  For exception handlers
	   * the value of EA-4 (return address from interrupts etc) is sometimes stored.
	   */
	  int orig = value[rb].reg;
	  if (orig > 0 && (value[rb].offset == 0 || (orig == EA_REGNUM && value[rb].offset == -4)))
	    {
	      /* We are most interested in stores to the stack, but will also take note
	       * of stores to other places as they might be useful later.
	       */
	      if ((value[ra].reg == SP_REGNUM && cache->reg_saved[orig].basereg != SP_REGNUM) ||
		  cache->reg_saved[orig].basereg == -1) 
		{
		  if (pc < current_pc)
		    {
		      /* Save off callee saved registers */
		      cache->reg_saved[orig].basereg = value[ra].reg;
		      cache->reg_saved[orig].addr    = value[ra].offset + GET_IW_IMM16(insn);
		    }

		  prologue_insn = 1;

		  if (orig == EA_REGNUM || orig == ESTATUS_REGNUM)
		    exception_handler = 1;
		}
	    }
        }

      else if ((insn & 0xFFC1F83F) == 0x0001303A)
	{
	  /* RDCTL rC, ctlN */
	  int rc = GET_IW_C(insn);
	  int n = GET_IW_CONTROL_REGNUM(insn);

	  if (rc != 0)
	    {
	      value[rc].reg    = STATUS_REGNUM + n;
	      value[rc].offset = 0;
	    }

	  prologue_insn = 1;
        }

      else if ((insn & 0x0000003F) == 0 &&
		value[8].reg == RA_REGNUM && value[8].offset == 0 &&
		value[SP_REGNUM].reg == SP_REGNUM && value[SP_REGNUM].offset == 0)
	{
	  /* A CALL instruction.  This is treated as a call to mcount if ra
	   * has been stored into r8 beforehand and if it's before the stack adjust.
	   * mcount corrupts r2-r3, r9-r15 & ra 
	   */
	  for (i = 2 ; i <= 3 ; i++)
	    value[i].reg = -1;
	  for (i = 9 ; i <= 15 ; i++)
	    value[i].reg = -1;
	  value[RA_REGNUM].reg = -1;

	  prologue_insn = 1;
	}

      else if ((insn & 0xF83FFFFF) == 0xD800012E)
	{
	   /* BGEU sp, rx, +8
	    * BREAK 3
	    * This instruction sequence is used in stack checking - we can ignore it
	    */
	  unsigned int next_insn
	    = read_memory_unsigned_integer (pc, NIOS2_OPCODE_SIZE, byte_order);

	  if (next_insn != 0x003DA0FA)
	    within_prologue = 0;
	  else
	    pc += NIOS2_OPCODE_SIZE;
	}

      else if ((insn & 0xF800003F) == 0xD8000036)
	{
	   /* BLTU sp, rx, .Lstackoverflow
	    * If the location branched to holds a BREAK 3 instruction then this is
	    * also stack overflow detection.  We can ignore it.
	    */
	  CORE_ADDR target_pc = pc + ((insn & 0x3FFFC0) >> 6);
	  unsigned int target_insn
	    = read_memory_unsigned_integer (target_pc, NIOS2_OPCODE_SIZE,
					    byte_order);

	  if (target_insn != 0x003DA0FA)
	    within_prologue = 0;
	}

      /* Any other instructions are allowed to be moved up into the prologue.  If we
         reach a branch, call or return then the prologue is considered over */
      /* We also consider a second stack adjustment as terminating the prologue (see
         above) */

      else
	{
	  switch (GET_IW_OP(insn))
	    {
	    case OP_BEQ:
	    case OP_BGE:
	    case OP_BGEU:
	    case OP_BLT:
	    case OP_BLTU:
	    case OP_BNE:
	    case OP_BR:
	    case OP_CALL:
	      within_prologue = 0;
	      break;
	    case OP_OPX:
	      if (GET_IW_OPX(insn) == OPX_RET ||
		  GET_IW_OPX(insn) == OPX_ERET ||
		  GET_IW_OPX(insn) == OPX_BRET ||
		  GET_IW_OPX(insn) == OPX_CALLR ||
		  GET_IW_OPX(insn) == OPX_JMP)
		{
		  within_prologue = 0;
		}
	    default:
	      break;
	    }
	}

      if (prologue_insn)
	prologue_end = pc;
    }

  /* Are we within the function epilogue?  If so then we should go back to the
     original register values */
  if (innermost && current_pc > start_pc)
    {
      /* First check whether the previous instruction was a stack
         adjustment. */
      unsigned int insn
        = read_memory_unsigned_integer (current_pc - NIOS2_OPCODE_SIZE,
					NIOS2_OPCODE_SIZE, byte_order);

      if ((insn & 0xFFC0003C) == 0xDEC00004 || /* ADDI sp, sp, */
	  (insn & 0xFFC1FFFF) == 0xDEC1883A || /* ADD  sp, sp, */
	  (insn & 0xFFC0003F) == 0xDEC00017)   /* LDW  sp, constant(sp) */
	{
	  /* Then check if it's followed by a return or a tail call */
          insn = read_memory_unsigned_integer (current_pc, NIOS2_OPCODE_SIZE,
					       byte_order);

	  if (insn == 0xF800283A ||                /* RET */
	      insn == 0xE800083A ||                /* ERET */
	      (insn & 0x07FFFFFF) == 0x0000683A || /* JMP */
	      (insn & 0xFFC0003F) == 6)            /* BR */
	    {
	      /* Go back to default value of the registers */
	      nios2_setup_default(cache);
	    }
	}
    }

  /* Exception handlers use a different return address register. */
  if (exception_handler)
    cache->return_regnum = EA_REGNUM;

#ifdef DEBUG_PRINT
  fprintf_unfiltered (gdb_stdlog, "\n-> retreg=%d, ", cache->return_regnum);
#endif

  if (cache->reg_value[FP_REGNUM].reg == SP_REGNUM)
    {
      /* If the FP now holds an offset from the CFA then this is a normal
         frame which uses the frame pointer. */
      base_reg = FP_REGNUM;
    }
  else if (cache->reg_value[SP_REGNUM].reg == SP_REGNUM)
    {
      /* FP doesn't hold an offset from the CFA.  If SP still holds an
         offset from the CFA then we might be in a function which omits
	 the frame pointer, or we might be partway through the prologue.
	 In both cases we can find the CFA using SP. */
      base_reg = SP_REGNUM;
    }
  else
    {
      /* Somehow the stack pointer has been corrupted. We can't return. */
#ifdef DEBUG_PRINT
      fprintf_unfiltered (gdb_stdlog, "<can't reach cfa> }\n");
#endif
      return 0;
    }

  if (cache->reg_value[base_reg].offset == 0 || 
      cache->reg_saved[RA_REGNUM].basereg != SP_REGNUM || 
      cache->reg_saved[cache->return_regnum].basereg != SP_REGNUM)
    {
      /*
       * If the frame didn't adjust the stack, didn't save RA or didn't save
       * EA in an exception handler then it must either be a leaf function
       * (doesn't call any other functions) or it can't return.  If it has
       * called another function then it can't be a leaf, so set base == 0
       * to indicate that we can't backtrace past it.
       */

      if (!innermost)
	{
	  /* If it isn't the innermost function then it can't be a leaf, unless
	   * it was interrupted.  Check whether RA for this frame is the same
	   * as PC.  If so then it probably wan't interrupted.
	   */
	  char buf[8];
	  CORE_ADDR ra;

	  ra = get_frame_register_unsigned (this_frame, RA_REGNUM);

	  if (ra == current_pc)
	    {
#ifdef DEBUG_PRINT
	      fprintf_unfiltered (gdb_stdlog, "<noreturn ADJUST %s, r31@r%d+?>, r%d@r%d+?> }\n", 
		                  paddress (gdbarch,
					    cache->reg_value[base_reg].offset),
				  cache->reg_saved[RA_REGNUM].basereg,
				  cache->return_regnum, cache->reg_saved[cache->return_regnum].basereg);
#endif
	      return 0;
	    }
	}
    }

  /* Get the value of whichever register we are using for the base. */
  cache->base = get_frame_register_unsigned (this_frame, base_reg);

  /* What was the value of SP at the start of this function (or just
   * after the stack switch).
   */
  frame_high = cache->base - cache->reg_value[base_reg].offset;

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < NIOS2_NUM_REGS; i++)
    if (cache->reg_saved[i].basereg == SP_REGNUM)
      {
	cache->reg_saved[i].basereg = Z_REGNUM;
	cache->reg_saved[i].addr += frame_high;
      }

  for (i = 0; i < NIOS2_NUM_REGS; i++)
    if (cache->reg_saved[i].basereg == GP_REGNUM)
      {
	CORE_ADDR gp = get_frame_register_unsigned (this_frame, GP_REGNUM);

	for ( ; i < NIOS2_NUM_REGS; i++)
	  if (cache->reg_saved[i].basereg == GP_REGNUM)
	    {
	      cache->reg_saved[i].basereg = Z_REGNUM;
	      cache->reg_saved[i].addr += gp;
	    }
      }

  /* Work out what the value of SP was on the first instruction of this
   * function.  If we didn't switch stacks then this can be trivially
   * computed from the base address.
   */
  if (cache->reg_saved[SP_REGNUM].basereg == Z_REGNUM)
    {
      cache->cfa
        = read_memory_unsigned_integer (cache->reg_saved[SP_REGNUM].addr, 4,
					byte_order);
    }
  else
    cache->cfa = frame_high;

  /* Exception handlers restore ESTATUS into STATUS. */
  if (exception_handler)
  {
    cache->reg_saved[STATUS_REGNUM] = cache->reg_saved[ESTATUS_REGNUM];
    cache->reg_saved[ESTATUS_REGNUM].basereg = -1;
  }

#ifdef DEBUG_PRINT
  fprintf_unfiltered (gdb_stdlog, "cfa=%s }\n", paddress (gdbarch, cache->cfa));
#endif

  return prologue_end;
}

struct frame_info *
setup_arbitrary_frame (int argc, CORE_ADDR *argv)
{
  if (argc != 2)
    error ("Nios II frame specifications require two arguments: sp and pc");

  return create_new_frame (argv[0], argv[1]);
}

/* Given a PC value corresponding to the start of a function, return the PC
   of the first instruction after the function prologue.  */

CORE_ADDR
nios2_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR start_pc)
{
  CORE_ADDR limit_pc;
  CORE_ADDR func_addr;
  
  /* See if we can determine the end of the prologue via the symbol table.
     If so, then return either PC, or the PC after the prologue, whichever is
     greater.  */
  if (find_pc_partial_function (start_pc, NULL, &func_addr, NULL))
    {
      CORE_ADDR post_prologue_pc
        = skip_prologue_using_sal (gdbarch, func_addr);
      if (post_prologue_pc != 0)
        return max (start_pc, post_prologue_pc);
    }
  
  /* Can't determine prologue from the symbol table, need to examine
     instructions.  */
  return nios2_analyze_prologue (gdbarch, start_pc, start_pc, NULL, NULL);
}

const unsigned char*
nios2_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *bp_addr,
			  int *bp_size)
{
  /* break encoding: 31->27  26->22  21->17  16->11 10->6 5->0 */
  /*                 00000   00000   0x1d    0x2d   11111 0x3a */
  /*                 00000   00000   11101   101101 11111 111010 */
  /* In bytes:       00000000 00111011 01101111 11111010 */
  /*                 0x0       0x3b    0x6f     0xfa */
  static unsigned char breakpoint[] = {0xfa, 0x6f, 0x3b, 0x0};
  *bp_size = 4;
  return breakpoint;
}


int
gdb_print_insn_nios2 (bfd_vma memaddr, disassemble_info *info)
{
  if (info->endian == BFD_ENDIAN_BIG)
    {
      return print_insn_big_nios2 (memaddr, info);
    }
  else
    {
      return print_insn_little_nios2 (memaddr, info);
    }
}



/* Adjust the address downward (direction of stack growth) so that it
   is correctly aligned for a new stack frame.  */

/* ??? Big endian issues here? */

static CORE_ADDR
nios2_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return align_down (addr, 4);
}



/* We don't convert anything at the moment */
static int
nios2_convert_register_p (struct gdbarch *gdbarch, int regnum,
			  struct type *type)
{
  return 0;
}

static void
nios2_register_to_value (struct frame_info *frame, int regnum,
			 struct type *type, gdb_byte *to)
{
  get_frame_register (frame, regnum + 0, (char *) to + 0);
  get_frame_register (frame, regnum + 0, (char *) to + 0);
}

static void
nios2_value_to_register (struct frame_info *frame, int regnum,
			 struct type *type, const gdb_byte *from)
{
  put_frame_register (frame, regnum + 0, (const char *) from + 0);
  put_frame_register (frame, regnum + 0, (const char *) from + 0);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
nios2_return_value (struct gdbarch *gdbarch, struct type *func_type,
		    struct value *function, struct regcache *regcache,
		    gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (TYPE_LENGTH (value_type (function)) > 8)
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    nios2_extract_return_value (value_type (function), regcache, readbuf);
  if (writebuf)
    nios2_store_return_value (value_type (function), regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Assuming THIS_FRAME is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos() and returned from
   nios2_push_dummy_call, and the PC needs to match the dummy frame's
   breakpoint.  */

static struct frame_id
nios2_dummy_id (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  return frame_id_build
    (get_frame_register_unsigned (this_frame, SP_REGNUM),
     get_frame_pc (this_frame));
}

static CORE_ADDR
nios2_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
                       struct regcache *regcache, CORE_ADDR bp_addr,
                       int nargs, struct value **args, CORE_ADDR sp,
                       int struct_return, CORE_ADDR struct_addr)
{
  int argreg;
  int float_argreg;
  int argnum;
  int len = 0;
  int stack_offset = 0;
  CORE_ADDR func_addr = find_function_addr (function, NULL);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  /* Set the return address register to point to the entry point of
     the program, where a breakpoint lies in wait.  */
  regcache_cooked_write_signed (regcache, RA_REGNUM, bp_addr);

  /* Now make space on the stack for the args.  */
  for (argnum = 0; argnum < nargs; argnum++)
    len += align_up (TYPE_LENGTH (value_type (args[argnum])), 4);
  sp -= len;

  /* Initialize the register pointer.  */
  argreg = FIRST_ARGREG;

  /* The struct_return pointer occupies the first parameter-passing reg.  */
  if (struct_return)
    regcache_cooked_write_unsigned (regcache, argreg++, struct_addr);

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  Loop thru args
     from first to last.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      const gdb_byte *val;
      gdb_byte valbuf[MAX_REGISTER_SIZE];
      struct value *arg = args[argnum];
      struct type *arg_type = check_typedef (value_type (arg));
      int len = TYPE_LENGTH (arg_type);
      enum type_code typecode = TYPE_CODE (arg_type);

      val = value_contents (arg);

      /* Copy the argument to general registers or the stack in
         register-sized pieces.  Large arguments are split between
         registers and stack.  */
      while (len > 0)
        {
	  int partial_len = (len < 4 ? len : 4);

	  if (argreg <= LAST_ARGREG)
	    {
	      /* The argument is being passed in a register.  */
	      CORE_ADDR regval = extract_unsigned_integer (val, partial_len,
							   byte_order);
	      regcache_cooked_write_unsigned (regcache, argreg, regval);
	      argreg++;
	    }
	  else
	    {
	      /* The argument is being passed on the stack.  */
	      CORE_ADDR addr = sp + stack_offset;
	      write_memory (addr, val, partial_len);
	      stack_offset += align_up (partial_len, 4);
	    }

	  len -= partial_len;
	  val += partial_len;
	}
    }

  regcache_cooked_write_signed (regcache, SP_REGNUM, sp);

  /* Return adjusted stack pointer.  */
  return sp;
}

static CORE_ADDR
nios2_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char buf[8];

  frame_unwind_register (next_frame, PC_REGNUM, buf);
  return extract_typed_address (buf, builtin_type (gdbarch)->builtin_func_ptr);
}

static CORE_ADDR
nios2_unwind_sp (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  return frame_unwind_register_unsigned (this_frame, SP_REGNUM);
}


/* Frame base handling.  */

struct nios2_unwind_cache *
nios2_frame_unwind_cache (struct frame_info *this_frame,
			  void **this_prologue_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  CORE_ADDR current_pc;
  struct nios2_unwind_cache *cache;
  int i;

  if (*this_prologue_cache)
    return *this_prologue_cache;

  cache = FRAME_OBSTACK_ZALLOC (struct nios2_unwind_cache);
  (*this_prologue_cache) = cache;

  /* Zero all fields.  */
  cache->base = 0;
  cache->cfa = 0;
  cache->pc = 0;

  cache->return_regnum = RA_REGNUM;

  nios2_setup_default(cache);

  cache->pc = get_frame_func (this_frame);
  current_pc = get_frame_pc (this_frame);

  /* Prologue analysis does the rest...  */
  if (cache->pc != 0)
    nios2_analyze_prologue (gdbarch, cache->pc, current_pc, cache, this_frame);

  return cache;
}

static void
nios2_frame_this_id (struct frame_info *this_frame, void **this_cache,
		     struct frame_id *this_id)
{
  struct nios2_unwind_cache *cache =
    nios2_frame_unwind_cache (this_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  (*this_id) = frame_id_build (cache->cfa, cache->pc);
}

static struct value *
nios2_frame_prev_register (struct frame_info *this_frame, void **this_cache,
			   int regnum)
{
  struct nios2_unwind_cache *cache =
    nios2_frame_unwind_cache (this_frame, this_cache);

  gdb_assert (regnum >= 0);

  /* The PC of the previous frame is stored in the RA register of
     the current frame.  Frob regnum so that we pull the value from
     the correct place.  */
  if (regnum == PC_REGNUM)
    regnum = cache->return_regnum;

  if (regnum == SP_REGNUM && cache->cfa)
    return frame_unwind_got_constant (this_frame, regnum, cache->cfa);

  /* If we've worked out where a register is stored then load it from there.
   */
  if (regnum < NIOS2_NUM_REGS && cache->reg_saved[regnum].basereg == Z_REGNUM)
    return frame_unwind_got_memory (this_frame, regnum,
				    cache->reg_saved[regnum].addr);

  return frame_unwind_got_register (this_frame, regnum, regnum);
}

static CORE_ADDR
nios2_frame_base_address (struct frame_info *this_frame, void **this_cache)
{
  struct nios2_unwind_cache *info
    = nios2_frame_unwind_cache (this_frame, this_cache);
  return info->base;
}

static const struct frame_unwind nios2_frame_unwind =
{
  NORMAL_FRAME,
  nios2_frame_this_id,
  nios2_frame_prev_register,
  NULL,
  default_frame_sniffer
};

static const struct frame_base nios2_frame_base =
{
  &nios2_frame_unwind,
  nios2_frame_base_address,
  nios2_frame_base_address,
  nios2_frame_base_address
};



static struct trad_frame_cache *
nios2_stub_frame_cache (struct frame_info *this_frame, void **this_cache)
{
  CORE_ADDR pc;
  CORE_ADDR start_addr;
  CORE_ADDR stack_addr;
  struct trad_frame_cache *this_trad_cache;
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  int num_regs = gdbarch_num_regs (gdbarch);

  if ((*this_cache) != NULL)
    return (*this_cache);
  this_trad_cache = trad_frame_cache_zalloc (this_frame);
  (*this_cache) = this_trad_cache;

  /* The return address is in the link register.  */
  trad_frame_set_reg_realreg (this_trad_cache,
                              gdbarch_pc_regnum (gdbarch),
                              RA_REGNUM);

  /* Frame ID, since it's a frameless / stackless function, no stack
     space is allocated and SP on entry is the current SP.  */
  pc = get_frame_pc (this_frame);
  find_pc_partial_function (pc, NULL, &start_addr, NULL);
  stack_addr = get_frame_register_unsigned (this_frame, SP_REGNUM);
  trad_frame_set_id (this_trad_cache, frame_id_build (start_addr, stack_addr));
  /* Assume that the frame's base is the same as the
     stack-pointer.  */
  trad_frame_set_this_base (this_trad_cache, stack_addr);

  return this_trad_cache;
}

static void
nios2_stub_frame_this_id (struct frame_info *this_frame, void **this_cache,
                          struct frame_id *this_id)
{
  struct trad_frame_cache *this_trad_cache
    = nios2_stub_frame_cache (this_frame, this_cache);
  trad_frame_get_id (this_trad_cache, this_id);
}

static struct value *
nios2_stub_frame_prev_register (struct frame_info *this_frame,
			        void **this_cache, int regnum)
{
  struct trad_frame_cache *this_trad_cache
    = nios2_stub_frame_cache (this_frame, this_cache);
  return trad_frame_get_register (this_trad_cache, this_frame, regnum);
}

static int
nios2_stub_frame_sniffer (const struct frame_unwind *self,
			  struct frame_info *this_frame, void **cache)
{
  gdb_byte dummy[4];
  struct obj_section *s;
  CORE_ADDR pc = get_frame_address_in_block (this_frame);

  /* Use the stub unwinder for unreadable code.  */
  if (target_read_memory (get_frame_pc (this_frame), dummy, 4) != 0)
    return 1;

  if (in_plt_section (pc, NULL))
    return 1;

  return 0;
}

static const struct frame_unwind nios2_stub_frame_unwind =
{
  NORMAL_FRAME,
  nios2_stub_frame_this_id,
  nios2_stub_frame_prev_register,
  NULL,
  nios2_stub_frame_sniffer
};

static CORE_ADDR
nios2_stub_frame_base_address (struct frame_info *this_frame, void **this_cache)
{
  struct trad_frame_cache *this_trad_cache
    = nios2_stub_frame_cache (this_frame, this_cache);
  return trad_frame_get_this_base (this_trad_cache);
}

static const struct frame_base nios2_stub_frame_base =
{
  &nios2_stub_frame_unwind,
  nios2_stub_frame_base_address,
  nios2_stub_frame_base_address,
  nios2_stub_frame_base_address
};

static unsigned long
nios2_fetch_instruction (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  return read_memory_unsigned_integer (pc, NIOS2_OPCODE_SIZE, byte_order);
}

/* Determine where to set a single step breakpoint while considering
   branch prediction.  */
static CORE_ADDR
nios2_get_next_pc (struct frame_info *frame, CORE_ADDR pc)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  unsigned long inst;
  int op;
  int imm16;
  int ra;
  int rb;
  int ras;
  int rbs;
  unsigned int rau;
  unsigned int rbu;

  inst = nios2_fetch_instruction (gdbarch, pc);
  pc += NIOS2_OPCODE_SIZE;

  imm16 = (short) GET_IW_IMM16(inst);
  ra = GET_IW_A(inst);
  rb = GET_IW_B(inst);
  ras = get_frame_register_signed (frame, ra);
  rbs = get_frame_register_signed (frame, rb);
  rau = get_frame_register_unsigned (frame, ra);
  rbu = get_frame_register_unsigned (frame, rb);

  switch (GET_IW_OP(inst))
    {
    case OP_BEQ:
      if (ras == rbs)
	pc += imm16;
      break;

    case OP_BGE:
      if (ras >= rbs)
        pc += imm16;
      break;

    case OP_BGEU:
      if (rau >= rbu)
        pc += imm16;
      break;

    case OP_BLT:
      if (ras < rbs)
        pc += imm16;
      break;

    case OP_BLTU:
      if (rau < rbu)
        pc += imm16;
      break;

    case OP_BNE:
      if (ras != rbs)
        pc += imm16;
      break;

    case OP_BR:
      pc += imm16;
      break;

    case OP_JMPI:
    case OP_CALL:
      pc = (pc & 0xf0000000) | (GET_IW_IMM26(inst) << 2);
      break;

    case OP_OPX:
      switch (GET_IW_OPX(inst))
	{
	case OPX_JMP:
	case OPX_CALLR:
	case OPX_RET:
	  pc = ras;
	  break;
	default:
	  break;
	}
      break;
    default:
      break;
    }
  return pc;
}


/* nios2_software_single_step() is called just before we want to resume the
   inferior, if we want to single-step it but there is no hardware or kernel
   single-step support.  We find the target of the coming instruction
   and breakpoint it.  */

int
nios2_software_single_step (struct frame_info *frame)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  struct address_space *aspace = get_frame_address_space (frame);
  CORE_ADDR next_pc = nios2_get_next_pc (frame, get_frame_pc (frame));

  insert_single_step_breakpoint (gdbarch, aspace, next_pc);

  return 1;
}

/* Core file and register set support.  */

static const int reg_offsets[NIOS2_NUM_REGS] =
  {-1,  8,  9, 10, 11, 12, 13, 14,
    0,  1,  2,  3,  4,  5,  6,  7,
   23, 24, 25, 26, 27, 28, 29, 30,
   -1, -1, 19, 18, 17, 21, -1, 16,
   21, -1, 20, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1,
   -1};

void
nios2_supply_gregset (const struct regset *regset,
                      struct regcache *regcache,
                      int regnum, const void *gregs_buf, size_t len)
{
  const gdb_byte *gregs = gregs_buf;
  int regno;
  static const gdb_byte zero_buf[4] = {0, 0, 0, 0};

  for (regno = Z_REGNUM; regno <= MPUACC_REGNUM; regno++)
    if (regnum == -1 || regnum == regno)
      {
	if (reg_offsets[regno] != -1)
	  regcache_raw_supply (regcache, regno,
			       gregs + 4 * reg_offsets[regno]);
	else
	  regcache_raw_supply (regcache, regno, zero_buf);
      }
}

static struct regset nios2_regset =
  {
    NULL,
    nios2_supply_gregset
  };

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
nios2_regset_from_core_section (struct gdbarch *gdbarch,
                                const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0)
    return &nios2_regset;

  return NULL;
}

/* Initialize a trad-frame cache corresponding to the tramp-frame.
   FUNC is the address of the instruction TRAMP[0] in memory.  */
static void
nios2_linux_sigreturn_init (const struct tramp_frame *self,
			    struct frame_info *next_frame,
			    struct trad_frame_cache *this_cache,
			    CORE_ADDR func)
{
  CORE_ADDR base = func + 16;
  int i;

  for (i = 0; i < 8; i++)
    trad_frame_set_reg_addr (this_cache, i+8, base + i * 4);
  for (i = 0; i < 7; i++)
    trad_frame_set_reg_addr (this_cache, i+1, base + (i+8)*4);
  trad_frame_set_reg_addr (this_cache, RA_REGNUM, base + 16*4);
  trad_frame_set_reg_addr (this_cache, FP_REGNUM, base + 17*4);
  trad_frame_set_reg_addr (this_cache, SP_REGNUM, base + 18*4);
  trad_frame_set_reg_addr (this_cache, GP_REGNUM, base + 19*4);
  trad_frame_set_reg_addr (this_cache, ESTATUS_REGNUM, base + 20*4);
  trad_frame_set_reg_addr (this_cache, PC_REGNUM, base + 21*4);

  /* Save a frame ID.  */
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

/* Initialize a trad-frame cache corresponding to the tramp-frame.
   FUNC is the address of the instruction TRAMP[0] in memory.  */
static void
nios2_linux_rt_sigreturn_init (const struct tramp_frame *self,
			       struct frame_info *next_frame,
			       struct trad_frame_cache *this_cache,
			       CORE_ADDR func)
{
  CORE_ADDR sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  CORE_ADDR base = func + 41*4;
  int i;

  for (i = 0; i < 23; i++)
    trad_frame_set_reg_addr (this_cache, i+1, base + i * 4);
  trad_frame_set_reg_addr (this_cache, RA_REGNUM, base + 23*4);
  trad_frame_set_reg_addr (this_cache, FP_REGNUM, base + 24*4);
  trad_frame_set_reg_addr (this_cache, GP_REGNUM, base + 25*4);
  trad_frame_set_reg_addr (this_cache, PC_REGNUM, base + 27*4);
  trad_frame_set_reg_addr (this_cache, SP_REGNUM, base + 28*4);

  /* Save a frame ID.  */
  trad_frame_set_id (this_cache, frame_id_build (sp, func));
}

static struct tramp_frame nios2_linux_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { 0x00800004 | (119 << 6), -1 },  /* movi r2,__NR_sigreturn */
    { 0x003b683a, -1 },               /* trap */
    { TRAMP_SENTINEL_INSN }
  },
  nios2_linux_sigreturn_init
};

static struct tramp_frame nios2_linux_rt_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { 0x00800004 | (173 << 6), -1 },  /* movi r2,__NR_rt_sigreturn */
    { 0x003b683a, -1 },               /* trap */
    { TRAMP_SENTINEL_INSN }
  },
  nios2_linux_rt_sigreturn_init
};

static struct gdbarch *
nios2_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  int register_bytes, i;

  /* Find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found, create a new architecture from the information
     provided.  We don't have any architecture specific state, so just
     pass in 0 for the struct gdbarch_tdep parameter.  */
  gdbarch = gdbarch_alloc (&info, (struct gdbarch_tdep *)0);

  /* Data type sizes.  */
  set_gdbarch_ptr_bit (gdbarch, 32);
  set_gdbarch_addr_bit (gdbarch, 32);
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 32);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);

  set_gdbarch_float_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format (gdbarch, floatformats_ieee_double);

  /* The register set.  */
  set_gdbarch_num_regs (gdbarch, NIOS2_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PC_REGNUM);	/* Pseudo register PC */

  set_gdbarch_register_name (gdbarch, nios2_register_name);
  set_gdbarch_register_type (gdbarch, nios2_register_type);

   /* The "default" register numbering scheme for AMD64 is referred to
      as the "DWARF Register Number Mapping" in the System V psABI.
      dwarf1 isn't supported, so provide the mapping for stabs and dwarf2.  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, nios2_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, nios2_dwarf_reg_to_regnum);

  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
 
  /* Call dummy code.  */
  set_gdbarch_frame_align (gdbarch, nios2_frame_align); 

  set_gdbarch_convert_register_p (gdbarch, nios2_convert_register_p); 
  set_gdbarch_register_to_value (gdbarch, nios2_register_to_value); 
  set_gdbarch_value_to_register (gdbarch, nios2_value_to_register); 

  set_gdbarch_return_value (gdbarch, nios2_return_value); 

  set_gdbarch_skip_prologue (gdbarch, nios2_skip_prologue);
  set_gdbarch_breakpoint_from_pc (gdbarch, nios2_breakpoint_from_pc);

  set_gdbarch_dummy_id (gdbarch, nios2_dummy_id); 
  set_gdbarch_unwind_pc (gdbarch, nios2_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, nios2_unwind_sp);

  /* The dwarf2 unwinder will normally produce the best results if the
  debug information is available, so register it first. */
  dwarf2_append_unwinders (gdbarch);

  frame_unwind_append_unwinder (gdbarch, &nios2_stub_frame_unwind);
  frame_unwind_append_unwinder (gdbarch, &nios2_frame_unwind);

  /* Single stepping.  */
  set_gdbarch_software_single_step (gdbarch, nios2_software_single_step);

  /* Shared library handling.  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  set_solib_svr4_fetch_link_map_offsets (gdbarch,
					 svr4_ilp32_fetch_link_map_offsets);

  frame_base_set_default (gdbarch, &nios2_frame_base);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  set_gdbarch_print_insn (gdbarch, gdb_print_insn_nios2);

  /* Enable inferior call support.  */
  set_gdbarch_push_dummy_call (gdbarch, nios2_push_dummy_call);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section (gdbarch,
                                        nios2_regset_from_core_section);

  tramp_frame_prepend_unwinder (gdbarch,
                                &nios2_linux_sigreturn_tramp_frame);
  tramp_frame_prepend_unwinder (gdbarch,
                                &nios2_linux_rt_sigreturn_tramp_frame);

  return gdbarch;
}

void
_initialize_nios2_tdep (void)
{
  int i, offset = 0;

  register_gdbarch_init (bfd_arch_nios2, nios2_gdbarch_init);

  /* Do not display anything after NIOS2_MAX_REG_DISPLAYED_REGNUM */
  for (i = 0; i <= NIOS2_MAX_REG_DISPLAYED_REGNUM; i++)
    {
       nios2_register_byte_table[i] = offset;
       offset += nios2_register_info_table[i].size;
    }

  for (i = NIOS2_MAX_REG_DISPLAYED_REGNUM + 1; i < NIOS2_NUM_REGS; i++)
    {
       nios2_register_byte_table[i] = -1;
    }    
  
}

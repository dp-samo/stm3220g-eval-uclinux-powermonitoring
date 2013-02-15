/* Handle FDPIC shared libraries for GDB, the GNU Debugger.
   Copyright (C) 2010 Free Software Foundation, Inc.

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

/* FDPIC dynamic libraries work like svr4 shared libraries in many ways,
   but not quite.  The main differences are that even the main executable
   can be relocated unpredictably, and the read-only and writable sections
   of the inferior can be relocated independently (indeed, there may be an
   arbitrary number of separate regions).

   The load maps cannot be determined by reading from program memory,
   because it's not at a fixed location.  Instead they must be obtained
   from the Linux kernel using a special ptrace call.

   The added complexity of the load maps means that we need more than a
   single offset to do the relocation.  Each relocation requires a table
   lookup to determine what range it falls into.  */

#include "defs.h"
#include "arch-utils.h"
#include "solib-svr4.h"
#include "solist.h"
#include "bfd.h"
#include "exec.h"
#include "target.h"
#include "objfiles.h"
#include "cli/cli-decode.h"

/* elf.h is not available on all host platforms.  */
#include "stdint.h"
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;

/* These data structures are taken from elf-fdpic.h in uCLibc.  */

struct elf32_fdpic_loadseg
{
  /* Core address to which the segment is mapped.  */
  Elf32_Addr addr;
  /* VMA recorded in the program header.  */
  Elf32_Addr p_vaddr;
  /* Size of this segment in memory.  */
  Elf32_Word p_memsz;
};

struct elf32_fdpic_loadmap {
  /* Protocol version number, must be zero.  */
  Elf32_Half version;
  /* Number of segments in this map.  */
  Elf32_Half nsegs;
  /* The actual memory map.  */
  struct elf32_fdpic_loadseg segs[/*nsegs*/];
};

/* Load maps for the main executable and the interpreter.
   These are obtained from ptrace.  They are the starting point for getting
   into the program, and are required to find the solib list with the
   individual load maps for each module.  */

static struct elf32_fdpic_loadmap *exec_loadmap = NULL, *interp_loadmap = NULL;


/* normalize_loadmap_endian

   Convert a load map from target endian to host endian.  */

static void
normalize_loadmap_endian (struct elf32_fdpic_loadmap *map)
{
  enum bfd_endian byte_order = gdbarch_byte_order (target_gdbarch);
  int i;

#define NORMAILIZE_ENDIAN(field) \
  (field) = extract_unsigned_integer ((gdb_byte*)&(field), sizeof (field), \
				      byte_order);

  NORMAILIZE_ENDIAN (map->version);
  NORMAILIZE_ENDIAN (map->nsegs);

  for (i = 0; i < map->nsegs; i++)
    {
      NORMAILIZE_ENDIAN (map->segs[i].addr);
      NORMAILIZE_ENDIAN (map->segs[i].p_vaddr);
      NORMAILIZE_ENDIAN (map->segs[i].p_memsz);
    }

#undef NORMAILIZE_ENDIAN
}

static void
fdpic_print_loadmap (struct elf32_fdpic_loadmap *map)
{
  int i;

  if (map == NULL)
    printf_filtered ("(null)\n");
  else if (map->version != 0)
    printf_filtered (_("Unsupported map version: %d\n"), map->version);
  else
    for (i = 0; i < map->nsegs; i++)
      printf_filtered ("0x%08x:0x%08x -> 0x%08x:0x%08x\n",
		       map->segs[i].p_vaddr,
		       map->segs[i].p_vaddr + map->segs[i].p_memsz,
		       map->segs[i].addr,
		       map->segs[i].addr + map->segs[i].p_memsz);
}

/* fdpic_get_initial_loadmaps

   Interrogate the Linux kernel to find out where the program was loaded.
   There are two load maps; one for the executable and one for the
   interpreter (only in the case of a dynamically linked executable).  */

static void
fdpic_get_initial_loadmaps (void)
{
  if (0 >= target_read_alloc (&current_target, TARGET_OBJECT_FDPIC,
			      "exec", (gdb_byte**)&exec_loadmap))
    {
      error (_("Error reading FDPIC exec loadmap\n"));
      exec_loadmap = NULL;
      return;
    }
  else
    normalize_loadmap_endian (exec_loadmap);

  if (0 >= target_read_alloc (&current_target, TARGET_OBJECT_FDPIC,
			      "interp", (gdb_byte**)&interp_loadmap))
    interp_loadmap = NULL;
  else
    normalize_loadmap_endian (interp_loadmap);
}

/* fdpic_find_load_offset

   Return the offset from the given objfile address to the actual loaded
   address.  Note that, unlike normal svr4, different sections in the same
   objfile might have different offsets.  */

static CORE_ADDR
fdpic_find_load_offset (CORE_ADDR addr, struct elf32_fdpic_loadmap *map)
{
  int seg;

  for (seg = 0; seg < map->nsegs; seg++)
    {
      CORE_ADDR sectionoffset = addr - map->segs[seg].p_vaddr;
      if (sectionoffset >= 0 && sectionoffset < map->segs[seg].p_memsz)
	{
	  return map->segs[seg].addr - map->segs[seg].p_vaddr;
	}
    }

  /* The address did not match the load map.  */
  return 0;
}

/* fdpic_relocate_file

   Relocate all the symbols/sections in the given file.

   If SYMFILE is NULL then relocate BFD instead.
   If SYMFILE is non-NULL, then BFD is not used.  */

static int
fdpic_relocate_file (struct objfile *symfile, bfd *bfd,
                     struct elf32_fdpic_loadmap *map)
{
  if (map == NULL)
    return 0;

  if (symfile)
    {
      struct section_offsets *new_offsets;
      struct obj_section *osect;
      int i = 0;

      new_offsets = alloca (symfile_objfile->num_sections
			      * sizeof (*new_offsets));

      ALL_OBJFILE_OSECTIONS (symfile_objfile, osect)
	new_offsets->offsets[i++] =
	  fdpic_find_load_offset (bfd_get_section_vma (osect->objfile->abfd,
	                                               osect->the_bfd_section),
	                          map);

      objfile_relocate (symfile_objfile, new_offsets);
    }
  else if (bfd)
    {
      asection *asect;

      for (asect = exec_bfd->sections; asect != NULL; asect = asect->next)
	{
          CORE_ADDR vma = bfd_get_section_vma (exec_bfd, asect);
	  exec_set_section_address (bfd_get_filename (exec_bfd), asect->index,
				    vma + fdpic_find_load_offset (vma, map));
	}
    }
  else
    return 0;

  return 1;
}


static void
fdpic_clear_solib (void)
{
  /* GDB crashes if this function doesn't exist.  */
}

static void
fdpic_solib_create_inferior_hook (int from_tty)
{
  if (exec_bfd == NULL)
    return;

  fdpic_get_initial_loadmaps ();

  if (!fdpic_relocate_file (symfile_objfile, exec_bfd, exec_loadmap))
    return;

  /* TODO: actually place a hook.  */
}

static struct so_list *
fdpic_current_sos (void)
{
  /* FDPIC shared libraries are not yet supported.  */
  return NULL;
}

static void
info_fdpic_command (char *args, int from_tty)
{
  printf_filtered (_("Main executable load map:\n"));
  fdpic_print_loadmap (exec_loadmap);
}


struct target_so_ops fdpic_so_ops;

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern initialize_file_ftype _initialize_fdpic_solib;

void
_initialize_fdpic_solib (void)
{
  fdpic_so_ops.clear_solib = fdpic_clear_solib;
  fdpic_so_ops.solib_create_inferior_hook =
      fdpic_solib_create_inferior_hook;
  fdpic_so_ops.current_sos = fdpic_current_sos;

  add_info ("fdpic", info_fdpic_command, _("Display the FDPIC load maps.\n"));
}

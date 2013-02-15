/* vi: set sw=4 ts=4: */
/* SuperH ELF shared library loader suppport
 *
 * Copyright (C) 2002, Stefan Allius <allius@atecom.com> and
 *                     Eddie C. Dost <ecd@atecom.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the above contributors may not be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Program to load an ELF binary on a linux system, and run it.
   References to symbols in sharable libraries can be resolved by either
   an ELF sharable library or a linux style of shared library. */

/* Disclaimer:  I have never seen any AT&T source code for SVr4, nor have
   I ever taken any courses on internals.  This program was developed using
   information available through the book "UNIX SYSTEM V RELEASE 4,
   Programmers guide: Ansi C and Programming Support Tools", which did
   a more than adequate job of explaining everything required to get this
   working. */

#include "ldso.h"

extern int _dl_linux_resolve(void);

#ifdef __SH_FDPIC__
static int
is_in_module (void *address, struct elf32_fdpic_loadmap *map)
{
  int i;
  for (i = 0; i < map->nsegs; i++)
    if (address >= map->segs[i].addr
	&& address < map->segs[i].addr + map->segs[i].p_memsz)
      return 1;
  return 0;
}
#endif

#ifdef __SH_FDPIC__
struct funcdesc_value volatile *
#else
unsigned long
#endif
_dl_linux_resolver(struct elf_resolve *tpnt, int reloc_entry)
{
	ELF_RELOC *this_reloc;
	char *strtab;
	Elf32_Sym *symtab;
	int symtab_index;
	char *rel_addr;
	char *symname;
	char *new_addr;

#ifdef __SH_FDPIC__
	struct elf_resolve *new_tpnt;
	struct funcdesc_value funcval;
	struct funcdesc_value volatile *got_addr;
#else
	char **got_addr;
	unsigned long instr_addr;
#endif

#ifdef __SH_FDPIC__
	/* In FDPIC the parameters are not what they appear to be, but they can be
	   used to derive the right information.

	   On entry,
	     TPNT is the elf_resolve pointer for the calling function, OR, if the
	     relocation has already been done by a competing thread, it might be
	     the elf_resolve pointer for the function we are about to call.
	     RELOC_ENTRY is the address of the lazy PLT stub. The actual value will
	     be at address reloc_entry-4.

	   In order to calculate the true values of the parameters, we need to
	   detect, and possibly correct, the race condition.  */
	struct elf_resolve *module = tpnt;
	void *lazyPLT = (void *)reloc_entry;

	if (!is_in_module (lazyPLT, module->loadaddr.map))
	  {
	    /* The race condition has occurred!  */

	    /* Traverse the module list and find the one containing lazyPLT.  */
	    while (module->prev)
	      module = module->prev;
	    for (; module; module = module->next)
	      if (is_in_module (lazyPLT, module->loadaddr.map))
		break;

	    if (!module)
	      {
		/* No module was found.  This shouldn't ever happen.  */
		_dl_dprintf (2, "%s: internal error in lazy relocation\n", _dl_progname);
		_dl_exit(1);
	      }
	  }

	/* We now know the proper values for the parameters.  */
	tpnt = module;
	reloc_entry = ((int*)lazyPLT)[-1];
#endif

	rel_addr = (char *)tpnt->dynamic_info[DT_JMPREL];

	this_reloc = (ELF_RELOC *)(intptr_t)(rel_addr + reloc_entry);
	symtab_index = ELF32_R_SYM(this_reloc->r_info);

	symtab = (Elf32_Sym *)(intptr_t) tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *)tpnt->dynamic_info[DT_STRTAB];
	symname = strtab + symtab[symtab_index].st_name;

#ifdef __SH_FDPIC__
	/* Address of GOT entry fix up */
	got_addr = (struct funcdesc_value *)
	  DL_RELOC_ADDR (tpnt->loadaddr, this_reloc->r_offset);
#else
	/* Address of jump instruction to fix up */
	instr_addr = (unsigned long) (this_reloc->r_offset + tpnt->loadaddr);
	got_addr = (char **) instr_addr;
#endif

	/* Get the address of the GOT entry */
	new_addr = _dl_lookup_hash(symname, tpnt->symbol_scope, tpnt, ELF_RTYPE_CLASS_PLT
#if __SH_FDPIC__
				   , &new_tpnt
#endif
				  );
	if (unlikely(!new_addr)) {
		_dl_dprintf(2, "%s: can't resolve symbol '%s'\n", _dl_progname, symname);
		_dl_exit(1);
	}

#ifdef __SH_FDPIC__
	funcval.entry_point = new_addr;
	funcval.got_value = new_tpnt->loadaddr.got_value;
#endif

#if defined (__SUPPORT_LD_DEBUG__)
	if ((unsigned long) got_addr < 0x20000000) {
		if (_dl_debug_bindings) {
			_dl_dprintf(_dl_debug_file, "\nresolve function: %s", symname);
			if (_dl_debug_detail) _dl_dprintf(_dl_debug_file,
#ifdef __SH_FDPIC__
					"\n\tpatched (%x,%x) ==> (%x,%x) @ %x\n",
					got_addr->entry_point, got_addr->got_value,
					funcval.entry_point, funcval.got_value,
					got_addr);
#else
					"\n\tpatched %x ==> %x @ %x\n", *got_addr, new_addr, got_addr);
#endif
		}
	}
	if (!_dl_debug_nofixups)
#endif
#ifdef __SH_FDPIC__
	/* To deal with a race condtion, the GOT value must be written first.  */
	got_addr->got_value = funcval.got_value;
	got_addr->entry_point = funcval.entry_point;
#else
	*got_addr = new_addr;
#endif

#ifdef __SH_FDPIC__
	return got_addr;
#else
	return (unsigned long) new_addr;
#endif
}


static int
_dl_parse(struct elf_resolve *tpnt, struct dyn_elf *scope,
	  unsigned long rel_addr, unsigned long rel_size,
	  int (*reloc_fnc) (struct elf_resolve *tpnt, struct dyn_elf *scope,
			    ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab))
{
	unsigned int i;
	char *strtab;
	Elf32_Sym *symtab;
	ELF_RELOC *rpnt;
	int symtab_index;
	/* Now parse the relocation information */

	rpnt = (ELF_RELOC *)(intptr_t) rel_addr;
	rel_size = rel_size / sizeof(ELF_RELOC);

	symtab = (Elf32_Sym *)(intptr_t)tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *)tpnt->dynamic_info[DT_STRTAB];

	for (i = 0; i < rel_size; i++, rpnt++) {
		int res;

		symtab_index = ELF32_R_SYM(rpnt->r_info);
		debug_sym(symtab,strtab,symtab_index);
		debug_reloc(symtab,strtab,rpnt);

		res = reloc_fnc(tpnt, scope, rpnt, symtab, strtab);

		if (res == 0) continue;

		_dl_dprintf(2, "\n%s: ",_dl_progname);

		if (symtab_index)
			_dl_dprintf(2, "symbol '%s': ", strtab + symtab[symtab_index].st_name);

		if (unlikely(res < 0)) {
			int reloc_type = ELF32_R_TYPE(rpnt->r_info);
#if defined (__SUPPORT_LD_DEBUG__)
			_dl_dprintf(2, "can't handle reloc type %s\n ", _dl_reltypes(reloc_type));
#else
			_dl_dprintf(2, "can't handle reloc type %x\n", reloc_type);
#endif
			_dl_exit(-res);
		}
		if (unlikely(res > 0)) {
			_dl_dprintf(2, "can't resolve symbol\n");
			return res;
		}
	}
	return 0;
}


static int
_dl_do_reloc (struct elf_resolve *tpnt,struct dyn_elf *scope,
	      ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab)
{
	int reloc_type;
	int symtab_index;
	char *symname;
	unsigned long *reloc_addr;
	unsigned long symbol_addr;
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif
#ifdef __SH_FDPIC__
	struct funcdesc_value funcval;
	struct elf_resolve *symbol_tpnt;
#endif

	reloc_addr   = (unsigned long *) DL_RELOC_ADDR(tpnt->loadaddr, rpnt->r_offset);
	reloc_type = ELF32_R_TYPE(rpnt->r_info);
	symtab_index = ELF32_R_SYM(rpnt->r_info);
	symbol_addr = 0;
	symname = strtab + symtab[symtab_index].st_name;

#ifdef __SH_FDPIC__
	if (ELF_ST_BIND (symtab[symtab_index].st_info) == STB_LOCAL) {
		symbol_addr = (unsigned long) DL_RELOC_ADDR(tpnt->loadaddr, symtab[symtab_index].st_value);
		symbol_tpnt = tpnt;
	} else
#endif
	if (symtab_index) {
		symbol_addr = (unsigned long) _dl_lookup_hash(symname, scope, tpnt,
							      elf_machine_type_class(reloc_type)
#ifdef __SH_FDPIC__
							      , &symbol_tpnt
#endif
							     );

		/*
		 * We want to allow undefined references to weak symbols - this might
		 * have been intentional.  We should not be linking local symbols
		 * here, so all bases should be covered.
		 */
		if (!symbol_addr && ELF32_ST_BIND(symtab[symtab_index].st_info) != STB_WEAK) {
			_dl_dprintf(2, "%s: can't resolve symbol '%s'\n",
			            _dl_progname, strtab + symtab[symtab_index].st_name);

			/*
			 * The caller should handle the error: undefined reference to weak symbols
			 * are not fatal.
			 */
			return 1;
		}
	}
#ifdef __SH_FDPIC__
	else
	  symbol_tpnt = tpnt;
#endif

#if defined (__SUPPORT_LD_DEBUG__)
	old_val = *reloc_addr;
#endif
	switch (reloc_type) {
		case R_SH_NONE:
			break;
		case R_SH_COPY:
			if (symbol_addr) {
#if defined (__SUPPORT_LD_DEBUG__)
				if (_dl_debug_move)
					_dl_dprintf(_dl_debug_file,"\n%s move %x bytes from %x to %x",
						    symname, symtab[symtab_index].st_size,
						    symbol_addr, reloc_addr);
#endif
				_dl_memcpy((char *) reloc_addr, (char *) symbol_addr, symtab[symtab_index].st_size);
			}
			return 0; /* no further LD_DEBUG messages for copy relocs */
		case R_SH_DIR32:
		case R_SH_GLOB_DAT:
		case R_SH_JMP_SLOT:
			*reloc_addr = symbol_addr + rpnt->r_addend;
			break;
		case R_SH_REL32:
			*reloc_addr = symbol_addr + rpnt->r_addend -
					(unsigned long) reloc_addr;
			break;
		case R_SH_RELATIVE:
			*reloc_addr = (unsigned long) DL_RELOC_ADDR (tpnt->loadaddr, rpnt->r_addend);
			break;
#ifdef __SH_FDPIC__
		case R_SH_FUNCDESC_VALUE:
			funcval.entry_point = (void*)symbol_addr;
			/* The addend of FUNCDESC_VALUE
			   relocations referencing global
			   symbols must be ignored, because it
			   may hold the address of a lazy PLT
			   entry.  */
			if (ELF_ST_BIND(symtab[symtab_index].st_info) == STB_LOCAL)
				funcval.entry_point += *reloc_addr;
			if (symbol_addr)
				funcval.got_value
					= symbol_tpnt->loadaddr.got_value;
			else
				funcval.got_value = 0;
			*(struct funcdesc_value *)reloc_addr = funcval;
			break;
		case R_SH_FUNCDESC:
			if (symbol_addr)
				*reloc_addr = (unsigned long)_dl_funcdesc_for(
						(char *)symbol_addr + *reloc_addr,
						symbol_tpnt->loadaddr.got_value);
			else
				*reloc_addr = 0;
			break;
#endif
		default:

			return -1;
	}
#if defined (__SUPPORT_LD_DEBUG__)
	    if (_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\tpatched: %x ==> %x @ %x", old_val, *reloc_addr, reloc_addr);
#endif

	return 0;
}


static int
_dl_do_lazy_reloc (struct elf_resolve *tpnt, struct dyn_elf *scope,
		   ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab)
{
	int reloc_type;
#ifdef __SH_FDPIC__
	struct funcdesc_value volatile *reloc_addr;
	struct funcdesc_value funcval;
#else
	unsigned long *reloc_addr;
#endif
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif
	(void)scope;
	(void)symtab;
	(void)strtab;

	reloc_addr = DL_RELOC_ADDR (tpnt->loadaddr, rpnt->r_offset);
	reloc_type = ELF32_R_TYPE(rpnt->r_info);

#if defined (__SUPPORT_LD_DEBUG__)
#ifdef __SH_FDPIC__
	old_val = (unsigned long)reloc_addr->entry_point;
#else
	old_val = *reloc_addr;
#endif
#endif
	switch (reloc_type) {
		case R_SH_NONE:
			break;
#ifdef __SH_FDPIC__
		case R_SH_FUNCDESC_VALUE:
			funcval = *reloc_addr;
			funcval.entry_point = DL_RELOC_ADDR(tpnt->loadaddr, funcval.entry_point);
			funcval.got_value = tpnt->loadaddr.got_value;
			*reloc_addr = funcval;
			break;
#else
		case R_SH_JMP_SLOT:
			*reloc_addr = (unsigned long) DL_RELOC_ADDR (tpnt->loadaddr, *reloc_addr);
			break;
#endif
		default:
			return -1;
	}
#if defined (__SUPPORT_LD_DEBUG__)
	if (_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\tpatched: %x ==> %x @ %x", old_val,
#ifdef __SH_FDPIC__
			    reloc_addr->entry_point,
#else
			    *reloc_addr,
#endif
			    reloc_addr);
#endif
	return 0;

}

void _dl_parse_lazy_relocation_information(struct dyn_elf *rpnt,
	unsigned long rel_addr, unsigned long rel_size)
{
	(void)_dl_parse(rpnt->dyn, NULL, rel_addr, rel_size, _dl_do_lazy_reloc);
}

int _dl_parse_relocation_information(struct dyn_elf *rpnt,
	unsigned long rel_addr, unsigned long rel_size)
{
	return _dl_parse(rpnt->dyn, rpnt->dyn->symbol_scope, rel_addr, rel_size, _dl_do_reloc);
}

#if defined __SH_FDPIC__ && !defined IS_IN_libdl
# include "../../libc/sysdeps/linux/sh/crtreloc.c"
#endif

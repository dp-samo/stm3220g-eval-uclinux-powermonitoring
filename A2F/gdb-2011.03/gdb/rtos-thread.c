/* Low level interface for debugging RTOS threads

   Copyright 2007 Free Software Foundation, Inc.
   Copyright 2007 Viosoft Corporation (www.viosoft.com)

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


/* This module uses the libXXXdbg.a libraries that provide kernel debugging
   for embedded realtime OSes.  */

#include "defs.h"
#include "gdb_assert.h"
#include "gdbthread.h"
#include "target.h"
#include "inferior.h"
#include "regcache.h"
#include "gdbcmd.h"
#include "gdb_string.h"

#define RTOS_CLIENT 1
#include "rtos-thread.h"

#if defined (_WIN32) && !defined (__CYGWIN32__)
#include <windows.h>
typedef HMODULE rtos_lib_t;

static inline char *
rtos_unspecified_error (void)
{
  return "unspecified error";
}

#define rtos_dlopen(libname)  \
  LoadLibrary (libname)
#define rtos_dlclose(libhandle) \
  FreeLibrary (libhandle)
#define rtos_dlerror() \
  rtos_unspecified_error ()
#define rtos_dlsym(libhandle, symbolname) \
  ((void *) GetProcAddress ((libhandle), (symbolname)))

#else
#include <dlfcn.h>
typedef void *rtos_lib_t;

#define rtos_dlopen(libname) \
  dlopen (libname, RTLD_LAZY)
#define rtos_dlclose(libhandle) \
  dlclose (libhandle)
#define rtos_dlerror() \
  dlerror ()
#define rtos_dlsym(libhandle, symbolname) \
  dlsym ((libhandle), (symbolname))

#endif

/* Saved ops from the current target.  */
static void (*raw_fetch_registers) (struct target_ops *,
				    struct regcache *, int regno) = 0;
static void (*raw_store_registers) (struct target_ops *,
				    struct regcache *, int regno) = 0;
static ptid_t (*raw_wait) (struct target_ops *,
			   ptid_t ptid, struct target_waitstatus *status,
			   int options) = 0;

/* 1 if the plug-in is loaded.  */
static int plugin_loaded = 0;

/* Temp buffer for reading/writing memory and its len in bytes.  */
static char *scratchbuf = 0;
static int scratchbuf_len = 0;

/* Return handle to required plug-in symbol.  */
static void *
rtos_required_sym (rtos_lib_t handle, const char *name)
{
  void *sym = rtos_dlsym (handle, name);
  if (sym == NULL)
    error ("Symbol \"%s\" not found in RTOS plugin: %s", name,
	   rtos_dlerror ());
  return (sym);
}

/* Return handle to optional plug-in symbol.  */
static void *
rtos_optional_sym (rtos_lib_t handle, const char *name)
{
  void *sym = rtos_dlsym (handle, name);
  if (sym == NULL)
    warning ("Symbol \"%s\" not found in RTOS plugin: %s", name,
	     rtos_dlerror ());
  return (sym);
}

/* RTOS target-specific operations.  */

static struct target_ops rtos_thread_ops;
static raw_target_ops_t rtos_target_raw_ops;

/* Open the RTOS plug-in target.  */
static void
rtos_client_thread_open (char *name, int from_tty)
{
  const char *object_type;

  printf_filtered ("RTOS thread debugging enabled for:\n");
  printf_filtered ("\t%s\n", (*rtos_getinfo) ());

  /* Get list of all known object types.  */
  object_type = rtos_get_all_object_types (0);
  while (object_type != NULL)
    {
      printf ("Object %s\n", object_type);
      object_type = rtos_get_all_object_types (1);
    }

  push_target (&rtos_thread_ops);

  /* Add a default global thread.  */
  add_thread (null_ptid);

  /* Set the inferior ptid.  */
  inferior_ptid = pid_to_ptid (rtos_find_active_tid ());
  rtos_set_current_tid (ptid_get_pid (inferior_ptid));
}

/* Close the RTOS plug-in target.  */
static void
rtos_client_thread_close (int quitting)
{
  unpush_target (&rtos_thread_ops);

  printf_filtered ("RTOS thread debugging disabled for:\n");
  printf_filtered ("\t%s\n", rtos_getinfo ());
}

/* Convert gdb register numbering to rtos register numbering.  */
static int
rtos_register_rtos_regno (struct gdbarch *gdbarch, int regno)
{
  if (gdbarch_register_rtos_regno_p (gdbarch))
    return gdbarch_register_rtos_regno (gdbarch, regno);
  else
    return regno;
}

/* Fetch task-qualified registers.  */
static void
rtos_client_fetch_registers (struct target_ops *ops,
			     struct regcache *regcache, int regno)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  char rbuf[RTOS_MAX_REGSIZE];
  gdb_byte buf[MAX_REGISTER_SIZE];
  LONGEST reg;
  int regsize;

  /* If regno is -1, read all registers.  */
  if (regno == -1)
    {
      for (regno = 0; regno < gdbarch_num_regs (gdbarch); regno++)
	rtos_client_fetch_registers (ops, regcache, regno);
      return;
    }

  /* For the current thread and/or global thread, return the raw
     machine registers.  */
  if (rtos_is_global_tid (ptid_get_pid (inferior_ptid))
      || ptid_get_pid (inferior_ptid) == (*rtos_get_current_tid) ())
    {
      raw_fetch_registers (ops, regcache, regno);
      return;
    }

  /* Read the value of the specific register through the plug-in.  */
  regsize = rtos_read_register (ptid_get_pid (inferior_ptid),
				rtos_register_rtos_regno (gdbarch, regno),
				rbuf, RTOS_MAX_REGSIZE);

  /* Return the raw machine registers if the plug-in failed.  */
  if (regsize < 0)
    {
      raw_fetch_registers (ops, regcache, regno);
      return;
    }

  reg = extract_signed_integer (rbuf, regsize, byte_order);
  if (regsize > MAX_REGISTER_SIZE)
    regsize = MAX_REGISTER_SIZE;
  store_signed_integer (buf, regsize, reg, byte_order);

  regcache_raw_supply (regcache, regno, buf);
}

/* Store task-qualified registers.  */
static void
rtos_client_store_registers (struct target_ops *ops,
			     struct regcache *regcache, int regno)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  char rbuf[RTOS_MAX_REGSIZE];
  LONGEST reg;
  int regsize;

  /* If regno is -1, write all registers.  */
  if (regno == -1)
    {
      for (regno = 0; regno < gdbarch_num_regs (gdbarch); regno++)
	rtos_client_store_registers (ops, regcache, regno);
      return;
    }

  /* For current and/or global thread, store raw machine registers.  */
  if (rtos_is_global_tid (ptid_get_pid (inferior_ptid))
      || ptid_get_pid (inferior_ptid) == rtos_get_current_tid ())
    {
      raw_store_registers (ops, regcache, regno);
      return;
    }

  regsize = register_size (gdbarch, regno);
  if (regsize > RTOS_MAX_REGSIZE)
    regsize = RTOS_MAX_REGSIZE;

  regcache_cooked_read_signed (regcache, regno, &reg);
  store_signed_integer (rbuf, regsize, reg, byte_order);

  /* Call the plug-in to write task-qualified registers.  */
  regsize = rtos_write_register (ptid_get_pid (inferior_ptid),
				 rtos_register_rtos_regno (gdbarch, regno),
				 rbuf, regsize);

  /* Resort to raw write if failed.  */
  if (regsize < 0)
    {
      raw_store_registers (ops, regcache, regno);
      return;
    }
}

/* Wait for target RTOS event.  */
static ptid_t
rtos_client_wait (struct target_ops *ops,
		  ptid_t ptid, struct target_waitstatus *status, int options)
{
  /* Call raw wait to stop on all events.  */
  ptid = raw_wait (ops, ptid, status, options);

  /* Find and set the current pid.  */
  inferior_ptid = pid_to_ptid (rtos_find_active_tid ());
  rtos_set_current_tid (ptid_get_pid (inferior_ptid));
  return (inferior_ptid);
}

/* Retrieve the list of threads on the target.  */
static void
rtos_client_threads_info (struct target_ops *ops)
{
  /* Get all objects of type "threads".  */
  rtos_thread_id_t tid = rtos_get_all_object_ids ("threads", 0);

  while (tid != -1)
    {
      /* Add if not already on thread list.  */
      if (!in_thread_list (pid_to_ptid (tid)))
	add_thread (pid_to_ptid (tid));

      /* Get next thread object.  */
      tid = rtos_get_all_object_ids ("threads", 1);
    }

  /* Find and set the current pid.  */
  inferior_ptid = pid_to_ptid (rtos_find_active_tid ());
  rtos_set_current_tid (ptid_get_pid (inferior_ptid));
}

/* Read target memory and return the number of units read.
   unit_size is the size of each unit read in byte.
   unit_count is the count of units read.  */
int
rtos_client_read_memory (rtos_addr memaddr, char *myaddr, int unit_size,
			 int unit_count)
{
  enum bfd_endian byte_order = gdbarch_byte_order (target_gdbarch);
  int len, i;
  char *s, *d;		/* s=source,d=dest for extraction */

  if (unit_size > sizeof (ULONGEST))
    error ("Can't handle unit_size more than %d bytes",
	   (int) sizeof (ULONGEST));

  if (!scratchbuf || scratchbuf_len < (unit_size * unit_count))
    {
      if (scratchbuf)
	xfree (scratchbuf);
      scratchbuf = xmalloc (scratchbuf_len = unit_size * unit_count);
    }

  len = target_read_memory (memaddr, scratchbuf, unit_size * unit_count);

  for (i = 0, s = scratchbuf, d = myaddr;
       i < unit_count;
       i++, s += unit_size, d += unit_size)
    {
      int j;
      ULONGEST value = extract_unsigned_integer (s, unit_size, byte_order);
      for (j = 0; j < unit_size; j++)
	d[j] = (char) ((value >> j * 8) & 0xff);
    }

  return len / unit_size;
}

/* Write target memory and return the number of units written.
   unit_size is the size of each unit to write in byte.
   unit_count is the count of units to write.  */
int
rtos_client_write_memory (rtos_addr memaddr, char *myaddr, int unit_size,
			  int unit_count)
{
  enum bfd_endian byte_order = gdbarch_byte_order (target_gdbarch);
  int len, i;
  char *s, *d;		/* s=source,d=dest for extraction */

  if (unit_size > sizeof (ULONGEST))
    error ("Can't handle unit_size more than %d bytes",
	   (int) sizeof (ULONGEST));

  if (!scratchbuf || scratchbuf_len < (unit_size * unit_count))
    {
      if (scratchbuf)
	xfree (scratchbuf);
      scratchbuf = xmalloc (scratchbuf_len = unit_size * unit_count);
    }

  for (i = 0, d = scratchbuf, s = myaddr;
       i < unit_count;
       i++, s += unit_size, d += unit_size)
    {
      store_unsigned_integer (d, unit_size, (ULONGEST) (*s), byte_order);
    }

  len = target_write_memory (memaddr, scratchbuf, unit_size * unit_count);

  return len / unit_size;
}

/* Look-up and return the address of the given symbol.  */
rtos_addr
rtos_client_lookup_symbol (const char *varname)
{
  struct minimal_symbol *ms;
  ms = lookup_minimal_symbol (varname, NULL, NULL);
  if (!ms)
    return 0;
  return SYMBOL_VALUE_ADDRESS (ms);
}

/* Return the offset of a given field within the given struct.  */
int
rtos_client_lookup_field_offset (const char *structname, const char *fieldname)
{
  char *exp;
  struct expression *expr;
  struct value *value;
  struct cleanup *old_chain = 0;
  int offset;
  struct type *type;

  exp = xstrprintf ("&(((struct %s *)0)->%s)", structname, fieldname);
  expr = parse_expression (exp);
  old_chain = make_cleanup (free_current_contents, &expr);
  value = evaluate_expression (expr);
  type = check_typedef (value_type (value));
  offset = unpack_long (type, value_contents (value));
  do_cleanups (old_chain);
  return offset;
}

/* Return 1 if the specified ptid on the target is alive.  */
int
rtos_client_thread_alive (struct target_ops *ops, ptid_t ptid)
{
  return rtos_valid_thread (ptid_get_pid (ptid));
}

/* Convert the given ptid to a printable string.  */
char *
rtos_client_pid_to_str (struct target_ops *ops, ptid_t ptid)
{
  return rtos_get_thread_name (ptid_get_pid (ptid));
}

/* Return extra information for the given thread.  */
static char *
rtos_client_extra_thread_info (struct thread_info *tp)
{
  rtos_object_id_t id = (rtos_object_id_t) (ptid_get_pid (tp->ptid));
  return rtos_get_extra_object_info ("threads", id);
}

/* Initialize target RTOS ops.  */
static void
init_rtos_thread_ops (void *plugin_handle)
{
  rtos_thread_ops.to_shortname = rtos_getname ();
  rtos_thread_ops.to_longname = rtos_getinfo ();
  rtos_thread_ops.to_doc = rtos_getinfo ();
  rtos_thread_ops.to_open = rtos_client_thread_open;
  rtos_thread_ops.to_close = rtos_client_thread_close;
  /* Read/write registers are optional.  If specified, use the rtos_client_xxx
     stub to call the plug-in read/write methods.  */
  if (rtos_read_register)
    {
      raw_fetch_registers = current_target.to_fetch_registers;
      rtos_thread_ops.to_fetch_registers = rtos_client_fetch_registers;
    }
  if (rtos_write_register)
    {
      raw_store_registers = current_target.to_store_registers;
      rtos_thread_ops.to_store_registers = rtos_client_store_registers;
    }
  raw_wait = current_target.to_wait;
  rtos_thread_ops.to_wait = rtos_client_wait;
  rtos_thread_ops.to_find_new_threads = rtos_client_threads_info;
  rtos_thread_ops.to_thread_alive = rtos_client_thread_alive;
  rtos_thread_ops.to_pid_to_str = rtos_client_pid_to_str;
  rtos_thread_ops.to_extra_thread_info = rtos_client_extra_thread_info;
  rtos_thread_ops.to_stratum = thread_stratum;
  rtos_thread_ops.to_magic = OPS_MAGIC;
}

/* Get information specific to the RTOS.  */
void
rtos_client_info_command (char *args, int from_tty)
{
  char *subcmd = strtok (args, " \t");
  char *subcmdargs = subcmd ? strtok (NULL, " \t") : 0;

  /* If command is "info <object type> schema", then return the schema for
     the specified object type.  */
  if (rtos_get_object_schema && subcmdargs
      && strcmp (subcmdargs, "schema") == 0)
    {
      if (strcmp (subcmd, "objects") == 0)
	printf_filtered ("type=!20,count=<4\n");
      else
	printf_filtered ("%s\n", rtos_get_object_schema (subcmd));
      return;
    }

  if (rtos_get_stack_info && strcmp (subcmd, "stackinfo") == 0)
    {
      rtos_thread_id_t tid = ptid_get_pid (inferior_ptid);
      printf_filtered ("%s\n", rtos_get_stack_info (tid));
    }

  else if (strcmp (subcmd, "objects") == 0)
    {
      int instance_count;

      /* Get the first type name.  */
      const char *object_type = rtos_get_all_object_types (0);

      /* ... and continue until done.  */
      while (object_type != NULL)
	{
	  /* For each object, get its current instance count.  */
	  instance_count = rtos_get_object_instance_count (object_type);
	  printf_filtered ("type=%s, count=%4d\n",
			   object_type, instance_count);
	  object_type = rtos_get_all_object_types (1);
	}
    }

  else
    {
      const char *object_type = subcmd;
      /* Get the instance count.  If -1, this object type doesn't exist.  */
      int instance_count = rtos_get_object_instance_count (object_type);
      rtos_object_id_t oid;

      if (instance_count < 0)
	error ("\"%s\" plugin does not recognize \"%s\" object and/or command",
	       rtos_getname(), subcmd);

      /* Get all ids of the specified type.  */
      oid = rtos_get_all_object_ids (object_type, 0);

      /* oid of 0 means there is no object of this type (although the type
	 exists.  */
      if (oid == 0)
	return;

      /* ... and continue until done.  */
      while (oid != (rtos_object_id_t) (-1) && oid != 0)
	{
	  printf_filtered ("%s\n",
			   rtos_get_extra_object_info (object_type, oid));
	  oid = rtos_get_all_object_ids (object_type, 1);
	}
    }
}

/* Load the RTOS-plugin extension for various RTOSes.  */
static void
load_plugins_command (char *args, int from_tty)
{
  rtos_lib_t plugin_handle;

  if (plugin_loaded)
    error ("Plugin already loaded.");

  plugin_handle = rtos_dlopen (args);
  if (!plugin_handle)
    error ("Unable to load plugin extension \"%s\".  Error: %s\n",
	   args, rtos_dlerror ());

  /* Resolve required symbols.  */
  rtos_errno =
    rtos_required_sym (plugin_handle, "rtos_errno");
  rtos_initialize =
    rtos_required_sym (plugin_handle, "rtos_initialize");
  rtos_getname =
    rtos_required_sym (plugin_handle, "rtos_getname");
  rtos_getinfo =
    rtos_required_sym (plugin_handle, "rtos_getinfo");
  rtos_is_global_tid =
    rtos_required_sym (plugin_handle, "rtos_is_global_tid");
  rtos_get_current_tid =
    rtos_required_sym (plugin_handle, "rtos_get_current_tid");
  rtos_set_current_tid =
    rtos_required_sym (plugin_handle, "rtos_set_current_tid");
  rtos_find_active_tid =
    rtos_required_sym (plugin_handle, "rtos_find_active_tid");
  rtos_valid_thread =
    rtos_required_sym (plugin_handle, "rtos_valid_thread");
  rtos_get_all_object_types =
    rtos_required_sym (plugin_handle, "rtos_get_all_object_types");
  rtos_get_all_object_ids =
    rtos_required_sym (plugin_handle, "rtos_get_all_object_ids");
  rtos_get_object_instance_count =
    rtos_required_sym (plugin_handle, "rtos_get_object_instance_count");
  rtos_get_object_info =
    rtos_required_sym (plugin_handle, "rtos_get_object_info");
  rtos_get_extra_object_info =
    rtos_required_sym (plugin_handle, "rtos_get_extra_object_info");
  rtos_get_thread_name =
    rtos_required_sym (plugin_handle, "rtos_get_thread_name");

  /* Resolve optional symbols.  */
  rtos_read_register =
    rtos_optional_sym (plugin_handle, "rtos_read_register");
  rtos_write_register =
    rtos_optional_sym (plugin_handle, "rtos_write_register");
  rtos_get_stack_info =
    rtos_optional_sym (plugin_handle, "rtos_get_stack_info");
  rtos_get_object_schema =
    rtos_optional_sym (plugin_handle, "rtos_get_object_schema");

  /* Initialize the plug-in.  */
  rtos_target_raw_ops.oldest_rev = RTOS_OLDEST_REVISION;
  rtos_target_raw_ops.newest_rev = RTOS_CURRENT_REVISION;
  rtos_target_raw_ops.raw_read_memory = rtos_client_read_memory;
  rtos_target_raw_ops.raw_write_memory = rtos_client_write_memory;
  rtos_target_raw_ops.raw_lookup_symbol = rtos_client_lookup_symbol;
  rtos_target_raw_ops.raw_lookup_field_offset =
    rtos_client_lookup_field_offset;
  rtos_initialize (&rtos_target_raw_ops);

  /* Initialize RTOS thread ops.  */
  init_rtos_thread_ops (plugin_handle);
  add_target (&rtos_thread_ops);

  /* Add commands.  */
  add_info (rtos_getname(), rtos_client_info_command,
	    "Show information specific to the given RTOS");

  /* Mark the plugin loaded.  */
  plugin_loaded = 1;
}


/* Startup initialization function, automagically called by init.c.  */
void
_initialize_rtos_thread (void)
{
  add_cmd ("rtos-load-plugins", class_obscure, load_plugins_command,
	   "Load gdb plug-in extensions", &cmdlist);
}

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

#ifndef RTOS_THREAD_H
#define RTOS_THREAD_H

#ifdef RTOS_CLIENT
#define RTOS_FUNC_DECL(rtype, fname, ...) \
  static rtype (*fname) (__VA_ARGS__) = 0
/* The global error number.  */
int *rtos_errno;
#endif

#ifdef RTOS_SERVER
#define RTOS_FUNC_DECL(rtype, fname, ...) \
  extern rtype fname (__VA_ARGS__)
/* The global error number.  */
extern int rtos_errno;
#endif

#define RTOS_MAX_REGSIZE			8

/* Common types.  */
typedef long long rtos_addr;
typedef unsigned long rtos_reg_t;
typedef unsigned long rtos_thread_id_t;
typedef unsigned long rtos_object_id_t;
typedef unsigned long rtos_debug_version_t;

#define RTOS_CURRENT_REVISION			10
#define RTOS_OLDEST_REVISION			10

typedef struct
{
  rtos_debug_version_t oldest_rev;
  rtos_debug_version_t newest_rev;
  int (*raw_read_memory) (rtos_addr memaddr, char *myaddr, int unit_size,
			  int unit_count);
  int (*raw_write_memory) (rtos_addr memaddr, char *myaddr, int unit_size,
			   int unit_count);
  rtos_addr (*raw_lookup_symbol) (const char *varname);
  int (*raw_lookup_field_offset) (const char *structname,
				  const char *fieldname);
} raw_target_ops_t;

/* Required symbols from each RTOS plugin.  */

/* Initialize the plug-in.  The operations passed in "raw_ops" should come
   from a remote target with connectivity to the inferior target.  */
RTOS_FUNC_DECL (void, rtos_initialize, raw_target_ops_t * raw_ops);

/* Get name of the plugin (e.g. "threadx", "integrity", "linux", etc...).  */
RTOS_FUNC_DECL (char *, rtos_getname, void);

/* Get a one-liner description of the plugin. (e.g. "Threadx 4.0 on...").  */
RTOS_FUNC_DECL (char *, rtos_getinfo, void);

/* Return 1 if this specified thread ID is the global thread.  */
RTOS_FUNC_DECL (int, rtos_is_global_tid, rtos_thread_id_t tid);

/* Return TID of thread currently running on the target.  */
RTOS_FUNC_DECL (rtos_thread_id_t, rtos_get_current_tid, void);

/* Set TID of thread currently running on the target.  */
RTOS_FUNC_DECL (void, rtos_set_current_tid, rtos_thread_id_t ctid);

/* Find/return the TID of the current active thread.  */
RTOS_FUNC_DECL (rtos_thread_id_t, rtos_find_active_tid, void);

/* Return 1 if thread the given thread id is valid.  */
RTOS_FUNC_DECL (int, rtos_valid_thread, rtos_thread_id_t tid);

/* Return the name of the given thread.  */
RTOS_FUNC_DECL (char *, rtos_get_thread_name, rtos_thread_id_t tid);

/* Note: The following query functions (which take a "next" argument)
   return the first object querried if "next" is 0.  For non-zero "next",
   these functions return the object considered to be next to the
   previously (and most recently) queried object.  */

/* Return the type name(s) of all RTOS objects.  */
RTOS_FUNC_DECL (const char *, rtos_get_all_object_types, int next);

/* Return all object id(s) of the given object type.  */
RTOS_FUNC_DECL (rtos_object_id_t, rtos_get_all_object_ids,
		const char *object_type_name, int next);

/* Return the instance count for the given object type *.  */
RTOS_FUNC_DECL (int, rtos_get_object_instance_count,
		const char *object_type_name);

/* Return brief info for the given object id.  */
RTOS_FUNC_DECL (const char *, rtos_get_object_info,
		const char *object_type_name, rtos_object_id_t oid);

/* Return extra info for the given object id.  */
RTOS_FUNC_DECL (char *, rtos_get_extra_object_info,
		const char *object_type_name, rtos_object_id_t oid);

/* Return the schema of the given object type.  */
RTOS_FUNC_DECL (const char *, rtos_get_object_schema,
		const char *object_type_name);

/* Optional symbols from each RTOS plugin.  */

/* Read (in target byte order) the value of the target register specified in
   regno into buf and return the number of bytes read.  */
RTOS_FUNC_DECL (int, rtos_read_register, rtos_thread_id_t tid, int regno,
		char *buf, unsigned int max_buf_size);

/* Write the value of the target register for which the register number
   specified in regno, and the value is contained (in target byte order)
   in buf.  buf_size is the length in bytes of the register.  */
RTOS_FUNC_DECL (int, rtos_write_register, rtos_thread_id_t tid, int regno,
		char *buf, unsigned int buf_size);

RTOS_FUNC_DECL (const char *, rtos_get_stack_info, rtos_thread_id_t tid);

#endif

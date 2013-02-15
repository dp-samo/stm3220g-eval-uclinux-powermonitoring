#ifndef _BITS_SYSCALLS_H
#define _BITS_SYSCALLS_H
#ifndef _SYSCALL_H
# error "Never use <bits/syscalls.h> directly; include <sys/syscall.h> instead."
#endif

/* m68k headers does stupid stuff with __NR_iopl / __NR_vm86:
 * #define __NR_iopl   not supported
 * #define __NR_vm86   not supported
 */
#undef __NR_iopl
#undef __NR_vm86

#ifndef __ASSEMBLER__

#include <errno.h>

/* Linux takes system call arguments in registers:

	syscall number	%d0	     call-clobbered
	arg 1		%d1	     call-clobbered
	arg 2		%d2	     call-saved
	arg 3		%d3	     call-saved
	arg 4		%d4	     call-saved
	arg 5		%d5	     call-saved

   The stack layout upon entering the function is:

	20(%sp)		Arg# 5
	16(%sp)		Arg# 4
	12(%sp)		Arg# 3
	 8(%sp)		Arg# 2
	 4(%sp)		Arg# 1
	  (%sp)		Return address

   (Of course a function with say 3 arguments does not have entries for
   arguments 4 and 5.)

   Separate move's are faster than movem, but need more space.  Since
   speed is more important, we don't use movem.  Since %a0 and %a1 are
   scratch registers, we can use them for saving as well.  */

#define _syscall0(type,name) \
type name(void) { \
  return (type) INLINE_SYSCALL(name, 0); \
}

#define _syscall1(type,name,type1,arg1) \
type name(type1 arg1) { \
  return (type) INLINE_SYSCALL(name, 1, arg1); \
}

#define _syscall2(type,name,type1,arg1,type2,arg2) \
type name(type1 arg1, type2 arg2) { \
  return (type) INLINE_SYSCALL(name, 2, arg1, arg2); \
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3) \
type name(type1 arg1, type2 arg2, type3 arg3) { \
  return (type) INLINE_SYSCALL(name, 3, arg1, arg2, arg3); \
}

#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4) \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) { \
  return (type) INLINE_SYSCALL(name, 4, arg1, arg2, arg3, arg4); \
}

#define _syscall5(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5) \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5) { \
  return (type) INLINE_SYSCALL(name, 5, arg1, arg2, arg3, arg4, arg5); \
}

#define _syscall6(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5,type6,arg6) \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, type6 arg6) { \
  return (type) INLINE_SYSCALL(name, 6, arg1, arg2, arg3, arg4, arg5, arg6); \
}

/* Blatanly copied from glibc/ports/sysdep/unix/sysv/linux/m68k/sysdep.h:  */

/* Define a macro which expands into the inline wrapper code for a system
   call.  */
#define INLINE_SYSCALL(name, nr, args...)				\
  ({ unsigned int _sys_result = INTERNAL_SYSCALL (name, , nr, args);	\
     if (__builtin_expect (INTERNAL_SYSCALL_ERROR_P (_sys_result, ), 0))\
       {								\
	 __set_errno (INTERNAL_SYSCALL_ERRNO (_sys_result, ));		\
	 _sys_result = (unsigned int) -1;				\
       }								\
     (int) _sys_result; })

#undef INTERNAL_SYSCALL_DECL
#define INTERNAL_SYSCALL_DECL(err) do { } while (0)

/* Define a macro which expands inline into the wrapper code for a system
   call.  This use is for internal calls that do not need to handle errors
   normally.  It will never touch errno.  This returns just what the kernel
   gave back.  */
#define INTERNAL_SYSCALL_NCS(name, err, nr, args...)	\
  ({ unsigned int _sys_result;				\
     {							\
       /* Load argument values in temporary variables
	  to perform side effects like function calls
	  before the call used registers are set.  */	\
       LOAD_ARGS_##nr (args)				\
       LOAD_REGS_##nr					\
       register int _d0 __asm__ ("%d0") = name;		\
       __asm__ __volatile__ ("trap #0"				\
		     : "=d" (_d0)			\
		     : "0" (_d0) ASM_ARGS_##nr		\
		     : "memory");			\
       _sys_result = _d0;				\
     }							\
     (int) _sys_result; })
#define INTERNAL_SYSCALL(name, err, nr, args...)	\
  INTERNAL_SYSCALL_NCS (__NR_##name, err, nr, ##args)

#define INTERNAL_SYSCALL_ERROR_P(val, err)		\
  ((unsigned int) (val) >= -4095U)

#define INTERNAL_SYSCALL_ERRNO(val, err)	(-(val))

#define LOAD_ARGS_0()
#define LOAD_REGS_0
#define ASM_ARGS_0
#define LOAD_ARGS_1(a1)				\
  LOAD_ARGS_0 ()				\
  int __arg1 = (int) (a1);
#define LOAD_REGS_1				\
  register int _d1 __asm__ ("d1") = __arg1;		\
  LOAD_REGS_0
#define ASM_ARGS_1	ASM_ARGS_0, "d" (_d1)
#define LOAD_ARGS_2(a1, a2)			\
  LOAD_ARGS_1 (a1)				\
  int __arg2 = (int) (a2);
#define LOAD_REGS_2				\
  register int _d2 __asm__ ("d2") = __arg2;		\
  LOAD_REGS_1
#define ASM_ARGS_2	ASM_ARGS_1, "d" (_d2)
#define LOAD_ARGS_3(a1, a2, a3)			\
  LOAD_ARGS_2 (a1, a2)				\
  int __arg3 = (int) (a3);
#define LOAD_REGS_3				\
  register int _d3 __asm__ ("d3") = __arg3;		\
  LOAD_REGS_2
#define ASM_ARGS_3	ASM_ARGS_2, "d" (_d3)
#define LOAD_ARGS_4(a1, a2, a3, a4)		\
  LOAD_ARGS_3 (a1, a2, a3)			\
  int __arg4 = (int) (a4);
#define LOAD_REGS_4				\
  register int _d4 __asm__ ("d4") = __arg4;		\
  LOAD_REGS_3
#define ASM_ARGS_4	ASM_ARGS_3, "d" (_d4)
#define LOAD_ARGS_5(a1, a2, a3, a4, a5)		\
  LOAD_ARGS_4 (a1, a2, a3, a4)			\
  int __arg5 = (int) (a5);
#define LOAD_REGS_5				\
  register int _d5 __asm__ ("d5") = __arg5;		\
  LOAD_REGS_4
#define ASM_ARGS_5	ASM_ARGS_4, "d" (_d5)
#define LOAD_ARGS_6(a1, a2, a3, a4, a5, a6)	\
  LOAD_ARGS_5 (a1, a2, a3, a4, a5)		\
  int __arg6 = (int) (a6);
#define LOAD_REGS_6				\
  register int _a0 __asm__ ("a0") = __arg6;		\
  LOAD_REGS_5
#define ASM_ARGS_6	ASM_ARGS_5, "a" (_a0)

#endif /* __ASSEMBLER__ */
#endif /* _BITS_SYSCALLS_H */

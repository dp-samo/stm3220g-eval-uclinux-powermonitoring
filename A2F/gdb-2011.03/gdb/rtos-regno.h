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

#ifndef RTOS_REGNO_H
#define RTOS_REGNO_H

/* The following are register numbering definitions for the rtos_read/write
   register method for different CPU architectures.  */

enum
{
  RTOS_MIPS32_REG_R0 = 0,
  RTOS_MIPS32_REG_R1 = 1,
  RTOS_MIPS32_REG_R2 = 2,
  RTOS_MIPS32_REG_R3 = 3,
  RTOS_MIPS32_REG_R4 = 4,
  RTOS_MIPS32_REG_R5 = 5,
  RTOS_MIPS32_REG_R6 = 6,
  RTOS_MIPS32_REG_R7 = 7,
  RTOS_MIPS32_REG_R8 = 8,
  RTOS_MIPS32_REG_R9 = 9,
  RTOS_MIPS32_REG_R10 = 10,
  RTOS_MIPS32_REG_R11 = 11,
  RTOS_MIPS32_REG_R12 = 12,
  RTOS_MIPS32_REG_R13 = 13,
  RTOS_MIPS32_REG_R14 = 14,
  RTOS_MIPS32_REG_R15 = 15,
  RTOS_MIPS32_REG_R16 = 16,
  RTOS_MIPS32_REG_R17 = 17,
  RTOS_MIPS32_REG_R18 = 18,
  RTOS_MIPS32_REG_R19 = 19,
  RTOS_MIPS32_REG_R20 = 20,
  RTOS_MIPS32_REG_R21 = 21,
  RTOS_MIPS32_REG_R22 = 22,
  RTOS_MIPS32_REG_R23 = 23,
  RTOS_MIPS32_REG_R24 = 24,
  RTOS_MIPS32_REG_R25 = 25,
  RTOS_MIPS32_REG_R26 = 26,
  RTOS_MIPS32_REG_R27 = 27,
  RTOS_MIPS32_REG_R28 = 28,
  RTOS_MIPS32_REG_R29 = 29,
  RTOS_MIPS32_REG_R30 = 30,
  RTOS_MIPS32_REG_R31 = 31,
  RTOS_MIPS32_REG_HI = 32,
  RTOS_MIPS32_REG_LO = 33,
  RTOS_MIPS32_REG_BAD = 34,
  RTOS_MIPS32_REG_SR = 35,
  RTOS_MIPS32_REG_CAUSE = 36,
  RTOS_MIPS32_REG_PC = 37,
  RTOS_MIPS32_REG_F0 = 100,
  RTOS_MIPS32_REG_F1 = 101,
  RTOS_MIPS32_REG_F2 = 102,
  RTOS_MIPS32_REG_F3 = 103,
  RTOS_MIPS32_REG_F4 = 104,
  RTOS_MIPS32_REG_F5 = 105,
  RTOS_MIPS32_REG_F6 = 106,
  RTOS_MIPS32_REG_F7 = 107,
  RTOS_MIPS32_REG_F8 = 108,
  RTOS_MIPS32_REG_F9 = 109,
  RTOS_MIPS32_REG_F10 = 110,
  RTOS_MIPS32_REG_F11 = 111,
  RTOS_MIPS32_REG_F12 = 112,
  RTOS_MIPS32_REG_F13 = 113,
  RTOS_MIPS32_REG_F14 = 114,
  RTOS_MIPS32_REG_F15 = 115,
  RTOS_MIPS32_REG_F16 = 116,
  RTOS_MIPS32_REG_F17 = 117,
  RTOS_MIPS32_REG_F18 = 118,
  RTOS_MIPS32_REG_F19 = 119,
  RTOS_MIPS32_REG_F20 = 120,
  RTOS_MIPS32_REG_F21 = 121,
  RTOS_MIPS32_REG_F22 = 122,
  RTOS_MIPS32_REG_F23 = 123,
  RTOS_MIPS32_REG_F24 = 124,
  RTOS_MIPS32_REG_F25 = 125,
  RTOS_MIPS32_REG_F26 = 126,
  RTOS_MIPS32_REG_F27 = 127,
  RTOS_MIPS32_REG_F28 = 128,
  RTOS_MIPS32_REG_F29 = 129,
  RTOS_MIPS32_REG_F30 = 130,
  RTOS_MIPS32_REG_F31 = 131,
  RTOS_MIPS32_REG_FSR = 132,
  RTOS_MIPS32_REG_FIR = 133,
  RTOS_MIPS32_REG_FP = 134
};

#endif

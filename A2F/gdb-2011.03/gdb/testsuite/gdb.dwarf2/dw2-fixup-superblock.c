/* This testcase is part of GDB, the GNU debugger.

   Copyright 2010 Free Software Foundation, Inc.

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

/* Force inlining, even at -O0.  This works with both RealView and
   GCC.  Since the test is covering a workaround for RealView, this is
   good enough.  Gcc coverage is good to check the workaround doesn't
   break debug info from compilers that don't need it.  */
#define INLINE __attribute__((always_inline))

static INLINE int INCREMENT2 (int _x2)
{
  return (_x2) + 1;
}

static INLINE int INCREMENT3 (int _x3)
{
  return (_x3) + 1;
}

static INLINE int INCREMENT (int _x)
{
  /* A lexical block within an inlineable subroutine, and a couple of
     inlined function call blocks below.  */
  {
    int xxx = _x;
    return INCREMENT2 (xxx) + INCREMENT3 (xxx);
  }
}

int TestFunction (void)
{
  int retVal; /* These variables are visible always.  */
  int counter;

  {
    int x; /* x and y would become unavailable at line "here 1" below.  */
    int y;

    x = 10;
    y = 5;
    counter = 1;
    retVal = 0;

    {
      /* A lexical block within a lexical block, and an inlined
	 function call block below.  */
      int z1;

      z1 = 3;                          /* set breakpoint here 1 */
      counter = INCREMENT (counter);
      retVal = z1 + x - y;
    }
  }

  {
    /* A lexical block within a subprogram, and an inlined function
       call block below.  */
    int z2;

    counter = 2;
    z2 = 9;                            /* set breakpoint here 2 */
    counter = INCREMENT (counter);
    retVal = z2 + counter;
  }

  return retVal;
}

int
main (int argc, char **argv)
{
  return TestFunction ();
}

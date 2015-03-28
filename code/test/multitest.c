/*
 * multitest.c
 *
 * Tests multiple programs
 */

#include "syscall.h"

int
main()
{
  Exec("../test/exittest",0,0,0);
  Exec("../test/array",0,0,0);

  Yield();
  Yield();
  Yield();
  Yield();
  Yield();
  Yield();
  Yield();
  Yield();
  Yield();

  // Several Yields to make sure previos processes ended
  // Now memory should be free for matmult
  Exec("../test/matmult",0,0,0);
  Exit(1);
}

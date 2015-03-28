/*
 * multioutofmemorytest.c
 *
 * Tests multiple programs, last one cannot be allocated
 */

#include "syscall.h"

int
main()
{
  Exec("../test/exittest",0,0,0);
  Exec("../test/array",0,0,0);
  Exec("../test/runOutOfMemoryTest",0,0,0);
  Exit(1);
}

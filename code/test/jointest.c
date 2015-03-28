/*
 * jointest.c
 *
 * Join a simple program.
 */

#include "syscall.h"

int
main()
{
  int result = Exec("../test/array", 0, 0, 1);
  Exec("../test/exittest", 0, 0, 0);
  int value = Join(result); //Should be 1128
  Exit(value);
}

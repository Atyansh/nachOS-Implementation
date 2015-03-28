/*
 * joinUnjoinable.c
 *
 * Join thread that cannot be joined
 */

#include "syscall.h"

int
main()
{
  int result = Exec("../test/exittest", 0, 0, 0); // Cannot join
  int value = Join(result);
  Exit(value);
}

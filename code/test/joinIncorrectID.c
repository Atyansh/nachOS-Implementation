/*
 * joinIncorrectID.c
 *
 * Join with Incorrect ID
 */

#include "syscall.h"

int
main()
{
  Exec("../test/exittest", 0, 0, 1);
  int value = Join(5);
  Exit(value);
}

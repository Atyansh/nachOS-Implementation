/*
 * execlongnametest.c
 *
 * Exec a simple program. Won't be able to because name is too long
 */

#include "syscall.h"

int
main()
{
  int result = 1000;
  result = Exec("../test/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 0, 0, 0);
  Exit(result);
}

/*
 * writetoolarge.c
 *
 * Try to read with a very large size
 */

#include <syscall.h>

int
main ()
{
  Write("AA\n", 10000, ConsoleOutput);
}

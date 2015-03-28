/*
 * notwritingtostdout.c
 *
 * Try to write to stdin, shouldn't work
 */

#include <syscall.h>

int
main ()
{
  Write("AA\n", 3, ConsoleInput);
}

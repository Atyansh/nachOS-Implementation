/*
 * invalidbuffertest.c
 *
 * Try to Read from stdin to an invalid buffer
 */

#include <syscall.h>

int
main ()
{
  char * buffer = 10000;
  Read(buffer, 3, ConsoleInput);
}

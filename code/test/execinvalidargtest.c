/*
 * execinvalidargtest.c
 *
 * Used to call argtest using exec
 */

#include "syscall.h"

int
main()
{
  char ** argv;
  argv[0] = "Hellooooooooooooooooooooooooooooooooooooooooooooooooo\0";
  argv[1] = "World\0";
  Exec("../test/argtest", 2, argv, 0);
}

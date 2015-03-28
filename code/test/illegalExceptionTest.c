/* 
 * illegalExceptionTest.c
 *
 * Simple program to test for IllegalInstrException.
 */

#include "syscall.h"

int
main()
{
  int (* a) () = 0;
  a();
}

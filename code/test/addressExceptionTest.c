/* 
 * addressExceptionTest.c
 *
 * Simple program to test AddressErrorException.
 */

#include "syscall.h"

int
main()
{
  int * a = 0xdeadbead;
  *a = 1;
}

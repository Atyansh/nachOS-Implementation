/*
 * runOutOfMemoryTest.c
 *
 * Tests multiple programs
 */

#include "syscall.h"

#define Dim 1000	/* sum total of the arrays doesn't fit in */

int A[Dim][Dim];
int B[Dim][Dim];
int C[Dim][Dim];

int
main()
{
  Exit(1);
}

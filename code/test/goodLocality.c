#define Dim  100

#include "syscall.h"
int A[Dim];

int
main()
{
    int i;
    int j;
    for( j=0;j<Dim;j++){

      for (i = 8; i < 10; i++)		/* first initialize the matrices */
      {
          A[i]=i;
      }
    }

    Exit(1);
}


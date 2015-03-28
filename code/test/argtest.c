/*
 * argtest.c
 *
 * Simple program that takes in arguments and prints them out
 */

#include "syscall.h"

int
main(int argc, char ** argv)
{
  int i;
  int j;

  Write("Arguments:\n",11,ConsoleOutput);

  for(i = 0; i < argc; i++) {
    j = 0;
    while(argv[i][j] != '\0') {
      j++;
    }
    Write(argv[i], j, ConsoleOutput);
    Write("\n", 1, ConsoleOutput);
  }
  
  Exit(argc);
}

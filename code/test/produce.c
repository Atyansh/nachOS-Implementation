/*
 * produce.c
 *
 * Produces output that gets piped to another process
 */

#include "syscall.h"

int
main()
{
  char buffer[100];

  int i = 0;

  while(1) {

    i = 0;
    do {
      Read(&buffer[i],1, ConsoleInput);
    } while (buffer[i++] != '\n');

    buffer[i] = '\0';

    Write(buffer, i, ConsoleOutput);
    
    if (buffer[0] == '.' && buffer[1] == '\n') {
      break;
    }
  }

  Exit(2000);
}

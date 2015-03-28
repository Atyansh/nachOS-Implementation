/*
 * consume.c
 *
 * Consumes input that gets piped from another process
 * Performs a Caesar cipher of 1 on the input received
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
      if(buffer[i] == 'Z') {
        buffer[i] = 'A';
      }
      else if(buffer[i] == 'z') {
        buffer[i] = 'a';
      }
      else if((buffer[i] >= 'A' && buffer[i] < 'Z') ||
              (buffer[i] >= 'a' && buffer[i] < 'z')) {
        buffer[i]++;
      }
    } while (buffer[i++] != '\n');

    buffer[i] = '\0';

    Write(buffer, i, ConsoleOutput);

    if (buffer[0] == '.' && buffer[1] == '\n') {
      break;
    }
  }

  Exit(1000);
}

/* 
 * pipetest.c
 *
 *	Simple program to test pipe system call.  Note that
 * you will have to write "produce" and "consume".  Produce writes
 * output and consume reads it.
 */

#include <syscall.h>

int
main()
{
    Exec("../test/produce", 0, 0, 2);
    Exec("../test/consume", 0, 0, 6);
    Exec("../test/consume", 0, 0, 6);
    Exec("../test/consume", 0, 0, 4);

    Exit(500);
}

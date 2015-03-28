#ifndef PIPE_H
#define PIPE_H

#include "synch.h"


/*
 * Pipe
 * 
 * This class is used to create a pipe which isused to communicate between
 * processes
 */
class Pipe {

  public:

    Pipe(int bufferSize);         // Create a Pipe of capacity buffer.

    ~Pipe();                      // Pipe destructor

    int Read();                   // Read from pipe

    int Write(char ch);           // Write to pipe

    void setInOpen(int value);    // Setter for inOpen
    void setOutOpen(int value);   // Setter for outOpen

    int getInOpen();              // Getter for InOpen
    int getOutOpen();             // Getter for outOpen

  private:
    // Lock and CV to manage syynchronization
    Lock * bufferLock;
    Condition * bufferCV;

    char * buffer;                // buffer that pipe uses

    int capacity;                 // Capacity for the pipe
    int size;                     // Size of pipe

    int start;                    // Index where reading from
    int end;                      // Index where writing to

    int inOpen;                   // Check if input side is connected or not
    int outOpen;                  // Check if output side is connected or not
};

#endif /* PIPE_H */

#include "SynchConsole.h"
#include "synch.h"


/*
 * SynchReadAvailable - Handler when reading
 */
static void SynchReadAvail(int arg) {
  SynchConsole * console = (SynchConsole *) arg;

  console->CheckCharAvail();
}

/*
 * SynchWriting - Handler when writing
 */
static void SynchWriteDone(int arg) {
  SynchConsole * console = (SynchConsole *) arg;

  console->WriteDone();
}


// initialize the hardware console device
SynchConsole::SynchConsole(char *readFile, char *writeFile) {
  r = new Semaphore("r",0);
  w = new Semaphore("w",0);
  console = new Console(readFile, writeFile, SynchReadAvail, SynchWriteDone,
      (int) this);
}


// clean up console emulation
SynchConsole::~SynchConsole() {
  delete console;
  delete r;
  delete w;
}  


// external interface -- Nachos kernel code can call these

/*
 * Synchronized PutChar method that waits
 */
void SynchConsole::PutChar(char ch) {
  console->PutChar(ch);
  w->P();
}  

/*
 * Synchronized GetChar method that waits
 */
char SynchConsole::GetChar() {
  r->P();
  char ch = console->GetChar();
  return ch;
}


// internal routine to signal I/O completion
void SynchConsole::WriteDone() {
  w->V();
}


// internal routine to signal I/O completion
void SynchConsole::CheckCharAvail() {
  r->V();
}

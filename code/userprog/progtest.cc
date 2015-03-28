// progtest.cc
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"
#include "syscall.h"
#include "Table.h"
#include "SynchConsole.h"

int MAXPROCESS = 14;

MemoryManager * memoryManager;

Table * TablePtr;
Table * storeTable;

SynchConsole * synchConsole;

CoreMap * coreMap;


//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void StartProcess(char *filename) {
  OpenFile *executable = fileSystem->Open(filename);
  AddrSpace *space;

  memoryManager = new MemoryManager(NumPhysPages);
  TablePtr = new Table(MAXPROCESS);
  storeTable = new Table(MAXPROCESS);
  synchConsole = new SynchConsole(NULL, NULL);
  
  coreMap = new CoreMap(NumPhysPages);

  if (executable == NULL) {
    printf("Unable to open file %s\n", filename);
    return;
  }

  BackingStore * backingStore = new BackingStore();

  int storeID = storeTable->Alloc((void *) backingStore);

  if(storeID == -1) {
    fprintf(stderr, "Process limit reached, cannot allocate backing store\n");
    return;
  }

  space = new AddrSpace(executable);
  currentThread->space = space;

  if(space->Initialize(executable) != 0) {
    delete executable;			// close file
    delete memoryManager;
    delete TablePtr;
    storeTable->Release(storeID);
    delete backingStore;
    return;
  }

  if(backingStore->Initialize(space, storeID) != 0) {
    fprintf(stderr, "Couldn't Initialize Backing Store\n");
    return;
  }

  space->setThreadCount(space->getThreadCount() + 1);

  space->InitRegisters();		// set the initial register values
  space->RestoreState();		// load page table register
  
  TablePtr->Alloc((void *) currentThread);

  machine->Run();			// jump to the user progam
  ASSERT(FALSE);			// machine->Run never returns;
  // the address space exits
  // by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static SynchConsole *console;

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void ConsoleTest (char *in, char *out) {
  char ch;

  console = new SynchConsole(in, out);

  for (;;) {
    ch = console->GetChar();
    console->PutChar(ch);	// echo it!
    if (ch == 'q') return;  // if q, quit
  }
}

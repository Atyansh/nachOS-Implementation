// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "Table.h"
#include "SynchConsole.h"
#include "Pipe.h"

// External variables used
extern Table * TablePtr;
extern Table * storeTable;
extern MemoryManager * memoryManager;

extern Statistics * stats;

extern SynchConsole * synchConsole;

extern CoreMap * coreMap;


// Table Class //

/*
 * Table's constructor
 */
Table::Table(int size){
  lock = new Lock("lock for the table in syscall");
  tableSize = size;
  tableptr = new void*[size];

  for(int i = 0; i < size; i++) {
    tableptr[i] = NULL;
  }
} 

/*
 * Alloc - Allocates void pointers to a table for later access
 */
int Table::Alloc(void * object){
  lock->Acquire();

  for(int i = 0; i < tableSize; i++) {
    if(tableptr[i] == NULL) {
      tableptr[i] = object;
      lock->Release();
      return i;
    }
  }

  lock->Release();

  return -1;
}

/*
 * Get - Gets a void pointer at a particular index in the table
 */
void * Table:: Get(int index) {
  lock->Acquire();

  if(index >= tableSize || index < 0 ) {
    lock->Release();
    return NULL;
  }

  void * toReturn = tableptr[index];

  lock->Release();

  return toReturn;
}

/*
 * Release - Removes a void pointer from the table at a particular index
 */
void Table::Release(int index) { 
  lock->Acquire();  

  if(index >= tableSize || index < 0 ) {
    lock->Release();
    return;
  }

  tableptr[index] = NULL;

  lock->Release();
}


// Pipe Class //


/*
 * Pipe Constructor
 */
Pipe::Pipe(int bufferSize) {
  capacity = bufferSize;
  size = start = end = inOpen = outOpen = 0;

  buffer = new char[capacity];

  bufferLock = new Lock("Pipe");
  bufferCV = new Condition("Pipe");
}


/*
 * Pipe Destructor
 */
Pipe::~Pipe() {
  delete buffer;
  delete bufferLock;
  delete bufferCV;
}


/*
 * Read -> Synchronously reads a character from the pipe
 */
int Pipe::Read() {
  bufferLock->Acquire();

  while(size == 0) {
    if(!getOutOpen()) {
      bufferLock->Release();
      return 1000000;
    }
    bufferCV->Wait(bufferLock);
  }

  size--;

  if(start == capacity) {
    start = 0;
  }

  int retVal = buffer[start];
  buffer[start++] = 0;

  bufferCV->Broadcast(bufferLock);
  bufferLock->Release();
  return retVal;
}


/*
 * Write -> Synchronously writes a character from the pipe
 */
int Pipe::Write(char ch) {
  bufferLock->Acquire();

  while(size == capacity) {
    if(!getInOpen()) {
      bufferLock->Release();
      return 0;
    }
    bufferCV->Wait(bufferLock);
  }

  buffer[end++] = ch;
  size++;

  if(end == capacity) {
    end = 0;
  }

  bufferCV->Broadcast(bufferLock);
  bufferLock->Release();

  return 1;
}


/*
 * setInOpen - Setter for inOpen
 */
void Pipe::setInOpen(int value) {
  inOpen = value;
}


/*
 * setOutOpen - Setter for outOpen
 */
void Pipe::setOutOpen(int value) {
  outOpen = value;
}


/*
 * getInOpen - Getter for inOpen
 */
int Pipe::getInOpen() {
  return inOpen;
}


/*
 * getOutOpen - Getter for outOpen
 */
int Pipe::getOutOpen() {
  return outOpen;
}

// Static pipe pointer used to hold a reference for next pipe to be connected
static Pipe * freePipe = 0;
static int PIPE_SIZE = 128;



// Syscall Routines //



/*
 * SysCallExit - Implementation of Exit Sys Call
 */
void SysCallExit(int status) {
  fprintf(stderr, "Exit called with status: %d\n", status);

  int pipeValue = currentThread->getPipeValue();

  if((pipeValue & 4) == 4) {
    Pipe * inPipe = currentThread->getInPipe();
    inPipe->setInOpen(0);
    if(!inPipe->getOutOpen()) {
      delete inPipe;
    }
  }
  if((pipeValue & 2) == 2) {
    Pipe * outPipe = currentThread->getOutPipe();
    outPipe->setOutOpen(0);
    if(!outPipe->getInOpen()) {
      delete outPipe;
    }
  }

  currentThread->setJoinValue(status);
  AddrSpace * space = currentThread->space;

  for(int i = 0; i < TABLESIZE; i++) {
    Thread * temp = (Thread *) (TablePtr->Get(i));


    if(temp && temp->space == space) {
      TablePtr->Release(i);
    }
  }
 
  space->setThreadCount(space->getThreadCount() - 1);

  //coreMap->evictAll(space->getBackingStore());

  if(space->getThreadCount() <= 0) {
    //delete space;
  }

  currentThread->Finish();
}


/*
 * Process - Forked process routine to start executing new process
 */
void ProcessStartExec(int numParam) { 
  currentThread->space->InitRegisters();		// set the initial register values
  currentThread->space->RestoreState();		// load page table register
  if(!currentThread->space->LoadArguments()) {
    fprintf(stderr, "Couldn't Load Arguments\n");
    fprintf(stderr, "Killing Process now\n");
    SysCallExit(-1);
  }

  machine->Run();			// jump to the user progam
  ASSERT(FALSE);			// machine->Run never returns;
  // the address space exits
  // by doing the syscall "exit"
}


/*
 * readArgument -> Helper function to translate arguments from virtual memory
 */
char * readArgument(char * str) {
  int MAX = 50;

  int i = 0;
  int ch = 1;

  char arr[MAX];

  for(i = 0; i < MAX; i++) {
    if(!(machine->ReadMem((int)(str+i), 1,&ch))) {
      fprintf(stderr, "Problem while translating\n");
      return 0;
    }

    arr[i] = (char) ch;

    if(arr[i] == '\0') {
      break;
    }
  }

  if(i == MAX && arr[i-1] != '\0') {
    fprintf(stderr, "Argument exceeded maximum length of 50\n");
    return 0;
  }
  char * ret = new char[i+1];

  for(int j = 0; j <= i; j++) {
    ret[j] = arr[j];
  }

  return ret;
}


/*
 * Exec2 - Syscall routine for exec
 */
SpaceId Exec2(char *name, int argc, char **argv, int willJoin) {
  fprintf(stderr, "Exec called\n");

  char * internalFilename = new char[100];

  int pipeValue = 0;

  int i;
  int ch = 0;

  // define macro for file size limit (100=)
  for(i = 0; i < MAXSIZE; i++) {
    if(!(machine->ReadMem((int)(name+i), 1,&ch)) &&
       !(machine->ReadMem((int)(name+i), 1,&ch))) {
      return 0;
    }

    internalFilename[i] = (char) ch;

    if(internalFilename[i] == '\0') {
      break;
    }
  }

  if(i == MAXSIZE && internalFilename[i - 1] != '\0') {
    fprintf(stderr, "File name exceeded maximum of length 100\n");
    return 0;
  }

  OpenFile * executable = fileSystem->Open(internalFilename);

  if (executable == NULL) {
    fprintf(stderr, "Unable to open file %s\n", internalFilename);
    return 0;
  }

  if((willJoin & 2) == 2 || (willJoin & 4) == 4 || (willJoin & 6) == 6) {
    pipeValue = willJoin;
  }

  Thread * newCurrentThread = new Thread("New Thread", (willJoin & 1));

  newCurrentThread->setPipeValue(pipeValue);

  if((pipeValue & 6) == 6) {
    newCurrentThread->setInPipe(freePipe);
    newCurrentThread->getInPipe()->setInOpen(1);

    freePipe = new Pipe(PIPE_SIZE);
    newCurrentThread->setOutPipe(freePipe);
    newCurrentThread->getOutPipe()->setOutOpen(1);
  }
  else if((pipeValue & 4) == 4) {
    if(!freePipe) {
      freePipe = new Pipe(PIPE_SIZE);
      newCurrentThread->setInPipe(freePipe);
      newCurrentThread->getInPipe()->setInOpen(1);
    }
    else {
      newCurrentThread->setInPipe(freePipe);
      newCurrentThread->getInPipe()->setInOpen(1);
      freePipe = 0;
    }
  }
  else if((pipeValue & 2) == 2) {
    if(!freePipe) {
      freePipe = new Pipe(PIPE_SIZE);
      newCurrentThread->setOutPipe(freePipe);
      newCurrentThread->getOutPipe()->setOutOpen(1);
    }
    else {
      newCurrentThread->setOutPipe(freePipe);
      newCurrentThread->getOutPipe()->setOutOpen(1);
      freePipe = 0;
    }
  }

  char ** arr = new char * [argc+1];
  arr[0] = internalFilename;

  for(i = 0; i < argc; i++) {
    // Reading 4 bytes because pointer is size 4 bytes
    if(!machine->ReadMem((int)(argv+i),4,&ch) &&
       !machine->ReadMem((int)(argv+i),4,&ch)) {
      return 0;
    }
    arr[i+1] = readArgument((char *) ch);
    if(arr[i+1] == 0) {
      fprintf(stderr, "Could not read argument properly\n");
      delete executable;
      delete [] arr;
      return 0;
    }
  }


  BackingStore * backingStore = new BackingStore();

  int storeID = storeTable->Alloc((void *) backingStore);

  if(storeID == -1) {
    fprintf(stderr, "Process limit reached, cannot allocate backing store\n");
    return 0;
  }


  AddrSpace * space = new AddrSpace(executable);
  
  space->AddArguments(argc+1,arr);

  newCurrentThread->space = space;

  if(space->Initialize(executable) != 0) {
    storeTable->Release(storeID);
    delete executable;			// close file
    delete internalFilename;
    delete space;
    delete newCurrentThread;

    fprintf(stderr, "Couldn't initialize new address space for new process\n");
    return 0;
  }

  ;
  if(backingStore->Initialize(space, storeID) != 0) {
    fprintf(stderr, "Couldn't Initialize Backing Store\n");
    delete backingStore;
    delete executable;
    delete internalFilename;
    delete space;
    return 0;
  }

 // delete executable;			// close file
  space->setThreadCount(space->getThreadCount()+1);

  newCurrentThread->Fork(ProcessStartExec, 0);

  int processID = TablePtr->Alloc(newCurrentThread);

  if(processID == -1) {
    fprintf(stderr, "ERROR. Couldn't allocate process in thread\n");
    delete backingStore;
    delete executable;
    delete internalFilename;
    delete space;
    return 0;
  }

  return ++processID;
}

static Lock * WriteLock = new Lock("Write Lock");
static Lock * ReadLock = new Lock("Read Lock");


/*
 * Read2 - Syscall routine for Read
 */
int Read2(char *buffer, int size, OpenFileId id) {
  if(id != 0) {
    fprintf(stderr, "Error: Not reading from console input\n");
    fprintf(stderr, "Returning with value -1\n");
    return -1;
  }

  if(((unsigned int) buffer)>=(currentThread->space->getNumPages()*PageSize)) {
    fprintf(stderr, "Invalid buffer address (Outside of virtual memory)\n");
    fprintf(stderr, "Returning with value -1\n");
    return -1;
  }

  int pipeValue = currentThread->getPipeValue();
  int count = 0;

  if((pipeValue & 4) != 4 && (pipeValue & 6) != 6) {
    ReadLock->Acquire();
  
    for(int i = 0; i < size; i++) {
      if(((unsigned int) (buffer + i)) >= 
         (currentThread->space->getNumPages()*PageSize)) {
        fprintf(stderr, "Tried to read beyond virtual memory\n");
        fprintf(stderr, "Prevented that\n");
        break;
      }
      count++;
      int character = (int) synchConsole->GetChar();

      if(!machine->WriteMem((int) (buffer+i), 1, character) &&
         !machine->WriteMem((int) (buffer+i), 1, character)) {
        return -1;
      }
    }
    ReadLock->Release();
  }
  else {
    for(int i = 0; i < size; i++) {
      if(((unsigned int) (buffer + i)) >= 
         (currentThread->space->getNumPages()*PageSize)) {
        fprintf(stderr, "Tried to read beyond virtual memory\n");
        fprintf(stderr, "Prevented that\n");
        break;
      }

      int ch = currentThread->getInPipe()->Read();
      if(ch != 1000000) {
        if(!machine->WriteMem((int) (buffer+i), 1, ch) && 
           !machine->WriteMem((int) (buffer+i), 1, ch)) {
          return -1;
        }
        count++;
      }
    }
  }
  return count;
}


/*
 * Write2 - Syscall routine for Write
 */
int Write2(char *buffer, int size, OpenFileId id) {
  if(id != 1) {
    fprintf(stderr, "Error: Not writing to console output\n");
    fprintf(stderr, "Returning with value -1\n");
    return -1;
  }

  if(((unsigned int) buffer)>=(currentThread->space->getNumPages()*PageSize)) {
    fprintf(stderr, "Invalid buffer address\n");
    fprintf(stderr, "Returning with value -1\n");
    return -1;
  }

  int pipeValue = currentThread->getPipeValue();
  int count = 0;

  if((pipeValue & 2) != 2 && (pipeValue & 6) != 6) {
    WriteLock->Acquire();
  
    int ch;
    int i = 0;

    for(i = 0; i < size; i++) {
      if(((unsigned int) (buffer + i)) >= 
         (currentThread->space->getNumPages()*PageSize)) {
        fprintf(stderr, "Tried to write beyond virtual memory\n");
        fprintf(stderr, "Prevented that\n");
        break;
      }

      count++;
      if(!machine->ReadMem((int) (buffer + i), 1, &ch) &&
         !machine->ReadMem((int) (buffer + i), 1, &ch)) {
        return -1;
      }
      synchConsole->PutChar((char) ch);
    }
    WriteLock->Release();
  }
  else {
    int ch;
    int i = 0;
    for(i = 0; i < size; i++) {
      if(((unsigned int) (buffer + i)) >= 
         (currentThread->space->getNumPages()*PageSize)) {
        fprintf(stderr, "Tried to write beyond virtual memory\n");
        fprintf(stderr, "Prevented that\n");
        break;
      }

      if(!machine->ReadMem((int) (buffer + i), 1, &ch) &&
         !machine->ReadMem((int) (buffer + i), 1, &ch)) {
        return -1;
      }
      if(currentThread->getOutPipe()->Write((char) ch)) {
        count++;
      }
    }
  }
  return count++;
}


/*
 * Join2 - Syscall routine for Join
 */
int Join2(SpaceId id) {

  Thread * joinThread = (Thread *) TablePtr->Get(id-1);

  if(joinThread == NULL) {
    fprintf(stderr, "Incorrect ID, cannot join\n");
    return -65535;
  }

  if(!joinThread->canJoin()) {
    fprintf(stderr, "Thread unjoinable, cannot join\n");
    return -65535;
  }

  joinThread->Join();

  int retVal = joinThread->getJoinValue();

  return retVal;
}


/*
 * ForkedThread -> Forked routine to run the forked thread
 */
void ForkedThread(int funcPtr) {
  currentThread->space->InitRegisters();		// set the initial register values
  currentThread->space->RestoreState();		// load page table register

  machine->WriteRegister(PCReg, funcPtr);
  machine->WriteRegister(NextPCReg, funcPtr+4);

  machine->Run();			// jump to the user progam
  ASSERT(FALSE);			// machine->Run never returns;
  // the address space exits
  // by doing the syscall "exit"
}


/*
 * Fork2 - Syscall routine for Fork
 */
void Fork2(void (*func)()) {
  int funcPtr = (int) (func);

  if(!currentThread->space->allocateThreadSpace()) {
    return;
  }

  currentThread->SaveUserState();
  currentThread->RestoreUserState();

  AddrSpace * space = currentThread->space;

  Thread * forkedThread = new Thread("Forked Thread", 1);
  forkedThread->space = space;
  
  TablePtr->Alloc((void *)(forkedThread));

  forkedThread->Fork(ForkedThread, funcPtr);
  space->setThreadCount(space->getThreadCount()+1);
}


/*
 * Yield2 - Syscall routine for Yield
 */
void Yield2() {
  currentThread->Yield();
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------
void ExceptionHandler(ExceptionType which)
{
  int type = machine->ReadRegister(2);

  // If it was a system call
  if(which == SyscallException) {
    if(type == SC_Halt) {
      
      DEBUG('a', "Shutdown, initiated by user program.\n");
      interrupt->Halt();
    }

    if(type == SC_Exec) { 
      DEBUG('a', "Called exec by user program");

      char * name = (char *) machine->ReadRegister(4);
      int argc = machine->ReadRegister(5);
      char ** argv = (char **) machine->ReadRegister(6);
      int opt = machine->ReadRegister(7);

      SpaceId id = Exec2(name, argc, argv, opt); 

      machine->WriteRegister(2, id);
    }
    else if(type == SC_Exit) { 
      DEBUG('a', "Called exit, initiated by user program");

      int status = machine->ReadRegister(4);
      SysCallExit(status);
    }
    else if(type == SC_Read) { 
      DEBUG('a', "Called read, initiated by user program");

      char * buffer = (char *) machine->ReadRegister(4);
      int size = machine->ReadRegister(5);
      OpenFileId id = (OpenFileId) machine->ReadRegister(6);

      Read2(buffer, size, id);
    }
    else if(type == SC_Write) { 
      DEBUG('a', "Called write, initiated by user program");

      char * buffer = (char *) machine->ReadRegister(4);
      int size = machine->ReadRegister(5);
      OpenFileId id = machine->ReadRegister(6);

      Write2(buffer, size, id);
    }
    else if(type == SC_Join) {
      DEBUG('a', "Called Join, initiated by user program");

      SpaceId id = (SpaceId) machine->ReadRegister(4);
      int retVal = Join2(id);

      machine->WriteRegister(2, retVal);
    }
    else if(type == SC_Fork) {
      DEBUG('a', "Called Fork, initiated by user program");

      void (* a) () = (void (*) ()) machine->ReadRegister(4);

      Fork2(a);
    }
    else if(type == SC_Yield) {
      DEBUG('a', "Called Yield, initiated by user program");

      Yield2();
    }
    else {
      fprintf(stderr, "Unexpected System Call %d %d\n", which, type);
      fprintf(stderr, "Killing Process Now\n");
      SysCallExit(-1);
    }
  }
  // else exceptions
  else if(which == PageFaultException) {
    stats->numPageFaults = stats->numPageFaults + 1;

    int virtualAddr = machine->ReadRegister(BadVAddrReg);

    if(coreMap->isFull()) {
      CorePage * corePage = coreMap->evictCorePage();
      BackingStore * backingStore = corePage->getBackingStore();
      backingStore->pageOut(corePage->getVirtualPage());
      memoryManager->FreePage(corePage->getPhysicalPage());
      delete corePage;
    }
    
      
    //  fprintf(stderr, "Killing 33Process Now\n");
    int val = currentThread->space->allocVirtualPage(virtualAddr);

    if(val != 0) {
      fprintf(stderr, "PageFaultException encountered\n");
      fprintf(stderr, "Killing Process Now\n");
      SysCallExit(-1);
    }
    else {
      return;
    }
    //  fprintf(stderr, "Killmking 33Process Now\n");
  }
  else if(which == ReadOnlyException) {
    fprintf(stderr, "ReadOnlyException encountered\n");
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  else if(which == BusErrorException) {
    fprintf(stderr, "BusErrorException encountered\n");
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  else if(which == AddressErrorException) {
    fprintf(stderr, "AddressErrorException encountered\n");
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  else if(which == OverflowException) {
    fprintf(stderr, "OverflowException encountered\n");
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  else if(which == IllegalInstrException) {
    fprintf(stderr, "IllegalInstrException encountered\n");
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  else {
    fprintf(stderr, "Unrecognized Exception Type %d\n", which);
    fprintf(stderr, "Killing Process Now\n");
    SysCallExit(-1);
  }
  
  // increment the PC counter
  int oldNextPCValue = machine->ReadRegister(NextPCReg); 
  machine->WriteRegister(PCReg, oldNextPCValue);
  machine->WriteRegister(NextPCReg, oldNextPCValue+4);
}


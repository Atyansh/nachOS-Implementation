// thread.cc
//	Routines to manage threads.  There are four main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly
//		put back on the ready queue.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "system.h"

#define STACK_FENCEPOST 0xdeadbeef	// this is put at the top of the
// execution stack, for detecting
// stack overflows

//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//	"Pjoin" is an int value that indicates if a thread can be joined.
//	 0 indicates that it cannot and non-zero indicates that it can
//----------------------------------------------------------------------

Thread::Thread(char* threadName, int Pjoin) {
  name = threadName;
  stackTop = NULL;
  stack = NULL;
  status = JUST_CREATED;
#ifdef USER_PROGRAM
  space = NULL;
#endif

  lock = new Lock("Threads lock");
  cv = new Condition("Threads condition variable");

  //-----------------
  // modified code
  //----------------


  //Indicates if thread is joinable
  join = Pjoin;

  //Indicates if thread can be joined
  readyToJoin = 0;

  //The priority of the thread default value
  priority = 0;

 // pipe = 0;
}

//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread() {
  DEBUG('t', "Deleting thread \"%s\"\n", name);

  ASSERT(this != currentThread);
  if (stack != NULL)
    DeallocBoundedArray((char *) stack, StackSize * sizeof(int));

  delete lock;
  delete cv;
}

//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute
//	concurrently.
//
//	NOTE: although our definition allows only a single integer argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		cause it to run the procedure
//		3. Put the thread on the ready queue
//
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------
void Thread::Fork(VoidFunctionPtr func, int arg) {
  DEBUG('t', "Forking thread \"%s\" with func = 0x%x, arg = %d\n",
      name, (int) func, arg);

  StackAllocate(func, arg);

  isForked = 1;  // thread was forked

  IntStatus oldLevel = interrupt->SetLevel(IntOff);
  scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts
  // are disabled!
  (void) interrupt->SetLevel(oldLevel);
}


//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------
void Thread::CheckOverflow() {
  if (stack != NULL)
#ifdef HOST_SNAKE			// Stacks grow upward on the Snakes
    ASSERT(stack[StackSize - 1] == STACK_FENCEPOST);
#else
  ASSERT((int) *stack == (int) STACK_FENCEPOST);
#endif
}


//----------------------------------------------------------------------
// Thread::Finish
// 	Called by ThreadRoot when a thread is done executing the
//	forked procedure.
//
// 	NOTE: we don't immediately de-allocate the thread data structure
//	or the execution stack, because we're still running in the thread
//	and we're still on the stack!  Instead, we set "threadToBeDestroyed",
//	so that Scheduler::Run() will call the destructor, once we're
//	running in the context of a different thread.
//
// 	NOTE: we disable interrupts, so that we don't get a time slice
//	between setting threadToBeDestroyed, and going to sleep.
//
//	NOTE: If a thread is joinable i.e. join is some non-zero value 
//	then this thread cannot run to completion it must wait until its 
//	parent has call
//----------------------------------------------------------------------
void Thread::Finish () {
  (void) interrupt->SetLevel(IntOff);

  ASSERT(this == currentThread);

  DEBUG('t', "Finishing thread \"%s\"\n", getName());

  //Acquire lock
  lock->Acquire();
  //Wait if child is joinable and join has not been called
  while(readyToJoin == 0 && join) {
    //Alert parent that child thread nearing completion
    cv->Signal(lock);
    //Wait for the parent to do something
    cv->Wait(lock);
    //Wake up parent
    cv->Signal(lock);
  }

  //Alert parent that child has exited the while loop
  cv->Signal(lock);
  if(join) {
    cv->Wait(lock);//Wait for parent to give it permission to delete
  }
  lock->Release();//Release 

  if(join){
    DEBUG('t', "\n \nFully Finishing thread,parent has called join \"%s\"\n", 
        getName());  
  }
  threadToBeDestroyed = currentThread;
  Sleep();					// invokes SWITCH
  // not reached
}


//----------------------------------------------------------------------
// Thread::Join
// Called by parent thread to allow said parent to wait for the child
// to terminate.
//
// Note child must be joinable i.e. instance variable join must be non-zero
// Note joined can only be called once
// Note a thread cannot call join on itself
// Note join can only be called after fork has been called
//
//
// Panic if thread is not joinable i.e. is 0.
// Panic if join has been called prior
// Panic if calling thread is itself
// Panic if thread has not been forked.
//----------------------------------------------------------------------
void Thread::Join() {
  ASSERT(join == 1);  // make sure it is joinable
  ASSERT(readyToJoin == 0); // cannot have joined before
  ASSERT(currentThread != this);
  ASSERT(isForked == 1); // must be forked before joining

  lock->Acquire();

  readyToJoin = 1;  // child thread can be deleted

  //wake up child if it is asleep
  cv->Signal(lock);
  cv->Wait(lock);//go to sleep
  cv->Signal(lock);// wake up child threas

  lock->Release();
}


//----------------------------------------------------------------------
//
// Thread::setPriority
// Set the internal priority of the thread to the parameter value.
//
// "newPriority" the value the internal priority is to be set to. Range
// is all ints
//----------------------------------------------------------------------
void Thread::setPriority(int newPriority) {
  priority = newPriority;
}


//----------------------------------------------------------------------
// int Thread::getPriority
// Return the internal priority of the thread
//
// Return value an int that is internal priority of the queue
//----------------------------------------------------------------------
int Thread::getPriority() {
  return priority;
}


//----------------------------------------------------------------------
// int Thread::getJoinValue
// Return the join value of the thread
//
// Return value an int that is the join value after joining
//----------------------------------------------------------------------
int Thread::getJoinValue() {
  return joinValue;
}


//----------------------------------------------------------------------
// int Thread::setJoinValue
// Sets the join value of the thread
//
// Sets value of an int that is the join value after joining
//----------------------------------------------------------------------
void Thread::setJoinValue(int value) {
  joinValue = value;
}


//----------------------------------------------------------------------
// int Thread::getPipeValue
//
// Return the pipe value of the thread
//----------------------------------------------------------------------
int Thread::getPipeValue() {
  return pipeValue;
}


//----------------------------------------------------------------------
// int Thread::setPipeValue
//
// Sets the pipe value of the thread
//----------------------------------------------------------------------
void Thread::setPipeValue(int value) {
  pipeValue = value;
}


//----------------------------------------------------------------------
// void Thread::setInPipe
//
// Sets the pipe value of the thread
//----------------------------------------------------------------------
void Thread::setInPipe(Pipe * value) {
  inPipe = value;
}


//----------------------------------------------------------------------
// Pipe * Thread::getInPipe
//
// Return the pipe value of the thread
//----------------------------------------------------------------------
Pipe * Thread::getInPipe() {
  return inPipe;
}

//----------------------------------------------------------------------
// void Thread::setOutPipe
//
// Sets the pipe value of the thread
//----------------------------------------------------------------------
void Thread::setOutPipe(Pipe * value) {
  outPipe = value;
}


//----------------------------------------------------------------------
// Pipe * Thread::getOutPipe
//
// Return the pipe value of the thread
//----------------------------------------------------------------------
Pipe * Thread::getOutPipe() {
  return outPipe;
}


//----------------------------------------------------------------------
// int Thread::canJoin
//
// Checks whether thread can be joined or not
//----------------------------------------------------------------------
int Thread::canJoin() {
  return join;
}


//----------------------------------------------------------------------
// Thread::Yield
// 	Relinquish the CPU if any other thread is ready to run.
//	If so, put the thread on the end of the ready list, so that
//	it will eventually be re-scheduled.
//
//	NOTE: returns immediately if no other thread on the ready queue.
//	Otherwise returns when the thread eventually works its way
//	to the front of the ready list and gets re-scheduled.
//
//	NOTE: we disable interrupts, so that looking at the thread
//	on the front of the ready list, and switching to it, can be done
//	atomically.  On return, we re-set the interrupt level to its
//	original state, in case we are called with interrupts disabled.
//
// 	Similar to Thread::Sleep(), but a little different.
//----------------------------------------------------------------------
void Thread::Yield () {
  Thread *nextThread;
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  ASSERT(this == currentThread);
  DEBUG('t', "Yielding thread \"%s\"\n", getName());

  nextThread = scheduler->FindNextToRun();
  if (nextThread != NULL) {
    scheduler->ReadyToRun(this);
    scheduler->Run(nextThread);
  }
  (void) interrupt->SetLevel(oldLevel);
}


//----------------------------------------------------------------------
// Thread::Sleep
// 	Relinquish the CPU, because the current thread is blocked
//	waiting on a synchronization variable (Semaphore, Lock, or Condition).
//	Eventually, some thread will wake this thread up, and put it
//	back on the ready queue, so that it can be re-scheduled.
//
//	NOTE: if there are no threads on the ready queue, that means
//	we have no thread to run.  "Interrupt::Idle" is called
//	to signify that we should idle the CPU until the next I/O interrupt
//	occurs (the only thing that could cause a thread to become
//	ready to run).
//
//	NOTE: we assume interrupts are already disabled, because it
//	is called from the synchronization routines which must
//	disable interrupts for atomicity.   We need interrupts off
//	so that there can't be a time slice between pulling the first thread
//	off the ready list, and switching to it.
//----------------------------------------------------------------------
void Thread::Sleep () {
  Thread *nextThread;

  ASSERT(this == currentThread);
  ASSERT(interrupt->getLevel() == IntOff);

  DEBUG('t', "Sleeping thread \"%s\"\n", getName());

  status = BLOCKED;
  while ((nextThread = scheduler->FindNextToRun()) == NULL)
    interrupt->Idle();	// no one to run, wait for an interrupt

  scheduler->Run(nextThread); // returns when we've been signalled
}


//----------------------------------------------------------------------
// ThreadFinish, InterruptEnable, ThreadPrint
//	Dummy functions because C++ does not allow a pointer to a member
//	function.  So in order to do this, we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish() {
  currentThread->Finish();
}
static void InterruptEnable() {
  interrupt->Enable();
}
void ThreadPrint(int arg) {
  Thread *t = (Thread *)arg;
  t->Print();
}


//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------
void Thread::StackAllocate (VoidFunctionPtr func, int arg) {
  stack = (int *) AllocBoundedArray(StackSize * sizeof(int));

#ifdef HOST_SNAKE
  // HP stack works from low addresses to high addresses
  stackTop = stack + 16;	// HP requires 64-byte frame marker
  stack[StackSize - 1] = STACK_FENCEPOST;
#else
  // i386 & MIPS & SPARC stack works from high addresses to low addresses
#ifdef HOST_SPARC
  // SPARC stack must contains at least 1 activation record to start with.
  stackTop = stack + StackSize - 96;
#else  // HOST_MIPS  || HOST_i386
  stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
#ifdef HOST_i386
  // the 80386 passes the return address on the stack.  In order for
  // SWITCH() to go to ThreadRoot when we switch to this thread, the
  // return addres used in SWITCH() must be the starting address of
  // ThreadRoot.
  *(--stackTop) = (int)ThreadRoot;
#endif
#endif  // HOST_SPARC
  *stack = STACK_FENCEPOST;
#endif  // HOST_SNAKE

  machineState[PCState] = (int) ThreadRoot;
  machineState[StartupPCState] = (int) InterruptEnable;
  machineState[InitialPCState] = (int) func;
  machineState[InitialArgState] = arg;
  machineState[WhenDonePCState] = (int) ThreadFinish;
}

#ifdef USER_PROGRAM
#include "machine.h"


//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers --
//	one for its state while executing user code, one for its state
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------
void Thread::SaveUserState() {
  for (int i = 0; i < NumTotalRegs; i++)
    userRegisters[i] = machine->ReadRegister(i);
}


//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers --
//	one for its state while executing user code, one for its state
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------
void Thread::RestoreUserState() {
  for (int i = 0; i < NumTotalRegs; i++) {
    machine->WriteRegister(i, userRegisters[i]);
  }
  machine->pageTable = currentThread->space->getPageTable();
  machine->pageTableSize = currentThread->space->getNumPages();
}

#endif

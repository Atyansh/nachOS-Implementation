// synch.cc
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------
Semaphore::Semaphore(char* debugName, int initialValue) {
  name = debugName;
  value = initialValue;
  queue = new List;
}


//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------
Semaphore::~Semaphore() {
  delete queue;
}


//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------
void Semaphore::P() {
  IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts

  while (value == 0) { 			// semaphore not available
    queue->SortedInsert((void *)currentThread,
                      currentThread->getPriority()*-1);	// so go to sleep
    currentThread->Sleep();
  }
  value--; 					// semaphore available,
  // consume its value

  (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}


//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------
void Semaphore::V() {
  Thread *thread;
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  thread = (Thread *)queue->Remove();
  if (thread != NULL)	   // make thread ready, consuming the V immediately
    scheduler->ReadyToRun(thread);
  value++;
  (void) interrupt->SetLevel(oldLevel);
}


//----------------------------------------------------------------------
//                            P1 Code
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Lock::Lock
// 	Initialize a lock, .
//
//	"debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------
Lock::Lock(char* debugName) {
  name = debugName;
  locked = 0; // lock is free
  queue = new List;
  threadHeld = 0;
}


//----------------------------------------------------------------------
// Lock::~Lock
// 	De-allocate lock, when no longer needed. Panic if some thread is
// 	waiting on the lock or if lock is being held.!
//----------------------------------------------------------------------
Lock::~Lock() {
  ASSERT(locked != 1); // cannot delete lock if lock is currently acquired
  ASSERT(queue->IsEmpty()); // cannot delete if queue is not empty
  delete queue;
}


//----------------------------------------------------------------------
// Lock::Acquire
//  Disable interupts to achieve atomicity. If Lock is being held put
//  thread to sleep.Else set lock to being held and set variable current
//  thread owner to the calling thread.Re-enable interupts.
//
// Panic if thread that holds lock attempts to require it.
//----------------------------------------------------------------------
void Lock::Acquire() {
  IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts

  ASSERT(locked == 0 || !isHeldByCurrentThread()); // deadlock on itself

  while (locked == 1) { 			// lock is held
    queue->SortedInsert((void *)currentThread,
                      currentThread->getPriority()*-1);	// so go to sleep
    currentThread->Sleep();
  }
  locked = 1; 					// acquire lock
  threadHeld = currentThread;  // keep track of which thread is locked

  (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}


//----------------------------------------------------------------------
// bool Lock::isHelByCurrentThread
// Check if calling thread is the owner of the lock.
//
// Return value is a boolean,true if caller is owner,false otherwise.
//
//----------------------------------------------------------------------
bool Lock::isHeldByCurrentThread() {
  return currentThread == threadHeld;
}

//----------------------------------------------------------------------
// Lock::Release
//
// Disable iterupts to simulate atomicity. If current thread is the owner 
// release the lock,remove a thread from the head of the queue and 
// set  said thread as Ready To Run.
//
// Panic if lock is not held.
// Panic if caller is not the current holder of the lock
//----------------------------------------------------------------------
void Lock::Release() {
  Thread *thread;

  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  ASSERT(locked == 1); // lock must be acquired
  ASSERT(isHeldByCurrentThread()); // only owner of lock can release

  thread = (Thread *)queue->Remove();

  if (thread != NULL)	  // make thread ready, consuming the thread immediately
    scheduler->ReadyToRun(thread);

  locked = 0;   // lock is free 
  threadHeld = 0;// lock has no owner

  (void) interrupt->SetLevel(oldLevel);
}


//----------------------------------------------------------------------
// Condition::Condition
// Initialize condition with no waiting threads.
//
// "debugName" some name useful for debugging
//----------------------------------------------------------------------
Condition::Condition(char* debugName) {
  name = debugName;
  waitQ = new List;
}


//----------------------------------------------------------------------
// Condition::~Condition
// De-allocate a condition variable that has no waiting threads.
//
// Panic if at least one thread is waiting.
//----------------------------------------------------------------------
Condition::~Condition() {
  ASSERT(waitQ->IsEmpty()); // cannot delete if queue is not empty
  delete waitQ;
}


//----------------------------------------------------------------------
// Condition::Wait
// Disable interupts to simulate atomicity.Calling thread releases lock
// ,is placed into waiting queue and put to sleep.Re-enable interupts.
// When thread is woken up reaquire lock and exit the thread.
//
// Panic if the passed in lock is null.
// Panic if calling thread does not hold the lock.
// Panic if thread doesn't hold lock when method exits.
//----------------------------------------------------------------------
void Condition::Wait(Lock* conditionLock) {
  IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts

  ASSERT(conditionLock != NULL); 
  ASSERT(conditionLock->isHeldByCurrentThread()); // wait if you own lock 

  conditionLock->Release(); // release the lock


  waitQ->SortedInsert((void *)currentThread,
                      currentThread->getPriority()*-1);	// so go to sleep

  currentThread->Sleep(); // suspend execution of thread

  (void) interrupt->SetLevel(oldLevel); // re-enable interrupts

  conditionLock->Acquire(); // acquire the lock again

  ASSERT(conditionLock->isHeldByCurrentThread());
}


//----------------------------------------------------------------------
// Condition::Signal
// Remove thread from head of waiting queue and set said thread for ready to 
// run.
// Note if no threads are waiting signal is lost(i.e.operation is a no-op)
//
// Panic: if calling thread doesn't hold the lock
//----------------------------------------------------------------------
void Condition::Signal(Lock* conditionLock) {
  ASSERT(conditionLock->isHeldByCurrentThread());//Check if calling thread
  //holds lock

  Thread *thread;

  thread = (Thread *)waitQ->Remove(); //remove next thread from queue

  if(thread != NULL)
    scheduler->ReadyToRun(thread); // mark this thread as eligible to run
}


//----------------------------------------------------------------------
// Condition::Broadcast
// Remove threads  from waiting queue and set said threads as ready to 
// run.
//
// Note if no threads are waiting broadcast is lost(i.e. operation is no-op)
//
// Panic: if calling thread doesn't hold the lock
//----------------------------------------------------------------------
void Condition::Broadcast(Lock* conditionLock) {
  //Check that calling thread holds lock
  ASSERT(conditionLock->isHeldByCurrentThread());

  Thread * thread;

  //Remove all threads from the queue and set them as ready to run
  while(!waitQ->IsEmpty()) {
    thread = (Thread *) waitQ->Remove();

    if(thread != NULL)
      scheduler->ReadyToRun(thread);
  }
}


//----------------------------------------------------------------------
// Whale:Whale
// Initialize whale with no calls having been made.
//
// "debugName" some name the whale instance will be associated with.
//----------------------------------------------------------------------
Whale::Whale(char * debugName) {
  //Condition Variables for each call initialized
  maleWhale = new Condition("Male");
  femaleWhale = new Condition("Female");
  matchmaker = new Condition("Matchmaker");

  //Lock associated with class
  lock = new Lock("Whale Lock");

  //No calls have been made to any of the three methods
  maleCount = 0;
  femaleCount = 0;
  matchmakerCount = 0;

  //No calls are in the process of being matched
  maleFound = 0;
  femaleFound = 0;
  matchmakerFound = 0;
}


//----------------------------------------------------------------------
// Whale::~Whale
// De-allocate the whale.
//
// Panic if any calls are still waiting to be matched.
//----------------------------------------------------------------------
Whale::~Whale() {
  delete maleWhale;
  delete femaleWhale;
  delete matchmaker;
  delete lock;
}


//----------------------------------------------------------------------
// Whale::resetAll
// Alter all variables to indicate that match has been made between
// Male(),Female() and Matchmaker() calls.
//----------------------------------------------------------------------
void Whale::resetAll() {
  fprintf(stderr, "Match Found\n");
  //One less of each call is availible for match making
  maleCount--;
  femaleCount--;
  matchmakerCount--;

  //No call is in the process of being matched
  maleFound = femaleFound = matchmakerFound = 0;

  //Alert other threads that they can be potentially matched
  maleWhale->Signal(lock);
  femaleWhale->Signal(lock);
  matchmaker->Signal(lock);
}


//----------------------------------------------------------------------
// Whale::Male
// Acquire lock,increment count of number of whale calls availible,alert
// other threads of potential match and go to sleep if no other 
// corresponding pair is availible.If a pair is found set variable 
// indicating that male match has been made. If this call is the last
// of Male(),Female() and Matchmaker() to execute then call reset all.
// Release lock
//----------------------------------------------------------------------
void Whale::Male() {
  lock->Acquire();

  maleCount++;//Increment the number of male calls made

  //Alert other calls that potential match found
  femaleWhale->Signal(lock);
  matchmaker->Signal(lock);

  //Sleep if no match is present of male match has been made.
  while((matchmakerCount < 1 || femaleCount < 1) && maleFound==0) {
    maleWhale->Wait(lock);
  }

  //Indicate that male has been grouped with female and matchmaker calls
  maleFound = 1;

  //If last to exit call reset all
  if(maleFound && femaleFound && matchmakerFound) {
    resetAll();    
  }

  lock->Release();
}


//----------------------------------------------------------------------
// Whale::Female
// Acquire lock,increment count of number of whale calls availible,alert
// other threads of potential match and go to sleep if no other 
// corresponding pair is availible.If a pair is found set variable 
// indicating that male match has been made. If this call is the last
// of Male(),Female() and Matchmaker() to execute then call reset all.
// Release lock
//----------------------------------------------------------------------
void Whale::Female() {
  lock->Acquire();

  femaleCount++;//Increment number of female calls that have been made

  //Alert other calls of potential match
  maleWhale->Signal(lock);
  matchmaker->Signal(lock);

  //Sleep if no match is present of female match has been made.
  while((matchmakerCount<1 || maleCount<1) && femaleFound==0) {
    femaleWhale->Wait(lock);
  }

  //Indicate that female match has been made
  femaleFound = 1;

  //If last of group to exit call reset all
  if(maleFound && femaleFound && matchmakerFound) {
    resetAll();    
  }

  lock->Release();
}


//----------------------------------------------------------------------
// Whale::Matchmaker
// Acquire lock,increment count of number of whale calls availible,alert
// other threads of potential match and go to sleep if no other 
// corresponding pair is availible.If a pair is found set variable 
// indicating that male match has been made. If this call is the last
// of Male(),Female() and Matchmaker() to execute then call reset all.
// Release lock
//----------------------------------------------------------------------
void Whale::Matchmaker() {
  lock->Acquire();

  matchmakerCount++;//Increment number of matchmaker calls made

  //Alert other calls of potential match
  maleWhale->Signal(lock);
  femaleWhale->Signal(lock);

  //Wait if no group of three is possible or if another 
  //matchmaker is in the process of being matched
  while((maleCount < 1 || femaleCount < 1) && matchmakerFound == 0) {
    matchmaker->Wait(lock);
  }

  //Indicate that matchmaker is currently being matched
  matchmakerFound = 1;

  //Call resetAll if last of group to exit
  if(maleFound && femaleFound && matchmakerFound) {
    resetAll();    
  }

  lock->Release();
}


//----------------------------------------------------------------------
// MailBox::MailBox
// Initialize Mailbox as if no send or recieve calls have been made.
//
// "debugName" some name to be associated with mailbox instance
//  useful for debugging
//----------------------------------------------------------------------
Mailbox::Mailbox(char * debugName) {
  lock = new Lock("MailBox Lock");//Lock associated with class
  sendWait = new Condition("Send CV");//Condition associated with send
  receiveWait = new Condition("receive Wait");//Condition associated with rec

  receivePres=0;//Number of receive calls that have been made
  bufferRead = 0;//Indicate that buffer is not being used
  buffer = 0;//Actual buffer to be used
}


//----------------------------------------------------------------------
// Mailbox::~Mailbox
// De-allocates Mailbox.
//
// Panic if at least one recieve or send call is waiting.
//----------------------------------------------------------------------
Mailbox::~Mailbox() {
  //delete all fields.Panic should be generated by one of them
  delete lock;
  delete sendWait;
  delete receiveWait;
}


//----------------------------------------------------------------------
// Mailbox::Send
// Acquire lock, wait if buffer is currently in use or no recieve calls
// have been made.Else place message in the buffer,indicate that buffer
// is being used and then alert recieve calls via signal.Release lock.
//
// "message" the message to be passed.
//----------------------------------------------------------------------
void Mailbox::Send(int message) {
  lock->Acquire();

  //wait while buffer is being used or if no receive calls have been made
  while(bufferRead==1 ||receivePres<=0) {
    sendWait->Wait(lock); 
  }

  //Place message into the buffer
  buffer = message;
  //Indicate that buffer is being used
  bufferRead = 1;

  //Alert receive calls to possible match
  receiveWait->Signal(lock);

  lock->Release();
}


//----------------------------------------------------------------------
// Mailbox::Receive 
// Acquire lock.Increment variable associated with number of receive 
// calls that have been made.Alert send calls of the arrival of a 
// receive call. Wait while no message is in the buffer.Once message
// is in the internal buffer,place the message into the buffer passed
// in as parameter.Indicate that message has been passed on and decrment
// the number of receive calls that can be paired with.Release lock.
//
//
// "message" pointer to external method that message will be passed into.
//----------------------------------------------------------------------
void Mailbox::Receive(int * message) {
  lock->Acquire();

  receivePres++;//Number of receive calls to match with increases
  sendWait->Signal(lock);//Alert senders to arrival of new receives
  //Wait while no message is in the buffer
  while(bufferRead == 0) {
    receiveWait->Wait(lock);   
  }

  //place message in external buffer and indicate that internal buffer can 
  //be reused
  *message = buffer;
  buffer = 0;
  bufferRead = 0;
  receivePres--;///decrment number of availible receive calls

  lock->Release();
}

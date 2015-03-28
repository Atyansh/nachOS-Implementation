#include "copyright.h"
#include "system.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;


//----------------------------------------------------------------------
// SimpleThread
//  Loop 5 times, yielding the CPU to another ready thread 
//  each iteration.
//
//  "which" is simply a number identifying the thread, for debugging
//  purposes.
//----------------------------------------------------------------------
void
SimpleThread(int which) {
  int num;

  for (num = 0; num < 5; num++) {
    fprintf(stderr, "*** thread %d looped %d times\n", which, num);
    currentThread->Yield();
  }
}


//----------------------------------------------------------------------
// ThreadTest1
//  Set up a ping-pong between two threads, by forking a thread 
//  to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------
void
ThreadTest1() {
  DEBUG('t', "Entering ThreadTest1");

  Thread *t = new Thread("forked thread");

  t->Fork(SimpleThread, 1);
  SimpleThread(0);
}


// LockTest
Lock *locktest1 = NULL;


//----------------------------------------------------------------------
// AcquireThenRelease
// Thread simply grabs and releases the lock
//----------------------------------------------------------------------
void AcquireThenRelease(int pram) {
  locktest1->Acquire();
  fprintf(stderr, "Child thread now holds lock.Will be release now\n");
  locktest1->Release();
}


//----------------------------------------------------------------------
// acquireTwice
// Helper method to test what occurs when a thread attempts to acquire
// the thread twice. 
// Should crash the system
//
// param is there to satisfy fork's signature requirment
//----------------------------------------------------------------------
void acquireTwice(int param) {
  locktest1->Acquire();
  fprintf(stderr, "Attempting to acquire twice\n ");
  locktest1->Acquire();
}


//----------------------------------------------------------------------
// grabLock
// Helper method thread attempts to acquire lock
//
// pram there to satisfy requirment for fork
//----------------------------------------------------------------------
void grabLock(int pram) {
  locktest1->Acquire();
}


//----------------------------------------------------------------------
// LockThread1
// Helper method that works in synch with lockThread2.Will print
// out characters in predictable manner.
//
// param is there to satisfy fork signature requirment
//----------------------------------------------------------------------
void
LockThread1(int param) {
  fprintf(stderr, "L1:0\n");
  locktest1->Acquire();
  fprintf(stderr, "L1:1\n");
  currentThread->Yield();
  fprintf(stderr, "L1:2\n");
  locktest1->Release();
  fprintf(stderr, "L1:3\n");
}


//----------------------------------------------------------------------
// LockThread2
// Helper method that works in synch with lockThread1.Will print
// out characters in predictable manner.
//
// param is there to satisfy fork signature requirment
//----------------------------------------------------------------------
void
LockThread2(int param) {
  fprintf(stderr, "L2:0\n");
  locktest1->Acquire();
  fprintf(stderr, "L2:1\n");
  currentThread->Yield();
  fprintf(stderr, "L2:2\n");
  locktest1->Release();
  fprintf(stderr, "L2:3\n");
}


//----------------------------------------------------------------------
// LockTest1
// Method uses lock to force two seperate threads to operate in
// a predictable manner
//----------------------------------------------------------------------
void
LockTest1() {
  DEBUG('t', "Entering LockTest1");

  locktest1 = new Lock("LockTest1");

  Thread *t = new Thread("one");
  t->Fork(LockThread1, 0);
  t = new Thread("two");
  t->Fork(LockThread2, 0);
}


//----------------------------------------------------------------------
// LocktestHolder_Reacquires
// Tests that system chrashs when lock holder attempts to acquire 
// thread twice.
//----------------------------------------------------------------------
void LocktestHolder_Reacquires() {
  Thread * t=new Thread("holder");
  locktest1 = new Lock("lock multiple attempts");
  //helper method attempts to acquire lock twice
  t->Fork(acquireTwice,0);
}


//----------------------------------------------------------------------
// Locktest_ReleaseWithout_Held
// Tests that system chrashs when the someone attempts to release an
// unheld lock.
//----------------------------------------------------------------------
void Locktest_ReleaseWithout_Held() {
  locktest1=new Lock("Release without being held");
  fprintf(stderr, "Trying to Release Lock without holding one\n");
  locktest1->Release();
}


//----------------------------------------------------------------------
// Locktest_ReleaseWithout_Held2
//
// Tests that system chrashes when release is called on a thread that isn't
// held. Added condition is that another thread has grabbed and released the
// lock.
//----------------------------------------------------------------------
void Locktest_ReleaseWithout_Held2() {
  locktest1=new Lock("Release without being held");
  locktest1->Acquire();

  for(int i=0;i<10;i++) {
    Thread * helper= new Thread("Helper for being releases without being held");
    //Child will attempt to acquire and release lock
    helper->Fork(AcquireThenRelease,0);
  }

  fprintf(stderr, "parent thread holds lock.Will be released now\n");
  locktest1->Release();

  fprintf(stderr, "parent will now release lock without bein held.Should crash\n");
  locktest1->Release();
}


//----------------------------------------------------------------------
// LockTest_DeleteLockThatIsHeld
// Deletes lock that is being held and waited upon.
//----------------------------------------------------------------------
void LockTest_DeleteLockThatIsHeld() {
  locktest1 = new Lock("Delete lock that is held");
  locktest1->Acquire();
  Thread * sub=new Thread("helper");

  //Lock is now being waited upon
  sub->Fork(grabLock,0);
  fprintf(stderr, "At least one thread is waiting on the lock \n");
  fprintf(stderr, "Attempting to delete lock expect a crash \n");
  delete locktest1;  // should crash
}


//---------------------------------------------------------------------
// Condition variable tests
//----------------------------------------------------------------------

//variables for testing
Condition * conditionTest = NULL;
Lock * conditionLock = NULL;


//----------------------------------------------------------------------
// ThreadWait 
// Helper method simply makes thread wait
// 
// pram there to satisfy fork requirment
//----------------------------------------------------------------------
void ThreadWait(int pram) {
  conditionLock->Acquire();

  conditionTest->Wait(conditionLock);
  fprintf(stderr, "Thread has been woken up \n \n");
  conditionLock->Release();
}


//----------------------------------------------------------------------
// MulitYield
// Simply makes a thread yield multiple times. Ensures that other threads
// are allowed a chance at the lock
//
// numberToYield number of times thread is expect to yield
//----------------------------------------------------------------------
void MultiYield(int numberToYield) {
  for(int i=0;i<numberToYield;i++)
    currentThread->Yield();
}


//----------------------------------------------------------------------
// LoadOntoCV
// Helper method to make multiple threads wait on condition variable
//
// numOftests is the number of threads to be placed onto the condition 
// variable
//----------------------------------------------------------------------
void LoadOntoCV(int numOftests) {
  Thread * subject;
  for(int i=0;i<numOftests;i++) {
    subject = new Thread("Will be loaded onto condition variable\n");
    subject->Fork(ThreadWait,0);
  }

  //Ensure that all threads are  waiting
  MultiYield(numOftests*2);
}


//----------------------------------------------------------------------
// Condition_Test_Signal_WakesUpOne
// Tests that signal wakes up only a single thread.
// Should expect a single print statement
//----------------------------------------------------------------------
void Condition_Test_Signal_WakesUpOne() {
  fprintf(stderr, "Testing that signal wakes up a single thread \n");
  conditionTest = new Condition("Condition Var test");
  conditionLock = new Lock("Lock for cv");

  //Make 20 threads wait on condition
  LoadOntoCV(20);

  conditionLock->Acquire();
  fprintf(stderr, "Lock has been acquired.Preparing to signal one print statement");
  fprintf(stderr, " should appear \n \n");

  conditionTest->Signal(conditionLock);
  conditionLock->Release();
}


//----------------------------------------------------------------------
// Condition_Test_BroadCastWakesAll
// Tests that broadcast wakes up all threads that are waiting on
// condition
//----------------------------------------------------------------------
void Condition_Test_BroadCastWakesAll() {
  fprintf(stderr, "Testing that broadcast wakes up all threads \n");
  conditionTest = new Condition("Condition Var test");
  conditionLock = new Lock("Lock for cv");

  //Make 5 threads wait on condition variable
  int numOftests = 5;
  LoadOntoCV(numOftests);

  //Broadcast should wake up all threads
  conditionLock->Acquire();
  fprintf(stderr, "Lock has been acquired.Preparing to broadcast  %d print",numOftests); 
  fprintf(stderr, " statements should  appear \n");

  conditionTest->Broadcast(conditionLock);
  conditionLock->Release();
}


//----------------------------------------------------------------------
// Condition_Test_Signal_Lost
// Tests that signal operation on empty condition variable does not
// affect later operations
//----------------------------------------------------------------------
void Condition_Test_Signal_Lost() {
  fprintf(stderr, "Testing that signal is lost if no thread is waiting.");
  conditionTest = new Condition("Condition Var test");
  conditionLock = new Lock("Lock for cv");

  //NO-op signal is called
  fprintf(stderr, "Calling signal should be lost\n");
  conditionLock->Acquire();
  conditionTest->Signal(conditionLock);
  conditionLock->Release();

  //Make 5 threads wait on condition
  int numOfTests=5;
  LoadOntoCV(numOfTests);

  //Tests that signal still functions 
  //One statement should appear
  conditionLock->Acquire();

  fprintf(stderr, "Lock has been acquired.Preparing to signal one  print statement");
  fprintf(stderr, " should  appear \n");

  conditionTest->Signal(conditionLock);
  conditionLock->Release();

  //Ensure that thread that has been woken up has a chance to grab lock
  MultiYield(numOfTests*2);

  fprintf(stderr, "\n \n");

  //Tests that broadcast still functions 
  conditionLock->Acquire();
  fprintf(stderr, "Lock has been acquired.Preparing to broadcast  %d",numOfTests-1);
  fprintf(stderr, " print statements should  appear \n \n");

  conditionTest->Broadcast(conditionLock);
  conditionLock->Release();
}


//----------------------------------------------------------------------
// Condition_Test_BroadCast_Lost
// Tests that broadcast on an empty condtion variable will no affect
// later operations.
//----------------------------------------------------------------------
void Condition_Test_BroadCast_Lost() {
  conditionTest = new Condition("Condition Var test");
  conditionLock = new Lock("Lock for cv");

  //BroadCast should be lost
  fprintf(stderr, "Calling Broadcast should be lost\n");
  conditionLock->Acquire();
  conditionTest->Broadcast(conditionLock);
  conditionLock->Release();

  //Make 5 threads waitt
  int numOfTests=5;
  LoadOntoCV(5);

  conditionLock->Acquire();

  //Tests that signal still functions
  fprintf(stderr, "Lock has been acquired.Preparing to signal one thread");
  fprintf(stderr, " print statement should  appear \n \n");

  conditionTest->Signal(conditionLock);

  conditionLock->Release();

  fprintf(stderr, "\n \n");

  //Ensure that child threads have a chance to seize lock.
  MultiYield(numOfTests); 

  //Broadcast tested to determine if it still functions
  conditionLock->Acquire();
  fprintf(stderr, "Lock has been acquired.Preparing to broadcast  %d",numOfTests-1);
  fprintf(stderr, " print statements should  appear \n \n");

  conditionTest->Broadcast(conditionLock);
  conditionLock->Release();
}


//----------------------------------------------------------------------
// Condition_Test_GivesUpLockInWait
// Tests that thread gives up the lock when it enters the wait method
//----------------------------------------------------------------------
void Condition_Test_GivesUpLockInWait() {
  conditionTest = new Condition("Testing that condition gives up lock wait");
  conditionLock = new Lock("Test that condition gives up lock in wait");

  Thread * subject = new Thread("helper");

  //Single thread waits
  subject->Fork(ThreadWait,0); 
  fprintf(stderr, "Testing that CV releases lock when wait has been called \n");

  fprintf(stderr, "Should expect print statement, stating result \n");

  conditionLock-> Acquire();

  fprintf(stderr, "Success\n");
  conditionTest->Signal(conditionLock);
  conditionLock->Release();
}


//----------------------------------------------------------------------
// Condition_TestNotHoldingHelper
// Method attempts to call wait without holding lock
//----------------------------------------------------------------------
void Condition_TestNotHoldingHelper(int pram) {
  fprintf(stderr, "Child thread attempting to wait without lock.Should crasha\n");
  conditionTest->Wait(conditionLock);
}


//----------------------------------------------------------------------
// ConditionTest_NotHoldingLock
// Tests that the system crashs if a thread attempts to call wait 
// without the lock being held
//----------------------------------------------------------------------
void ConditionTest_NotHoldingLock() {
  conditionLock = new Lock("Lock for condition");
  conditionTest =  new Condition("Actviate condition without lock");

  conditionLock->Acquire();
  fprintf(stderr, "Condition created and child thread created. Parent hold lock\n");
  Thread * helper = new Thread("Condition without holding lock\n");
  //Helper calls function without holding lock
  helper->Fork(Condition_TestNotHoldingHelper,0);
}


//----------------------------------------------------------------------
// ConditionTest_DeleteWaitTest
// Tests that deleting a condition with a thread waiting on it 
// crashs the system
//----------------------------------------------------------------------
void ConditionTest_DeleteWaitTest() {
  fprintf(stderr, "Loading conditional variable with threads.");
  conditionLock = new Lock("Delete conditon with thread on lock");
  conditionTest = new Condition("Delete condition while threads waiting\n");
  fprintf(stderr, "Loading condtion with ten threads\n");

  //Make 20 threads wait on the condition variable
  LoadOntoCV(20);

  fprintf(stderr, "Delete condtion with thread waiting");
  delete conditionTest;
}


//----------------------------------------------------------------------
// Mailbox tests
//----------------------------------------------------------------------
Mailbox * subject;//Subject that will be tested
int * buffer;// buffer that will be used by receive
int message;// message that will be passed into receive


//----------------------------------------------------------------------
// callReceive
// Helper method that will allow any thread to call receive.
//----------------------------------------------------------------------
void callReceive(int pram) {
  subject->Receive(buffer);
}


//----------------------------------------------------------------------
// callSend
// Helper method that will allow any thread to call send
//----------------------------------------------------------------------
void callSend(int pram) {
  subject->Send(message);
}


//----------------------------------------------------------------------
// MultiSender 
// Helper method that sends some specified number of messages.
// 
// "numberOfSenders" the number of send calls that are to be made
// "start" is an optional variable inidcating where the content of
// the messages will start
// "sendGoesFirst" binary variable indicating if send is being called 
//  first.1 indicates that it is ,else it is  being called after 
//  receive.
//
//  Note:That if this method is called after MultiReceive,
//  then the numberOfSenders<=numberOfReceivers.
//----------------------------------------------------------------------
void MultiSender(int numberOfSenders,int start=0,int sendGoesFirst=1) {
  Thread * sender;//Will send multiple messages

  int total = numberOfSenders+start;
  fprintf(stderr, "Sending %d messages that contain ints %d to %d \n",numberOfSenders,
      start,total-1);

  //Send up to numberOfSenders -1 messages
  for(;start<total;start++) {
    sender= new Thread("sender");

    message=start;//place some number in the queue
    sender->Fork(callSend,0);//Send message
    MultiYield(40);//give time for child to finish

    //if send goes second,message must be received
    if(sendGoesFirst==0) {
      fprintf(stderr, "A: Some thread has returned with message %d \n",*buffer);
    }
    else
      fprintf(stderr, "SEND CALLED FIRST, WAITING FOR RECEIVE\n");
  }
}

//----------------------------------------------------------------------
// MultiReceive
// Helper method that is used to call receive multiple messages
//
// "numberOfReceivers" is the desired number of times receive must
// be called
// "receiveGoesFirst" indicates if receive has been called before
// send.If 0 receive is called second,else receive is called first.
// by default set to 0.
//
// Note:That if this method is called after MultiSender,
// then the numberOfSenders>=numberOfReceivers.
//----------------------------------------------------------------------
void MultiReceive(int numberOfReceivers,int receiveGoesFirst=0) {
  Thread * receiver;//Will receive multiple messages

  //If receive is called second there must be something in the buffer
  if(receiveGoesFirst==0) {
    fprintf(stderr, "Preparing to receive %d messages,check that contents are correct \n",
        numberOfReceivers);
    fprintf(stderr, "Note that numbers will appear in random order \n");
  }
  else
    fprintf(stderr, "%d waiters spawned and waiting for messages \n",numberOfReceivers);


  for(int i=0;i<numberOfReceivers;i++) {
    receiver = new Thread("receiver");
    receiver -> Fork(callReceive,0);
    MultiYield(40);
    //If goes second there must be some message inside
    if(receiveGoesFirst==0)
      fprintf(stderr, "RECEIVE WENT SECOND: Some thread has returned with message %d \n",*buffer);
    else
      fprintf(stderr, "RECEIVE WENT FIRST, BUFFER DOESN'T MATTER\n");
  }
}


//----------------------------------------------------------------------
// MailBoxSimpleTest
// Tests that a single send call will pair with a single receive call and
// that a single receive call will pair with a 
//----------------------------------------------------------------------
void MailBoxSimpleTest() {
  subject = new Mailbox("Test subject");
  //Agent wil be the thread that send and receive
  Thread * agent= new Thread("Sender thread");

  //Makes 'a' the message 
  message= 'a';

  //Should send 'a'
  agent->Fork(callSend,0);

  //Gives child a chance to perform task
  MultiYield(1);

  //Agent will now receive
  agent= new Thread("Receiver thread");
  buffer = new int();
  agent->Fork(callReceive,0);

  //Yield allows other threads to get the lock first
  MultiYield(30);

  fprintf(stderr, "Testing that mailbox works when send is called first\n");
  fprintf(stderr, "Should print %c, actual result %c \n \n",message,*buffer);

  agent = new Thread("Receiver thread\n");
  message='b';

  //child now calls receive
  agent->Fork(callReceive,0);

  //Gives child a chance to perform the task
  MultiYield(1);

  agent = new Thread("send called second");
  agent->Fork(callSend,0);

  //Give children thread a chance to run to completion
  MultiYield(30);

  fprintf(stderr, "Testing that mailbox works when receive is called first\n");
  fprintf(stderr, "Should print %c, actual result %c \n \n",message,*buffer);
}


//----------------------------------------------------------------------
// MailBoxMultiSendersWaiting
// Test that mailbox functions when multiple senders are present and waiting.
//----------------------------------------------------------------------
void MailBoxMultiSendersWaiting() {
  subject = new Mailbox("Subject mailbox");

  buffer = new int();
  //send six messages 
  MultiSender(6);
  MultiYield(40);//give children time to execute
  MultiReceive(6);
}


//----------------------------------------------------------------------
// MailBoxMulitReceiveWaiting
// Test that mailbox functions when multiple receivers are present and
// waiting.
//----------------------------------------------------------------------
void MailBoxMultiReceiveWaiting() {
  subject = new Mailbox("Subject mailbox");

  buffer = new int();

  //spawn six receive calls and wait
  MultiReceive(6,1);

  MultiYield(40);

  //spawn six senders,start at zero and is being called after receive.
  MultiSender(6,0,0);
}


//----------------------------------------------------------------------
// MailBoxCrashsWhenDeleted
// Tests that the system crashs if mailbox is deleted and at least one
// sender is waiting.
//----------------------------------------------------------------------
void MailBoxCrashsWhenDeleted() {
  subject = new Mailbox("subject mailbox");

  //assign some message
  message=0;
  //one thread is placed onto the mailbox
  MultiSender(1);

  //notify tester
  fprintf(stderr, "\n At least one sender is waiting in mailbox \n");
  fprintf(stderr, "About to delete mailbox expect crash \n");

  delete subject;
}


//----------------------------------------------------------------------
// MailBoxCrashsWhenDeleted2
// Tests that the system crashs if mailbox is deleted and at least one
// receiver is waiting.
//----------------------------------------------------------------------
void MailBoxCrashsWhenDeleted2() {
  subject = new Mailbox("subject mailbox");
  //buffer is allocated some memory
  buffer=new int();
  //one thread is placed onto the mailbox
  MultiReceive(1,1);

  //notify tester
  fprintf(stderr, "\n At least one receiver is waiting in mailbox \n");
  fprintf(stderr, "About to delete mailbox expect crash \n");

  delete subject;
}


//----------------------------------------------------------------------
// MailBoxDeleteEmpty
// Tests that deleting an empty mailbox doesn't chrash the system
//----------------------------------------------------------------------
void MailBoxDeleteEmpty() {
  subject = new Mailbox("subject mailbox");
  fprintf(stderr, "Deleting empty mail box. Nothing should happend \n");
  delete subject;
}


//----------------------------------------------------------------------
// MailBoxMixedSendAndReceive
// Tests mailbox by ordering a mixture of send and receive calls.
//----------------------------------------------------------------------
void MailBoxMixedSendAndReceive() {
  subject = new Mailbox("subject mailbox");
  buffer = new int();

  fprintf(stderr, "Currently sending six messages containing a distinct element ");
  fprintf(stderr, "from [0-5] \n \n");
  //send six messages starting from zero
  MultiSender(6,0);

  //give children a chance to execute
  MultiYield(40);  

  fprintf(stderr, "\nAbout to receive two messages in the range of [0-5] \n");
  //receive two messages
  MultiReceive(2,0);

  fprintf(stderr, "\nAbout to send two more messages. Expected range now  [0-7] \n");
  //send two more messages starint from six
  MultiSender(2,6);

  fprintf(stderr, "\nAbout to receive eight messages,should expect to see ");
  fprintf(stderr, "two duplicates. Range [0-7] \n \n");
  //Receive 8 messages expect two duplicates
  MultiReceive(6,0);
  MultiReceive(2,1);

  fprintf(stderr, "\nSending two messages to pair with two remaining receive");
  fprintf(stderr, " calls. Messages are the in range [0-9]");
  //two messages should be sent
  MultiSender(2,8,0);

  fprintf(stderr, "\n All integers from [0-9] should have been printed \n"); 
  delete subject;
}


//----------------------------------------------------------------------
// JoinTest
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// BusyWork
// Helper method in which child performs some busy work
//
// pram is the number of times it supposed to execute some task
//----------------------------------------------------------------------
void BusyWork(int pram) {
  fprintf(stderr, "Child is about to do execute some print statements \n");
  fprintf(stderr, "Yielding to parent if join functions, child should remain");
  fprintf(stderr, " in control \n");

  //Prints string if join works then yield should have no effect
  for(int i=0;i<pram;i++) {
    fprintf(stderr, "I am child, kuppo \n");
    MultiYield(pram);
  }

  fprintf(stderr, "Method about to terminate control is to be handed to parent \n");
}


//----------------------------------------------------------------------
// No_OpStub
// Helper method accomplishes nothing simply done to pass something into
// fork
//
// pram is there to satisfy fork requirment
//----------------------------------------------------------------------
void noOpStub(int pram) { }


//----------------------------------------------------------------------
// SelfJoin
// Helper method in which a thread calls join on itself should lead
// to chrash
//----------------------------------------------------------------------
void SelfJoin(int pram) {
  currentThread->Join();
}


//----------------------------------------------------------------------
// Joiner
// Helper thread that manages parent thread for forkerThread function
//
// joinee pointer to child thread used to call join
//----------------------------------------------------------------------
void Joiner(Thread *joinee) {
  currentThread->Yield();
  currentThread->Yield();

  fprintf(stderr, "Waiting for the Joinee to finish executing.\n");

  currentThread->Yield();
  currentThread->Yield();

  // Note that, in this program, the "joinee" has not finished
  // when the "joiner" calls Join().  You will also need to handle
  // and test for the case when the "joinee" _has_ finished when
  // the "joiner" calls Join().

  joinee->Join();

  currentThread->Yield();
  currentThread->Yield();

  fprintf(stderr, "Joinee has finished executing, we can continue.\n");

  currentThread->Yield();
  currentThread->Yield();
}


//----------------------------------------------------------------------
// Joinee
// Helper function that prints out statmenets for child thread
// Once joined has been called  
//----------------------------------------------------------------------
void Joinee() {
  int i;

  for (i = 0; i < 5; i++) {
    fprintf(stderr, "Smell the roses.\n");
    currentThread->Yield();
  }

  currentThread->Yield();
  fprintf(stderr, "Done smelling the roses!\n");
  currentThread->Yield();
}


//----------------------------------------------------------------------
// ForkerThread
// Tests that join works by creating a parent and child and making
// each execute print statements that will be examined by the tester.
//----------------------------------------------------------------------
void ForkerThread() {
  Thread *joiner = new Thread("joiner", 0);  // will not be joined
  Thread *joinee = new Thread("joinee", 1);  // WILL be joined

  // fork off the two threads and let them do their business
  joiner->Fork((VoidFunctionPtr) Joiner, (int) joinee);
  joinee->Fork((VoidFunctionPtr) Joinee, 0);

  // this thread is done and can go on its merry way
  fprintf(stderr, "Forked off the joiner and joiner threads.\n");
}


//----------------------------------------------------------------------
// TestJoinOnlyAfterFork
// Tests that system crashs when join is called on a thread that has
// not forked the system crashs.
//----------------------------------------------------------------------
void TestJoinOnlyAfterFork() {
  //child will crash if fork has not been called
  Thread * child = new Thread("Join on unforked thread",1);

  fprintf(stderr, "Calling join on a thread that has not been forked \n");
  fprintf(stderr, "System should crash \n");

  child->Join();
}


//----------------------------------------------------------------------
// TestNonJoinableCrashs
// Tests that system crashs when constructor does not specify that 
// child is joinable
//----------------------------------------------------------------------
void TestNonJoinableCrashs () {
  //Thread is not joinable
  Thread * child = new Thread("Thread is not joinable ",0);

  fprintf(stderr, "Thread is not joinable, should crash when join is called \n");

  child->Fork(noOpStub,0);
  child->Join();
}


//----------------------------------------------------------------------
// TestThatJoinCallableOnce
// Tests that join can only be called once.
//----------------------------------------------------------------------
void TestThatJoinCallableOnce() {
  Thread * child = new Thread("Thread is joinable ",1); 

  fprintf(stderr, "Forking child \n");

  child->Fork(noOpStub,0); 

  fprintf(stderr, "Calling join for the first time \n");
  child->Join();

  fprintf(stderr, "Calling join for the second time,should crash \n");

  child->Join();
}


//----------------------------------------------------------------------
// TestThatJoinCannotBeCalledOnself
// Tests that a thread calling join on itself induces a crash
//----------------------------------------------------------------------
void TestThatJoinCannotBeCalledOnself() {
  Thread * child = new Thread("Is joinable ",1);

  fprintf(stderr, "Child thread is calling join on itself expect crash \n");
  child->Fork(SelfJoin,0);
}

//----------------------------------------------------------------------
// TestThatParentsBlocking
// Tests that after child thread terminates parent stops blocking
//----------------------------------------------------------------------
void TestThatParentsBlocking() {
  Thread * child = new Thread("Is joinable",1);

  child->Fork(BusyWork,20);

  child->Join();
  fprintf(stderr, "Child has terminated, parent now in control\n");
}


//----------------------------------------------------------------------
// TestTerminatesOnlyWhenJoinCalled
// Tests that a child fully deletes itself only after the parent calls
// join
//----------------------------------------------------------------------
void TestTerminatesOnlyWhenJoinCalled() {
  Thread * child = new Thread("I am child",1);
  fprintf(stderr, "Testing that child terminates only after parent calls join\n ");
  fprintf(stderr, "Please perform in DEBUG mode: ./nachos -d t -q 27\n");

  child->Fork(noOpStub,0);
  MultiYield(1);
  fprintf(stderr, "Child has been completed should run to termination but not be ");
  fprintf(stderr, "deleted\n");

  fprintf(stderr, "\nParent is about to call join and allow child to delete itself\n");
  fprintf(stderr, "\n \n");
  child->Join();

  MultiYield(1);
}


//----------------------------------------------------------------------
// PriorityTest
//----------------------------------------------------------------------
Lock * priorityLock;  // Lock used to keep print statements synchronized

Condition * priorityCV; //Test that cv accounts for priority

Semaphore * prioritySema;//Test that semaphore accounts for priority

//----------------------------------------------------------------------
// task
// Helper method to print when thread was run and its priority.
//
// param there to satisfy fork requirment
//----------------------------------------------------------------------
void task(int param) {
  fprintf(stderr, "Thread with priority %d performed\n", param);
}


//----------------------------------------------------------------------
// multiTask
// Helper method in which a thread performs multiple task
//----------------------------------------------------------------------
void multiTask(int param) {
  //print multiple times,yield afterwards higher priority should remain
  //in control
  for(int i=0;i<param;i++) {
    currentThread->Print();
    fprintf(stderr, " with priority %d performed \n",currentThread->getPriority());
  }
}


//----------------------------------------------------------------------
// waitTask
// Helper method which makes thread wait on CV
//
// param the assigned as the priority of this thread
//----------------------------------------------------------------------
void waitTask(int param) {
  priorityLock->Acquire();
  //priority assigned
  currentThread->setPriority(param);
  
  priorityCV->Wait(priorityLock);
  currentThread->Print();
  fprintf(stderr, " has woken up, has priority %d \n",currentThread->getPriority());
  priorityLock->Release();
}


//----------------------------------------------------------------------
// waitOnLock
// Helper method which makes thread wait for lock
//
// param the priority to be assigned to this thread
//----------------------------------------------------------------------
void waitOnLock(int param) {
  //priority assigned
  currentThread->setPriority(param);

  //Thread waits to acquire lock
  priorityLock->Acquire();
   
  currentThread->Print();

  fprintf(stderr, " has acquired  lock, has priority %d \n",currentThread->getPriority());
  priorityLock->Release();
}


//----------------------------------------------------------------------
// waitOnSema
// Helper method which makes thread wait for semaphore
//
// param the priority to be assigned to a thread
//----------------------------------------------------------------------
void waitOnSema(int param) {
  //priority assigned
  currentThread->setPriority(param);
  
  //Thread waits to acquire lock
  prioritySema->P();
  
  currentThread->Print();
  fprintf(stderr, " has acquired sema, has priority %d \n",currentThread->getPriority());
  prioritySema->V();
}

//----------------------------------------------------------------------
// Priority
// Used to test whether higher priority threads run before lower
// priority threads or not.
//----------------------------------------------------------------------
void PriorityTest() {
  Thread * thread;

  priorityLock = new Lock("Priority Lock");
  priorityLock->Acquire();

  fprintf(stderr, "Threads should run from highest priority to lowest");
  fprintf(stderr, " priority, provided that they have already been forked.\n");

  int value = 1;

  for(int i = 0; i < 10; i++) {
    thread = new Thread("Thread");
    
    int priority = value * i;
    value = -value;

    thread->setPriority(priority);
    
    thread->Fork(task, priority);
    fprintf(stderr, "Thread with priority %d forked\n", priority);
  }

  priorityLock->Release();

  delete priorityLock;
}


//----------------------------------------------------------------------
// TestThatMaxPriorityAlwaysTop
// Tests that the thread with the maximum priority will remain at the
// top of the scheduler
//
//----------------------------------------------------------------------

void TestThatMaxPriorityAlwaysTop() {
  Thread * child1 = new Thread("Child with priority 10");
  Thread * child2 = new Thread("Child with priority 2");  
  Thread * child3 = new Thread("Child with priority 0");  

  child1->setPriority(10);
  child2->setPriority(2);
  child3->setPriority(0);
  
  int taskSize = 3;
  //fork all three children 
  child3->Fork(multiTask,taskSize);
  child2->Fork(multiTask,taskSize);
  child1->Fork(multiTask,taskSize);
  
  fprintf(stderr, "Have spawned three children,each thread should print 3 times, ");
  fprintf(stderr, " uninterupted from greatest to least priority \n");

  //give children time to spawn
  currentThread->Yield();
}


//----------------------------------------------------------------------
// TestPriorityWithConditionVariable
// Tests that thread with greatest priority is the first woken up
// with condition variables
//
//----------------------------------------------------------------------
void TestPriorityWithConditionVariable() {
  Thread * child;
  int threadsSpawn=3;

  priorityLock = new Lock("Priority Lock ");
  priorityCV = new Condition("Priority CV ");

  //Place threads from priority 0 to 2 on condition
  for(int i=0;i<threadsSpawn;i++) {
    child = new Thread("Thread child ");
    child->Fork(waitTask,i);
  }
  
  MultiYield(threadsSpawn);

  //wake CV threads up
  fprintf(stderr, "Threads should print from greatest to least \n");
  for(int i=0;i<threadsSpawn;i++) {
      priorityLock->Acquire();
      priorityCV->Signal(priorityLock);
      priorityLock->Release();
      MultiYield(2);
  }
}


//----------------------------------------------------------------------
// TestPriorityWithLock
// Tests that locks account for priority
//
//----------------------------------------------------------------------
void TestPriorityWithLock() {
  Thread * child;
  int threadsSpawn=3;

  priorityLock = new Lock("Priority Lock ");

  //Acquire Lock
  fprintf(stderr, "Driver thread acquiring lock and spawning threads\n");
  priorityLock->Acquire();

  //Place threads from priority 0 to 2 on Lock
  for(int i=0;i<threadsSpawn;i++) {
    child = new Thread("Thread child %d ",i);
    child->Fork(waitOnLock,i);
  
  }
  
  MultiYield(threadsSpawn);   
  priorityLock->Release();
  //wake up threads
  fprintf(stderr, "Threads should print from greatest to least \n");
  MultiYield(threadsSpawn);
}


//----------------------------------------------------------------------
// TestPriorityWithSemaphore
// Tests that semaphores account for priority
//----------------------------------------------------------------------
void TestPriorityWithSemaphore() {
  //Binary Semaphore will be basically treated as lock
  prioritySema = new Semaphore("Priority Semaphore",1);
  
  Thread * child;
  int threadsSpawn=3;

  //Acquire Sema
  fprintf(stderr, "Driver thread acquiring semaphore  and spawning threads\n");
  prioritySema->P();

  //Place threads from priority 0 to 2 on Lock
  for(int i=0;i<threadsSpawn;i++) {
    child = new Thread("Thread child ");
    child->Fork(waitOnSema,i);
  }

  //Give children time to execute P 
  MultiYield(threadsSpawn);   
  prioritySema->V();

  //wake up threads
  fprintf(stderr, "Threads should print from greatest to least \n");
  MultiYield(threadsSpawn);
}


//----------------------------------------------------------------------
// WhaleTest
//----------------------------------------------------------------------
Whale * whale;      // Whale created dynamically.


//----------------------------------------------------------------------
// callMale
// Helper method to call the Male method in a separate thread
//----------------------------------------------------------------------
void callMale(int param) {
  whale->Male();
}


//----------------------------------------------------------------------
// callFemale
// Helper method to call the Female method in a separate thread
//----------------------------------------------------------------------
void callFemale(int param) {
  whale->Female();
}


//----------------------------------------------------------------------
// callMatchmaker
// Helper method to call the Matchmaker method in a separate thread
//----------------------------------------------------------------------
void callMatchmaker(int param) {
  whale->Matchmaker();
}


//----------------------------------------------------------------------
// MultiMale
// Helper method to create multiple Female threads
//
// numberofMales represents number of instances created in
// separate threads
//----------------------------------------------------------------------
void MultiMale(int numberOfMales) {
  Thread * male;

  fprintf(stderr, "Creating %d male(s)\n", numberOfMales);

  for(int i = 0; i < numberOfMales; i++) {
    male = new Thread("male");

    male->Fork(callMale, 0);

    MultiYield(40);
  }
}


//----------------------------------------------------------------------
// MultiFemale
// Helper method to create multiple Male threads
//
// numberofFemales represents number of instances created in
// separate threads
//----------------------------------------------------------------------
void MultiFemale(int numberOfFemales) {
  Thread * female;

  fprintf(stderr, "Creating %d female(s)\n", numberOfFemales);

  for(int i = 0; i < numberOfFemales; i++) {
    female = new Thread("female");
    female->setPriority(100);
    female->Fork(callFemale, 0);
    MultiYield(40);
  }
}


//----------------------------------------------------------------------
// MultiMatchmaker
// Helper method to create multiple Matchmaker threads
//
// numberofMatchMakers represents number of instances created in
// separate threads
//----------------------------------------------------------------------
void MultiMatchmaker(int numberOfMatchmakers) {
  Thread * matchmaker;

  fprintf(stderr, "Creating %d matchmaker(s)\n", numberOfMatchmakers);

  for(int i = 0; i < numberOfMatchmakers; i++) {
    matchmaker = new Thread("matchmaker");
    matchmaker->Fork(callMatchmaker, 0);
    MultiYield(40);
  }
}


//----------------------------------------------------------------------
// WhaleTest
// Tests multiple orderings of Male, Female and Matchmakers
//----------------------------------------------------------------------
void WhaleTest() {
  whale = new Whale("Whale");

  fprintf(stderr, "Calling Various types of whales in different order.\n");
  fprintf(stderr, "The whales return whenever a successful match is formed.\n");

  MultiMale(3);
  MultiFemale(1);
  MultiMatchmaker(2);

  MultiFemale(3);
  MultiMatchmaker(2);
  MultiMale(1);
  
  MultiYield(40);

  delete whale;
}


//----------------------------------------------------------------------
// MaleCrash
// Tests that deleting when Male is waiting, aborts the system
//----------------------------------------------------------------------
void MaleCrash() {
  whale = new Whale("Whale");
  MultiMale(1);
  fprintf(stderr, "Trying to delete when a male is waiting. Should Crash.\n");
  delete whale;
}


//----------------------------------------------------------------------
// FemaleCrash
// Tests that deleting when Female is waiting, aborts the system
//----------------------------------------------------------------------
void FemaleCrash() {
  whale = new Whale("Whale");
  MultiFemale(1);
  fprintf(stderr, "Trying to delete when a female is waiting. Should Crash.\n");
  delete whale;
}

//----------------------------------------------------------------------
// MatchmakerCrash
// Tests that deleting when Matchmaker is waiting, aborts the system
//----------------------------------------------------------------------
void MatchmakerCrash() {
  whale = new Whale("Whale");
  MultiMatchmaker(1);
  fprintf(stderr, "Trying to delete when a matchmaker is waiting. Should Crash.\n");
  delete whale;
}


//----------------------------------------------------------------------
// WhaleDeleteEmpty
// Tests that deleting an empty Whale doesn't crash the system
//----------------------------------------------------------------------
void WhaleDeleteEmpty() {
  whale = new Whale("Whale");
  fprintf(stderr, "Deleting empty Whale. Should work.\n");
  delete whale;
}


//----------------------------------------------------------------------
// ThreadTest
//  Invoke a test routine.
//----------------------------------------------------------------------
void ThreadTest() {
  switch (testnum) {
    case 1: ThreadTest1();
            break;
    
    case 2: LockTest1();
            break;
    
    case 3: LocktestHolder_Reacquires();
            break;
    
    case 4: Locktest_ReleaseWithout_Held();
            break;
    
    case 5: Locktest_ReleaseWithout_Held2();
            break;
    
    case 6: LockTest_DeleteLockThatIsHeld();
            break;
    
    case 7: ConditionTest_NotHoldingLock();
            break;
    
    case 8: ConditionTest_DeleteWaitTest();
            break;
    
    case 9: Condition_Test_Signal_WakesUpOne();
            break;
    
    case 10:  Condition_Test_BroadCastWakesAll();
              break;
    
    case 11:  Condition_Test_Signal_Lost();
              break;
    
    case 12:  Condition_Test_BroadCast_Lost();
              break;
    
    case 13:  Condition_Test_GivesUpLockInWait();
              break;
    
    case 14:  MailBoxSimpleTest();
              break;
    
    case 15:  MailBoxMultiSendersWaiting();
              break;
    
    case 16:  MailBoxMultiReceiveWaiting();
              break;
    
    case 17:  MailBoxCrashsWhenDeleted();
              break;
    
    case 18:  MailBoxCrashsWhenDeleted2();
              break;
    
    case 19:  MailBoxDeleteEmpty();
              break;
    
    case 20:  MailBoxMixedSendAndReceive();
              break;
    
    case 21:  ForkerThread();
              break;
    
    case 22:  TestJoinOnlyAfterFork();
              break;
    
    case 23:  TestNonJoinableCrashs();
              break;
    
    case 24:  TestThatJoinCallableOnce();
              break;
    
    case 25:  TestThatJoinCannotBeCalledOnself();
              break;
    
    case 26:  TestThatParentsBlocking();
              break;
    
    case 27:  TestTerminatesOnlyWhenJoinCalled();
              break;
    
    case 28:  PriorityTest();
              break;

    case 29:  TestThatMaxPriorityAlwaysTop();
              break;
             
    case 30:  TestPriorityWithConditionVariable();
              break;

    case 31:  TestPriorityWithLock();
              break;

    case 32:  TestPriorityWithSemaphore();
              break;

    case 33:  WhaleTest();
              break;

    case 34:  MaleCrash();
              break;

    case 35:  FemaleCrash();
              break;

    case 36:  MatchmakerCrash();
              break;

    default:  fprintf(stderr, "No test specified.\n");
              break;
  }
}

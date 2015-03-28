// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define ArgumentSize    512  // Space allocated for command line arguments
#define UserStackSize		1024 // Space allocated for each thread stack

extern int MAXPROCESS;

class Lock;
class List;
class BitMap;
class BackingStore;
class CorePage;
class CoreMap;

class AddrSpace {
public:
    AddrSpace(OpenFile *executable);	// Create an address space,
    // initializing it with the program
    // stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
    // before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch
    int Initialize(OpenFile *executable); // load data into memory

    void AddArguments(int argCount, char ** args);  // Add arguments
    int LoadArguments();                       // Load arguments

    int allocateThreadSpace();  // allocate more memory for thread stack

    TranslationEntry * getPageTable(); // Getter for pageTable
    unsigned int getNumPages(); // Getter for numPages

    int getThreadCount(); // Getter for threadCount
    void setThreadCount(int value);

    int allocVirtualPage(int virtualAddr); // allocated page mem

    void writeMemoryToPage(int vpn, int written, int inFileAddr, int offset);

    BackingStore * getBackingStore();
    void setBackingStore(BackingStore * value);

    int getStoreID();
    void setStoreID(int value);

private:
    TranslationEntry *pageTable;	// Assume linear page table translation
    // for now!
    unsigned int numPages;		// Number of pages in the virtual
    // address space

    int argc;               // Number of arguments
    char ** argv;           // Array of arguments
    
    OpenFile * currentExecutable; // keep track of current executable

    int threadCount;

    BackingStore * backingStore;
    int storeID;
};


class BackingStore {
  private:
    BitMap * pages;
    AddrSpace * space;
    char * filename;

    int storeID;
    char * charID;

    int flag;

  public:
    BackingStore();
    ~BackingStore();

    int Initialize(AddrSpace * addrspace, int ID);

    void pageOut(int virtualPage);
    void pageIn(int virtualPage);

    int contains(int virtualPage);
    
    AddrSpace * getSpace();

    int getStoreID();
    char * getCharID();
};


class MemoryManager {
public:

  /* Create a manager to track the allocation of numPages of physical memory.  
   You will create one by calling the constructor with NumPhysPages as
   the parameter.  All physical pages start as free, unallocated pages. */
  MemoryManager(int numPages);

  ~MemoryManager();

  /* Allocate a free page, returning its physical page number or -1
   if there are no free pages available. */
  int AllocPage();
  
  /* Free the physical page and make it available for future allocation. */
  void FreePage(int physPageNum);

  /* True if the physical page is allocated, false otherwise. */
  bool PageIsAllocated(int physPageNum);

private:
  Lock * lock;     // For synchronization
  BitMap * pages;  // keep track of which pages are available
};

class CorePage {
  private:
    CorePage * next;
    BackingStore * backingStore;
    int physicalPage;
    int virtualPage;

  public:
    CorePage(BackingStore * bs, int physPage, int virtPage);
    ~CorePage();

    BackingStore * getBackingStore();
    int getPhysicalPage();
    int getVirtualPage();

    void setNext(CorePage * corePage);
    CorePage * getNext();
};


class CoreMap {
  private:
    CorePage * head;
    CorePage * tail;

    int size;
    int capacity;

  public:
    CoreMap(int physPages);
    ~CoreMap();

    int addCorePage(CorePage * corePage);
    CorePage * evictCorePage();

    void evictAll(BackingStore * backingStore);

    int isFull();
};

#endif // ADDRSPACE_H

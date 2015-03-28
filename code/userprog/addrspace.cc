// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include "synch.h"
#include "list.h"
#include "bitmap.h"
#ifdef HOST_SPARC
#include <strings.h>
#include <machine.h>
#endif
#include "Table.h"

extern MemoryManager * memoryManager;
extern Table * storeTable;

extern Statistics * stats;

extern CoreMap * coreMap;

extern int LRU;

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader (NoffHeader *noffH) {
  noffH->noffMagic = WordToHost(noffH->noffMagic);
  noffH->code.size = WordToHost(noffH->code.size);
  noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
  noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
  noffH->initData.size = WordToHost(noffH->initData.size);
  noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
  noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
  noffH->uninitData.size = WordToHost(noffH->uninitData.size);
  noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
  noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}


//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable) {
  //fprintf(stderr, "CONSTRUCT %x\n", (unsigned int) this);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace() {
  //fprintf(stderr, "DESTRUCT %x\n", (unsigned int) this);
  //delete backingStore;
  //fprintf(stderr, "pageTable before: %x\n", (unsigned int)pageTable);
  delete [] pageTable;
  pageTable = 0;
 // fprintf(stderr, "pageTable after: %x\n", (unsigned int)pageTable);
  delete [] argv;
  delete currentExecutable;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int AddrSpace::Initialize(OpenFile *executable) {
  NoffHeader noffH;
  unsigned int i, size;
  
  executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
  if ((noffH.noffMagic != NOFFMAGIC) &&
      (WordToHost(noffH.noffMagic) == NOFFMAGIC)) {
    SwapHeader(&noffH);
  }
  if(noffH.noffMagic != NOFFMAGIC) {
    fprintf(stderr, "Couldn't Initialize Process, NoffMagic value incorrect\n");
    return 1;
  }

  // how big is address space?
  size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
    + UserStackSize + ArgumentSize;	// we need to increase the size
  // to leave room for the stack

  numPages = divRoundUp(size, PageSize);
  size = numPages * PageSize;
 
  DEBUG('a', "Initializing address space, num pages %d, size %d\n",
      numPages, size);
  // first, set up the translation
  pageTable = new TranslationEntry[numPages];
  for (i = 0; i < numPages; i++) {
    pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
    pageTable[i].physicalPage = -2;
    pageTable[i].valid = FALSE;
    pageTable[i].use = FALSE;
    pageTable[i].dirty = FALSE;
    pageTable[i].readOnly = FALSE;  // if the code segment was entirely on
    // a separate page, we could set its
    // pages to be read-only
  }

  currentExecutable = executable; 
  return 0;
} 

//----------------------------
// writeMemoryToPage - write Memory to page
//----------------------------
void AddrSpace::writeMemoryToPage(int vpn, int written, int inFileAddr, int offset) {
  currentExecutable->ReadAt(&(machine->mainMemory[(pageTable[vpn].physicalPage * PageSize) + written]),
  1,inFileAddr + offset);
}

//----------------------------
// allocVirtualPage - allocates virtual page
//----------------------------
int AddrSpace::allocVirtualPage(int virtualAddr) {
  NoffHeader noffH;

  currentExecutable->ReadAt((char *)&noffH, sizeof(noffH), 0);
  if ((noffH.noffMagic != NOFFMAGIC) &&
      (WordToHost(noffH.noffMagic) == NOFFMAGIC)) {
    SwapHeader(&noffH);
  }
  if(noffH.noffMagic != NOFFMAGIC) {
    fprintf(stderr, "Couldn't Initialize Process, NoffMagic value incorrect\n");
    return 1;
  }

  unsigned int vpn = (unsigned int) virtualAddr / PageSize;
  
  int allocpage = memoryManager->AllocPage();
  coreMap->addCorePage(new CorePage(backingStore, allocpage, vpn));
  pageTable[vpn].physicalPage = allocpage;
  //fprintf(stderr, "virt page %d mapped to phys page %d \n",vpn,allocpage);

  unsigned int vpAddr = vpn * PageSize;
  
  int written = 0;

  bzero(machine->mainMemory + (pageTable[vpn].physicalPage * PageSize),
                               PageSize);
 
  int flag = 0;

  if(!(backingStore->contains(vpn))) {
    int lowerCode = noffH.code.virtualAddr;
    int upperCode = noffH.code.virtualAddr + noffH.code.size;
    
    int lowerData = noffH.initData.virtualAddr;
    int upperData = noffH.initData.virtualAddr + noffH.initData.size;

    while(written < PageSize) {
      int currAddress = written + vpAddr;

      //Check if code section must be written
      if(noffH.code.size > 0 && currAddress < upperCode
                             && currAddress >= lowerCode) {
        flag = 1;
        writeMemoryToPage(vpn, written, noffH.code.inFileAddr,
                          currAddress - lowerCode);
      }  
      //Check if data section must be written 
      else if(noffH.initData.size > 0 && currAddress < upperData
                                      && currAddress >= lowerData) {
        flag = 1;
        writeMemoryToPage(vpn, written, noffH.initData.inFileAddr,
                          currAddress - lowerData);
      }
      
      written++;
    }
    if(flag == 1) {
      stats->numPageIns = stats->numPageIns + 1;
    }
  }
  else {
    backingStore->pageIn(vpn);
    stats->numPageIns = stats->numPageIns + 1;
  }
  pageTable[vpn].valid = TRUE;   
  return 0;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters() {
  int i;

  for (i = 0; i < NumTotalRegs; i++)
    machine->WriteRegister(i, 0);

  // Initial program counter -- must be location of "Start"
  machine->WriteRegister(PCReg, 0);

  // Need to also tell MIPS where next instruction is, because
  // of branch delay possibility
  machine->WriteRegister(NextPCReg, 4);

  // Set the stack register to the end of the address space, where we
  // allocated the stack; but subtract off a bit, to make sure we don't
  // accidentally reference off the end!
  machine->WriteRegister(StackReg, numPages * PageSize - 16);
  DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() {
  machine->pageTable = pageTable;
  machine->pageTableSize = numPages;
}


/*
 * Add the arguments to be used by the forked process
 */
void AddrSpace::AddArguments(int argCount, char ** args) {
  argc = argCount;
  argv = args;
}

int checkSP(int start, int end) {
  return end-start;
}

/*
 * Procedure to load the arguments passed into the Exec'ed process.
 */
int AddrSpace::LoadArguments() {
  // stack pointer adjustor to fill in arguments
  int sp = numPages * PageSize;

  int end = sp;
  
  // Writing the strings into memory
  int arr[argc];
  for(int i = 0; i < argc; i++) {
    char * temp = argv[i];
    int j = 0;
    for(j = 0; temp[j] != 0; j++) {
    }

    sp -= j+1;

    if(!checkSP(sp, end)) {
      return 0;
    }

    for(int k = 0; k <= j; k++) {
      machine->WriteMem(sp+k, 1, (int)temp[k]);
    }

    arr[i] = sp;
  }

  sp -= (argc * 4);
  sp -= (sp % 4);

  if(!checkSP(sp, end)) {
    return 0;
  }

  machine->WriteRegister(StackReg, sp-16);
  machine->WriteRegister(4,argc);
  machine->WriteRegister(5,sp);

  for(int i = 0; i < argc; i++) {
    machine->WriteMem(sp+(i*4),4, arr[i]);
  }

  return 1;
}


/*
 * Getter for pageTable
 */
TranslationEntry * AddrSpace::getPageTable() {
  return pageTable;
}


/*
 * Getter for numPages
 */
unsigned int AddrSpace::getNumPages() {
  return numPages;
}


/*
 * Getter for threadCount
 */
int AddrSpace::getThreadCount() {
  return threadCount;
}


/*
 * Setter for threadCount
 */
void AddrSpace::setThreadCount(int value) {
  threadCount = value;
}


/*
 * Allocates more space for the forked thread
 */
int AddrSpace::allocateThreadSpace() {
  int newPages = divRoundUp(UserStackSize, PageSize);

  TranslationEntry * newTable = new TranslationEntry[numPages + newPages];

  for(unsigned int i = 0; i < numPages; i++) {
    newTable[i] = pageTable[i];
  }

  int allocpage;

  for (unsigned int i = numPages; i < numPages + newPages; i++) {
    newTable[i].virtualPage = i;

    if((allocpage = memoryManager->AllocPage()) == -1) {
      for (unsigned int j = numPages; j < i; j++) {
        fprintf(stderr, "Physical Page: %d\n", pageTable[j].physicalPage);
        fprintf(stderr, "numPages: %d\n", numPages);
        fprintf(stderr, "newPages: %d\n", newPages);
        memoryManager->FreePage(pageTable[j].physicalPage);        
      }
      fprintf(stderr, "Could not allocate memory for new thread\n");
      delete newTable;
      return 0;
    }

    newTable[i].physicalPage = allocpage;
    newTable[i].valid = TRUE;
    newTable[i].use = FALSE;
    newTable[i].dirty = FALSE;
    newTable[i].readOnly = FALSE;  // if the code segment was entirely on
    // a separate page, we could set its
    // pages to be read-only
  }

  delete pageTable;
  pageTable = newTable;

  // zero out the entire address space, to zero the unitialized data segment
  // and the stack segment
  for(unsigned int i = numPages; i < numPages + newPages; i++) {
    bzero(machine->mainMemory + (pageTable[i].physicalPage * PageSize), PageSize);
  }

  numPages = numPages + newPages;

  return 1;
}


//----------------------------
// setter for backingStore
//----------------------------
void AddrSpace::setBackingStore(BackingStore * value) {
  backingStore = value;
}


//----------------------------
// getter for backingStore
//----------------------------
BackingStore * AddrSpace::getBackingStore() {
  return backingStore;
}


//----------------------------
// gettter for backingStore
//----------------------------
void AddrSpace::setStoreID(int value) {
  storeID = value;
}


//----------------------------
// getter for storeID
//----------------------------
int AddrSpace::getStoreID() {
  return storeID;
}


// MemoryManager Class //

/*
 * MemoryManager Constructor
 */
MemoryManager::MemoryManager(int numpages) {  
  lock = new Lock("Memory Manager Lock");

  pages = new BitMap(numpages); // make map for n pages
}


/*
 * MemoryManager Destructor
 */
MemoryManager::~MemoryManager() {
  // need to know if all pages need to be cleared before deleting
  delete pages;
  pages = 0;
  delete lock;  
}

/*
 * AllocPage - Allocates a page in memory for the process
 */
int MemoryManager::AllocPage() {
  lock->Acquire();
  int page = pages->Find();  
  lock->Release();

  return page;
}


/*
 * Frees a page in memory for the process
 */
void MemoryManager::FreePage(int physPageNum) {
  lock->Acquire();
  pages->Clear(physPageNum);
  lock->Release();
}


/*
 * Checks if a page has been allocated or not
 */
bool MemoryManager::PageIsAllocated(int physPageNum) {
  lock->Acquire();  
  bool isSet = pages->Test(physPageNum);
  lock->Release();

  return isSet;
}



//----------------------------
// Constructor
//----------------------------
BackingStore::BackingStore() {
  flag = 0;
}


//----------------------------
// Destructor
//----------------------------
BackingStore::~BackingStore() {
  if(flag != 0) {
    delete filename;
    delete pages;
    storeTable->Release(storeID);
  }
}


//----------------------------
// Initialize the backing store with the address space
//----------------------------
int BackingStore::Initialize(AddrSpace * addrspace, int ID) {
  flag = 1;
  space = addrspace;
  pages = new BitMap(space->getNumPages());

  storeID = ID;

  filename = new char[15];
  filename[0] = 'a';
  filename[1] = 'j';
  filename[2] = 'n';
  filename[3] = 'r';
  filename[4] = '_';

  charID = new char[10];

  int i, j;
  for(i = 0, j = 5; ID != 0; i++, j++) {
    charID[i] = (char) ((ID % 10) + 48);
    filename[j] = charID[i];
    ID /= 10;
  }
  charID[i] = 0;
  filename[j] = 0;

  if(!fileSystem->Create(filename, space->getNumPages() * PageSize)) {
    return -1;
  }

  space->setBackingStore(this);

  return 0;
}


//----------------------------
// pageOut
//----------------------------
void BackingStore::pageOut(int virtualPage) {
  OpenFile * file = fileSystem->Open(filename);

  TranslationEntry * pte = space->getPageTable();

  if(pte == NULL) {
    return;
  }

  if(pte[virtualPage].dirty == TRUE) {
    pages->Mark(virtualPage);

    stats->numPageOuts = stats->numPageOuts + 1;
    
    file->WriteAt(&(machine->mainMemory[pte[virtualPage].physicalPage * PageSize]),
                  PageSize, virtualPage * PageSize);
  }

  pte[virtualPage].valid = FALSE;
  pte[virtualPage].dirty = FALSE;
  delete file;
}

void BackingStore::pageIn(int virtualPage) {
  OpenFile * file = fileSystem->Open(filename);

  TranslationEntry * pte = space->getPageTable();

  file->ReadAt(&(machine->mainMemory[pte[virtualPage].physicalPage * PageSize]),
               PageSize, virtualPage * PageSize);

  pte[virtualPage].valid = TRUE;
  delete file;
}


//----------------------------
// check whether it's been written or not
//----------------------------
int BackingStore::contains(int virtualPage) {
  if(pages->Test(virtualPage)) {
    return 1;
  }
  else {
    return 0;
  }
}


//----------------------------
// getter for space
//----------------------------
AddrSpace * BackingStore::getSpace() {
  return space;
}

//----------------------------
// getter for storeID
//----------------------------
int BackingStore::getStoreID() {
  return storeID;
}


//----------------------------
// setter for StoreID
//----------------------------
char * BackingStore::getCharID() {
  return charID;
}


//----------------------------
// Constructor for CorePage
//----------------------------
CorePage::CorePage(BackingStore * bs, int physPage, int virtPage) {
  backingStore = bs;
  physicalPage = physPage;
  virtualPage = virtPage;

  next = NULL;
}


//----------------------------
//  Destrctor for CorePage
//----------------------------
CorePage::~CorePage() {
  // Doesn't do anything and shouldn't for now
}


//----------------------------
//  getter for backingStore
//----------------------------
BackingStore * CorePage::getBackingStore() {
  return backingStore;
}


//----------------------------
// getter for physicalPage
//----------------------------
int CorePage::getPhysicalPage() {
  return physicalPage;
}


//----------------------------
// getter for virtualPage
//----------------------------
int CorePage::getVirtualPage() {
  return virtualPage;
}


//----------------------------
// setter for next
//----------------------------
void CorePage::setNext(CorePage * corePage) {
  next = corePage;
}


//----------------------------
// getter for next
//----------------------------
CorePage * CorePage::getNext() {
  return next;
}



//----------------------------
// Constructor for CoreMap
//----------------------------
CoreMap::CoreMap(int physPages) {
  size = 0;
  capacity = physPages;

  head = NULL;
  tail = NULL;
}


//----------------------------
// Destructor for CoreMap
//----------------------------
CoreMap::~CoreMap() {
  // Shouldn't have anything
}


//----------------------------
// add a CorePage
//----------------------------
int CoreMap::addCorePage(CorePage * corePage) {
  if(size == capacity) {
    // error code
    return 1;
  }

  size++;

  if(head == NULL) {
    head = corePage;
    tail = corePage;

    return 0;
  }

  tail->setNext(corePage);
  tail = tail->getNext();

  return 0;
}


//----------------------------
// evict a CorePage
//----------------------------
CorePage * CoreMap::evictCorePage() {
  if(size == 0) {
    //error code
    return NULL;
  }

  size--;
  CorePage * corePage = NULL;

  if(head == tail) {
    corePage = head;
    head = NULL;
    tail = NULL;
    return corePage;
  }

  if(LRU == 0) {
    corePage = head;
    head = head->getNext();

    return corePage;
  }
  else {
    TranslationEntry * pte = head->getBackingStore()->getSpace()->getPageTable();
    
    if(pte[head->getVirtualPage()].use == FALSE) {
      corePage = head;
      head = head->getNext();
      return corePage;
    }
    else {
      pte[head->getVirtualPage()].use = FALSE;
    }
    
    CorePage * travel = head;
    CorePage * next = head->getNext();
    
    while(next != NULL) {
      pte = next->getBackingStore()->getSpace()->getPageTable();
      if(pte[next->getVirtualPage()].use == TRUE) {
        pte[next->getVirtualPage()].use = FALSE;
        travel = travel->getNext();
        next = next->getNext();
      }
      else {
        corePage = next;
        travel->setNext(next->getNext());
        return corePage;
      }
    }

    corePage = head;
    head = head->getNext();
    return corePage;
  }
}


//----------------------------
// evict all Core Pages that match a particular backingStore
//----------------------------
void CoreMap::evictAll(BackingStore * bs) {
  while(head != NULL && head->getBackingStore() == bs) {
    memoryManager->FreePage(head->getPhysicalPage());
    head = head->getNext();
    size--;
  }

  if(head == NULL) {
    tail = NULL;
    return;
  }

  CorePage * travel = head;
  CorePage * next = head->getNext();

  while(next != NULL) {
    if(next->getBackingStore() == bs) {
      memoryManager->FreePage(next->getPhysicalPage());
      travel->setNext(next->getNext());
      next = travel->getNext();
      size--;
    }
    travel = travel->getNext();

    if(next != NULL) {
      next = next->getNext();
    }
  }
}


//----------------------------
// Check if CoreMap is Full
//----------------------------
int CoreMap::isFull() {
  if( size == capacity) {
    return 1;
  }
  else {
    return 0;
  }
}

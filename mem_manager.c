#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "scanner.h"    //For reading the input file
#include "dll.h"        //For the LRU page replacement

#define PROGRAM_MEMORY_SIZE 65536               //Size of the "program" in bytes
#define ALLOCATED_MEMORY 65536/2                //Size of the memory allocated to the "program" in bytes
#define PAGE_SIZE 256                           //Size of each page in bytes
#define TLB_SIZE 16                             //Number of entries in the TLB
#define NUM_BUFFER 100
#define ADDR_BUFFER 100
#define PROGRAM_LOCATION "BACKING_STORE.bin"    //Directory of the "program"

/*
 * Can hold the information of a logical or physical address.
 * Location holds the page/frame number, offset holds the offset.
 */
typedef struct address {
    unsigned int location, offset;
} ADDRESS;
static ADDRESS *newADDRESS(unsigned int pf, unsigned int o) {
    ADDRESS *a = malloc(sizeof(ADDRESS));
    assert(a != 0);

    a->location = pf;
    a->offset = o;
    return a;
}

/*
 * Contains a char array to simulate holding an array of bytes.
 * Also holds an integer for its identifying number.
 */
typedef struct page {
    int number;
    char *content;
} PAGE;
static PAGE *newPAGE(int n, char *c) {
    PAGE *p = malloc(sizeof(PAGE));
    assert(p != 0);

    p->number = n;
    p->content = c;
    //printf("Created page %d\n", n);
    return p;
}
static void freePAGE(PAGE *p) {
    free(p->content);
    //printf("Freeing page %d...\n", p->number);
    free(p);
}
static void displayPage(void *v, FILE *fp) {
    PAGE *p = v;
    if(p)
        fprintf(fp, "PageNum: %d", p->number);
    else
        fprintf(fp, "NULL PAGE");
}

/*
 * Contains the page number and frame number of a single entry in the TLB
 * table.
 */
typedef struct tlb_entry {
    int pageNum, frameNum;
} TLB_ENTRY;
TLB_ENTRY *newTLB_ENTRY(int p, int f) {
    TLB_ENTRY *t = malloc(sizeof(TLB_ENTRY));
    assert(t != 0);

    t->pageNum = p;
    t->frameNum = f;
    return t;
}

/*
 * Holds the mappings from pages to frames.
 * page_to_frame has indexes that relate to the page numbers. The integer stored
 * at that index relates to its frame number. If the number is -1, then that
 * page is not currently in memory.
 */
typedef struct page_table {
    int *page_to_frame;
    int numPages, numFrames, freeFrames;
} PAGE_TABLE;
static PAGE_TABLE *newPAGE_TABLE(int numP, int numF) {
    PAGE_TABLE *p = malloc(sizeof(PAGE_TABLE));
    assert(p != 0);

    p->page_to_frame = malloc(sizeof(int) * numP);
    assert(p->page_to_frame != 0);
    for(int i=0; i<numP; i++) {
        p->page_to_frame[i] = -1;   //Initialize each value to the unmapped value -1
    }

    p->numPages = numP;
    p->numFrames = numF;
    p->freeFrames = numF;
    return p;
}
static int getFrameNumber(PAGE_TABLE *table, int pageNum) {
    return table->page_to_frame[pageNum];
}
static void addPageTableEntry(PAGE_TABLE *table, int pageNum, int frameNum) {
    table->page_to_frame[pageNum] = frameNum;
    table->freeFrames--;
}
static void removePageTableEntry(PAGE_TABLE *table, int pageNum) {
    if(table->page_to_frame[pageNum] == -1)
        fprintf(stderr, "Error, attempting to remove an entry with page number %d.\n", pageNum);
    table->page_to_frame[pageNum] = -1;
    table->freeFrames++;
}

static void initializeTables();
static void initializeStats();
static int *readNumbers(FILE *fp);
static ADDRESS **parseAddresses(int *nums);
static void reportValues(ADDRESS **logicalAddr);
static ADDRESS *translateAddress(ADDRESS *addr);
static unsigned int getByte(ADDRESS *addr);
static void reportStats();

static ADDRESS *lookupTLB(ADDRESS *addr);
static ADDRESS *lookupPageTable(ADDRESS *addr);
static ADDRESS *loadPage(ADDRESS *addr);

static void pushPageLRU(PAGE *p);
static void updatePageLRU(PAGE *p);
static PAGE *popPageLRU();

static void addTLBEntry(TLB_ENTRY *e);
static int findTLBEntry(int pageNum);

PAGE *frames[ALLOCATED_MEMORY/PAGE_SIZE];   //Stores the pages in "memory"(frames)
TLB_ENTRY *tlb[TLB_SIZE];                   //Stores the tlb entries in a small table(array)
int sizeTLB, oldestTLB;
PAGE_TABLE *pageTable;                      //Stores the mappings of all pages in "memory"(frames)
DLL *pageStack;                             //Stores all pages in order of usage(0 - recent, maxSize - least)
int numPageAccesses, numPageFaults, numTLBLookups, numTLBHits;  //Various statistics

/*
 * Created by Zach Wassynger on 14 April 2018.
 *
 * Created for Assignment 5 of CS426-001; "Designing a Virtual Memory
 * Manager" in OS Concepts. Reads in a file of logical addresses, and
 * returns the contents at the physical addresses(in a bin).
 */
int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Incorrect usage of parameters. Correct usage: %s <inputfile>\n", argv[0]);
        return -1;
    }

    FILE *fp = fopen(argv[1], "r");
    if(fp == NULL) {
        fprintf(stderr, "File could not be read from.\n");
        return -2;
    }

    initializeTables();
    initializeStats();
    int *nums = readNumbers(fp);
    ADDRESS **logicalAddr = parseAddresses(nums);
    reportValues(logicalAddr);

    reportStats();
    return 0;
}

/*
 * Initializes all the static variables pertaining to the tables and various
 * means of looking pages up.
 */
static void initializeTables() {
    pageTable = newPAGE_TABLE(PROGRAM_MEMORY_SIZE/PAGE_SIZE, ALLOCATED_MEMORY/PAGE_SIZE);
    pageStack = newDLL(&displayPage, NULL); //Stores pages
    sizeTLB = 0;
    oldestTLB = 0;
}
/*
 * Initializes all the static variables pertaining to statistic
 * tracking.
 */
static void initializeStats() {
    numPageAccesses = 0;
    numPageFaults = 0;
    numTLBLookups = 0;
    numTLBHits = 0;
}
/*
 * Reads from a file into a dynamically allocated array of ints.
 * Assumes the ints are delimited by whitespace(and maybe a comma).
 * Closes the file(fp) after EOF reached.
 * Returns the array of ints with a -1 denoting the end of the list.
 */
static int *readNumbers(FILE *fp) {
    int *nums = malloc(sizeof(int) * NUM_BUFFER);
    int size = 0, capacity = NUM_BUFFER;

    char *token = readToken(fp);
    while(token != NULL) {
        if(size >= capacity) {
            capacity += NUM_BUFFER;
            nums = realloc(nums, sizeof(int) * capacity);
        }
        nums[size++] = atoi(token);
        token = readToken(fp);
    }
    nums = realloc(nums, sizeof(int) * (size+1));
    nums[size] = -1;    //End the array with a -1, signifying the end
    fclose(fp);
    return nums;
}
/*
 * Parses out logical/physical addresses from an integer array.
 * Assumes that for the 16-bit integer, the 8 bits on the left are
 * the page/frame number, and the 8 bits on the right are the
 * offset. Also assumes the list of ints is terminated with a -1.
 * Returns a NULL terminated array of ADDRESS pointers.
 */
static ADDRESS **parseAddresses(int *nums) {
    ADDRESS **addr = malloc(sizeof(ADDRESS *) * ADDR_BUFFER);
    int size = 0, capacity = ADDR_BUFFER;

    unsigned int location, offset;
    int index = 0, temp = nums[index++];
    while(temp != -1) {
        offset = (unsigned int)(temp & 0x00FF);              //Mask the right 8 bits
        location = (unsigned int)((temp >> 8) & 0x00FF);     //Bit shift the int so that the left 8 bits can be accessed
        if(size >= capacity) {
            capacity += ADDR_BUFFER;
            addr = realloc(addr, sizeof(ADDRESS *) * capacity);
        }
        addr[size++] = newADDRESS(location, offset);
        temp = nums[index++];
    }
    addr = realloc(addr, sizeof(ADDRESS *) * (size+1));
    addr[size] = NULL;  //Null terminate the end of the list.
    return addr;
}
/*
 * Finds the value of each byte for each logical address provided.
 */
static void reportValues(ADDRESS **logicalAddr) {
    int index = 0;
    ADDRESS *physicalAddr;
    ADDRESS *temp = logicalAddr[index++];
    while(temp != NULL) {
        physicalAddr = translateAddress(temp);
        signed char byte = getByte(physicalAddr);
        /*
        char *byteRep = malloc(sizeof(char) * 9);   //Size of byte + 1(for null terminator)
        for(int i=0; i<8; i++) {
            byteRep[i] = byte & 1 ? '1' : '0';
            byte >>= 1;
        }
        byteRep[8] = '\0';
        */
        int combinedLogical = (temp->location << 8) | temp->offset;
        int combinedPhysical = (physicalAddr->location << 8) | physicalAddr->offset;
        printf("Virtual address: %d Physical address: %d Value: %d\n", combinedLogical, combinedPhysical, byte);

        //free(byteRep);
        temp = logicalAddr[index++];
    }
}
/*
 * Translates a given logical address into a physical address.
 * First checks the TLB for a frame, and if not found, then checks the
 * page table. If there is a page fault, then a page is loaded into memory.
 * Then the frame is coupled with the given offset and returned as an
 * address.
 */
static ADDRESS *translateAddress(ADDRESS *addr) {
    numPageAccesses++;          //Increments a stat
    ADDRESS *physicalAddr = lookupTLB(addr);
    if(physicalAddr != NULL)
        return physicalAddr;    //If TLB lookup was successful
    
    physicalAddr = lookupPageTable(addr);
    if(physicalAddr != NULL)
        return physicalAddr;    //If page table lookup was successful
    
    numPageFaults++;            //Page fault, increment stat
    return loadPage(addr);      //If page fault occurred
}
/*
 * Performs a lookup on the TLB for the given page number.
 * If a frame is found, then the address for the frame is returned.
 * If not, NULL is returned.
 */
static ADDRESS *lookupTLB(ADDRESS *addr) {
    numTLBLookups++;    //Increments a stat
    int index = findTLBEntry(addr->location);
    if(index != -1) {
        numTLBHits++;   //Increments a stat
        return newADDRESS(index, addr->offset);
    }
    return NULL;
}
/*
 * Performs a lookup on the page table for the given page number.
 * If a frame is found, then an address with the given frame and offset
 * is returned. If not, NULL is returned.
 */
static ADDRESS *lookupPageTable(ADDRESS *addr) {
    int frameNumber = getFrameNumber(pageTable, addr->location);
    if(frameNumber == -1)
        return NULL;
    addTLBEntry(newTLB_ENTRY(addr->location, frameNumber));
    return newADDRESS(frameNumber, addr->offset);
}
/*
 * In the event of a page fault, the needed page must be loaded into memory.
 * To do so, there must be space in the frame table. If there is not, a page must 
 * be swapped out in favor of the new one. Once there is room, the frame is read from
 * file and loaded into the page table and TLB.
 */
static ADDRESS *loadPage(ADDRESS *addr) {
    int index = 0;
    if(pageTable->freeFrames <= 0) {    //No space
        PAGE *lru = popPageLRU();
        if(lru == NULL) {
            fprintf(stderr, "Could not remove the LRU page, exiting...\n");
            exit(-4);
        }
        int frame = getFrameNumber(pageTable, lru->number);
        frames[frame] = NULL;
        removePageTableEntry(pageTable, lru->number);
        freePAGE(lru);
        index = frame;  //Change starting index for quicker access later in the method
    }
    for(; index<ALLOCATED_MEMORY/PAGE_SIZE; index++) {
        if(frames[index] == NULL) {
            FILE *store = fopen(PROGRAM_LOCATION, "r");
            int offset = addr->location * PAGE_SIZE;
            fseek(store, offset, SEEK_SET);
            char *data = malloc(sizeof(char) * PAGE_SIZE);
            for(int i=0; i<PAGE_SIZE; i++)
                data[i] = fgetc(store);
            
            PAGE *page = newPAGE(addr->location, data);
            pushPageLRU(page);
            frames[index] = page;
            addPageTableEntry(pageTable, addr->location, index);
            addTLBEntry(newTLB_ENTRY(addr->location, index));
            fclose(store);
            return newADDRESS(index, addr->offset);
        }
    }
    fprintf(stderr, "Error loading page - No space was found for new page.\n");
    exit(-3);   //Fatal eror - should never reach here
}
/*
 * Finds the requested byte at the given frame and offset.
 */
static unsigned int getByte(ADDRESS *addr) {
    PAGE *page = frames[addr->location];
    if(page != NULL) {
        updatePageLRU(page);
        return page->content[addr->offset];
    }
    
    fprintf(stderr, "Could not read from frame at location %d.\n", addr->location);
    return 0;
}
/*
 * Prints out the final statistics in percentage form.
 * Should there be 0/0, -1 will be reported.
 */
static void reportStats() {
    double pageFaultRate = -1;
    if(numPageAccesses != 0)
        pageFaultRate = ((double)numPageFaults)/numPageAccesses;
    double TLBHitRate = -1;
    if(numTLBLookups != 0)
        TLBHitRate = ((double)numTLBHits)/numTLBLookups;
    printf("Number of Translated Addresses = %d\n", numPageAccesses);
    printf("Page Faults = %d\n", numPageFaults);
    printf("Page Fault Rate = %f\n", pageFaultRate);
    printf("TLB Hits = %d\n", numTLBHits);
    printf("TLB Hit Rate = %f\n", TLBHitRate);
}

static void pushPageLRU(PAGE *p) {
    insertDLL(pageStack, 0, p);
}
static void updatePageLRU(PAGE *p) {
    int index = findDLL(pageStack, p);
    if(index == -1)
        fprintf(stderr, "Error in updatePageLRU; could not update the page.\n");
    else
        insertDLL(pageStack, 0, removeDLL(pageStack, index));
}
static PAGE *popPageLRU() {
    return removeDLL(pageStack, sizeDLL(pageStack)-1);
}

/*
 * Adds a TLB entry to the table. Uses FIFO replacement, so the oldest entry will be replaced
 * if the table is full.
 */
static void addTLBEntry(TLB_ENTRY *e) {
    if(sizeTLB >= TLB_SIZE) {
        if(oldestTLB >= TLB_SIZE)
            oldestTLB = 0;
        TLB_ENTRY *temp = tlb[oldestTLB];
        tlb[oldestTLB++] = e;
        free(temp);
    }
    else
        tlb[sizeTLB++] = e;
}
/*
 * Searches for the given page number in the TLB table. If a match is found, the frame
 * number is returned. If no match is found, -1 is returned.
 */
static int findTLBEntry(int pageNum) {
    TLB_ENTRY *temp;
    for(int i=0; i<sizeTLB; i++) {
        temp = tlb[i];
        if(temp->pageNum == pageNum)
            return temp->frameNum;
    }
    return -1;
}

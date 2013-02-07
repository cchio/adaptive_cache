/***
	DEVELOPER'S NOTES: 
		(i) 	think of cleverer way to update-node
		(ii)	case 3 and default of checkAndInsert, how to insert to cache
		(iii)	where to do fseek?

	IMPLEMENTATION NOTES:
		1) Elements enter the queue from the head, and exit from the tail.
***/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h> 
#include <math.h>
#include <glib.h>
#include "ssarc.h"
#include "mgmt.h"
#define CACHE_SIZE 6990506 	//also known as "C" in the SSARC paper
							//can store 6990506 nodes (each node is one sector? or larger? DESIGN DECISION)
/* NOTE: if PAGE_SIZE changes, change the bitshift factor in getFrontBlockAddr() */
#define PAGE_SIZE 8192 // the smallest quantum size of each node's address block is 8kb = 8192 bytes
#define BYTES_PER_KILOBYTE 1024
#define SECTOR_SIZE 512 //each LBA sector is 512 bytes
#define PAGE_MULTIPLE (PAGE_SIZE/SECTOR_SIZE) //i.e. how many sectors make a page
#define SIZE_PARAMETER 256 //also known as "m" in the SSARC paper
#define SIZE_OF_TWO_QUEUES (g_queue_get_length((cachePtr->recQueue)->queue) + g_queue_get_length((cachePtr->freqQueue)->queue))
#define MAX_GHOST_QUEUE_SIZE 1024 //RANDOM NUMBER!!! RETHINK THIS

//GLOBAL VARIABLES
cache* cachePtr;
//node* nodeList;
static unsigned int rec_LRU_num, rec_MRU_num, freq_LRU_num, freq_MRU_num, currProcessNum;
static double recUtility, freqUtility; 

static void printEntries(node* data, int* user_data) //user_data here will be NULL
{
	printf("%d 	%d\n", data->processNum, data->hddAddr);
}

void printQueues()
{
	GFunc printFn = (GFunc)&printEntries;
	
	printf("\nENTRIES IN RECENT QUEUE:\nline# %d\n", currLineNum);
	g_queue_foreach((cachePtr->recQueue)->queue, printFn, NULL);
	printf("-----------------------------\n");
	printf("ENTRIES IN FREQUENT QUEUE:\n");
	g_queue_foreach((cachePtr->freqQueue)->queue, printFn, NULL);
	printf("-----------------------------\n\n");
}

/* DIAGNOSTIC FUNCTION to print bits */
const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
    return b;
}

static void toggleTwiceAccessedBit(unsigned int* blockAddr)
{
	*blockAddr ^= 0x1;
}

static inline unsigned int isTwiceAccessed(unsigned int blockAddr)
{
	return (blockAddr & 0x1);
}

static inline void triggerDatedFreqAccessedBit(unsigned int* blockAddr) //this is TRIGGER, not TOGGLE
{
	*blockAddr &= 0x2;
}
static inline void untriggerDatedFreqAccessedBit(unsigned int* blockAddr) //this is UNTRIGGER, not TOGGLE
{
	*blockAddr &= ~0x2;
}

static inline unsigned int isDatedFreqAccessed(unsigned int blockAddr)
{
	return (blockAddr & 0x2);
}

static inline unsigned int getAddr(unsigned int blockAddr) //this is currently unused
{
	return (blockAddr & ~0x3);
}

static inline int isInTail(node *nodePtr, unsigned char whichQueue)
{
	if(whichQueue == 1) {
		if(nodePtr->processNum <= (cachePtr->recMidTailProcNum)) return 1;
	} else {
		if(nodePtr->processNum <= (cachePtr->freqMidTailProcNum)) return 1;
	}
	
	return 0;
}

static inline double calcDistance(unsigned char whichQueue, int thisProcessNum)
{
	if(whichQueue == 1) {
		if(rec_MRU_num == rec_LRU_num) return 0;
		return SIZE_OF_TWO_QUEUES * ((double)(thisProcessNum - rec_LRU_num) / (rec_MRU_num - rec_LRU_num));
	} else {
		if(freq_MRU_num == freq_LRU_num) return 0;
		return SIZE_OF_TWO_QUEUES * ((double)(thisProcessNum - freq_LRU_num) / (freq_MRU_num - freq_LRU_num));
	}
}

/*  credits to  http://www.flipcode.com/archives/Fast_log_Function.shtml 
	supposed 5x speed improvement over math.h library's log/ln functions
	maximum error is below 0.007
	should be sufficient accuracy for our purposes?
*/
static inline float fastLogTwo(float val)
{
   int* const exp_ptr = (int*)(&val);
   int x = *exp_ptr;
   const int log_2 = ((0x23) & 255) - 128; //IS THIS 0x23 or 23?
   x &= ~(255 << 23);
   x += 127 << 23;
   *exp_ptr = x;

   val = ((-1.0f/3) * val + 2) * val - 2.0f/3;   // (1)

   return (val + log_2);
} 

/*	because Math says: "log x to the base 2 = ln(x) / ln(2)" */
static inline double baseNLog(double base, double x)
{
	return (double)((fastLogTwo((float)x))/(fastLogTwo((float)base)));
}

static inline double calcEmergencyOne(unsigned char whichQueue, int thisProcessNum)
{
    return baseNLog((double)SIZE_PARAMETER, 
    	(double)(CACHE_SIZE/calcDistance(whichQueue,thisProcessNum)));
}

static inline double calcEmergencyTwo(cache *cachePtr)
{
    return baseNLog((double)SIZE_PARAMETER, 
    	((double)(g_queue_get_length((cachePtr->freqQueue)->queue))/(g_queue_get_length((cachePtr->recQueue)->queue))));
}

//BOTH QUEUES HAVE SAME SIZED TAILS! TAIL SIZE ONLY CHANGES WHEN EITHER QUEUES CHANGES SIZE
static void updateTailSize() //at what point(s) do we update tail length?
{
    cachePtr->tailSize = MIN(CACHE_SIZE/SIZE_PARAMETER, 
    	MIN(g_queue_get_length((cachePtr->recQueue)->queue),g_queue_get_length((cachePtr->freqQueue)->queue)));

    if(g_queue_get_length((cachePtr->recQueue)->queue) == 0 || 
		g_queue_get_length((cachePtr->freqQueue)->queue) == 0) {
		cachePtr->recMidTailProcNum = currProcessNum + 1;
		cachePtr->freqMidTailProcNum = currProcessNum + 1;
    	return;
    }

    if(g_queue_get_length((cachePtr->recQueue)->queue) < cachePtr->tailSize || 
		g_queue_get_length((cachePtr->freqQueue)->queue) < cachePtr->tailSize) {
		cachePtr->recMidTailProcNum = rec_MRU_num;
		cachePtr->freqMidTailProcNum = freq_MRU_num;
    	return;
    }

    cachePtr->recMidTailProcNum = ((node*)(g_queue_peek_nth((cachePtr->recQueue)->queue, 
            	(cachePtr->tailSize)-1)))->processNum;
    cachePtr->freqMidTailProcNum = ((node*)(g_queue_peek_nth((cachePtr->freqQueue)->queue, 
            	(cachePtr->tailSize)-1)))->processNum;
}

// static void resetProcessNum() 
// {
// 	node* currNode = nodeList;
// 	unsigned int offset = currNode->processNum;
// 	assert(offset > 0); // safeguard against a pointless linked-list traversal
	
// 	while(currNode != NULL) {
// 		currNode->processNum -= offset;
// 		currNode = currNode->nextNode;
// 	}
	
// 	currProcessNum -= offset;
// 	rec_LRU_num -= offset;
// 	rec_MRU_num -= offset;
// 	freq_MRU_num -= offset;
// 	freq_LRU_num -= offset;
// 	cachePtr->recMidTailProcNum -= offset;
// 	cachePtr->freqMidTailProcNum -= offset;
// }

static void resetAllQueueNums(node* data, int* user_data)
{
	data->processNum -= *user_data;
}

static void resetProcessNum()
{
	unsigned int offset = MIN(((node*)g_queue_peek_tail((cachePtr->recQueue)->queue))->processNum, 
		((node*)g_queue_peek_tail((cachePtr->freqQueue)->queue))->processNum);

	GFunc resetNums = (GFunc)&resetAllQueueNums;
	g_queue_foreach((cachePtr->recQueue)->queue, resetNums, &offset);
	g_queue_foreach((cachePtr->freqQueue)->queue, resetNums, &offset);

	currProcessNum -= offset;
	rec_LRU_num -= offset;
	rec_MRU_num -= offset;
	freq_MRU_num -= offset;
	freq_LRU_num -= offset;
	cachePtr->recMidTailProcNum -= offset;
	cachePtr->freqMidTailProcNum -= offset;
}

unsigned int getFrontBlockAddr(unsigned int elemAddr)
{
	//should I just & ~0x3 instead?
	return (elemAddr >> 3) << 3; //because 2^4=16, and we are aligning to (512b)*16=8kb page size
}

/*
static void insertToList(node* newNode) //updating linked list pointers
{
	nodeList->prevNode = newNode;
	newNode->nextNode = (node*)(&(*nodeList));
	newNode->prevNode = NULL; //signalling the "entrance point" of the list
	nodeList = newNode;
	newNode->processNum = currProcessNum; //"Stamping the node"
}

static void removeFromList(node* nodeToRemove) //removing node from process linked-list
{
	(nodeToRemove->nextNode)->prevNode = (nodeToRemove->prevNode);
	(nodeToRemove->prevNode)->nextNode = nodeToRemove->nextNode;
}

static void moveToListFront(node* nodeToMove)
{
	removeFromList(nodeToMove);
	insertToList(nodeToMove);
}
*/

static node* createNewNode(unsigned int blockAddr)
{
	node* newNode = malloc(sizeof(node));
	newNode->processNum = currProcessNum;
	//insertToList(newNode); //updating linked-list pointers
	newNode->hddAddr = blockAddr;
	return newNode;
}

static void popTailAndMoveToGhost(node* ptrInQueue, unsigned char whichQueue)
{
	if(g_queue_get_length(cachePtr->ghostQueue) >= MAX_GHOST_QUEUE_SIZE) {
		removeNodeFromTable(((node*)(g_queue_peek_tail(cachePtr->ghostQueue)))->hddAddr);
		g_queue_pop_tail(cachePtr->ghostQueue); //evicting from ghost queue because MAX_GHOST_QUEUE_SIZE is hit
	}

	int* ghostAddr = malloc(sizeof(unsigned int));
	*ghostAddr = ptrInQueue->hddAddr;
	g_queue_push_head(cachePtr->ghostQueue, ghostAddr);
	changeToGhostNodeTableEntry(ptrInQueue->hddAddr);

	/* hereon starts the deletion of the node */
	//removeFromList(ptrInQueue);

	if(whichQueue == 1) {
		g_queue_pop_tail((cachePtr->recQueue)->queue);
	} else {
		g_queue_pop_tail((cachePtr->freqQueue)->queue);
	}

	//updateTailSize(); //is this necessary here?
	free(ptrInQueue);
}

static void updateUtilities()
{
	double sumUtilities = freqUtility + recUtility;
	freqUtility *= ((double)CACHE_SIZE / sumUtilities);
	recUtility *= ((double)CACHE_SIZE / sumUtilities);
}

static void updateProcessNumsAtEnds()
{
	if(g_queue_get_length((cachePtr->recQueue)->queue) != 0) {
		rec_LRU_num = ((node*)(g_queue_peek_tail((cachePtr->recQueue)->queue)))->processNum;
		rec_MRU_num = ((node*)(g_queue_peek_head((cachePtr->recQueue)->queue)))->processNum;
	} else {
		rec_LRU_num = 0;
		rec_MRU_num = 0;
	}
	if(g_queue_get_length((cachePtr->freqQueue)->queue) != 0) {
		freq_LRU_num = ((node*)(g_queue_peek_tail((cachePtr->freqQueue)->queue)))->processNum;
		freq_MRU_num = ((node*)(g_queue_peek_head((cachePtr->freqQueue)->queue)))->processNum;
    } else {
		freq_LRU_num = 0;
		freq_MRU_num = 0;
    }
}

static void shiftTailToHead()
{
	node* tempNode = g_queue_pop_tail((cachePtr->recQueue)->queue);
	g_queue_push_head((cachePtr->recQueue)->queue, tempNode);
}

static void replace()
{
	updateUtilities();
	node* tempNode;

	if(g_queue_get_length((cachePtr->recQueue)->queue) >= (int)recUtility 
		|| g_queue_get_length((cachePtr->freqQueue)->queue) <= (int)freqUtility ) {
		while(1) {
			if(isDatedFreqAccessed(((node*)(g_queue_peek_tail((cachePtr->recQueue)->queue)))->hddAddr) //ERROR POINT
				|| !isTwiceAccessed(((node*)(g_queue_peek_tail((cachePtr->recQueue)->queue)))->hddAddr)) {
				tempNode = g_queue_peek_tail((cachePtr->recQueue)->queue);
				popTailAndMoveToGhost(tempNode, 1);
				break;
			} else {
				shiftTailToHead();
				triggerDatedFreqAccessedBit(&(((node*)(g_queue_peek_head((cachePtr->recQueue)->queue)))->hddAddr));
			}
		}
	} else {
		tempNode = g_queue_peek_tail((cachePtr->freqQueue)->queue);
		popTailAndMoveToGhost(tempNode, 2);
	}
}

static node* enterQueue(node* ptrInQueue, unsigned int blockAddr, unsigned char fromQueue, unsigned char toQueue)
{
	if(ptrInQueue == NULL) {
		ptrInQueue = createNewNode(blockAddr);
	}

	ptrInQueue->processNum = currProcessNum;
	ptrInQueue->hddAddr = blockAddr;

	if(fromQueue == 1) {
		//moveToListFront(ptrInQueue);
		g_queue_remove((cachePtr->recQueue)->queue, ptrInQueue);
	}
	if(fromQueue == 2) {
		g_queue_remove((cachePtr->freqQueue)->queue, ptrInQueue);
	}
	// } else { // case when fromQueue == 3 or fromQueue == 0
	// 	ptrInQueue->processNum = currProcessNum;
	// 	//insertToList(ptrInQueue); //NOTE THIS MEANS THAT WHEN SOMETHING IS DISPLACED TO GHOST LIST, REMOVE FROM LINKED LIST
	// }

	ptrInQueue->whichQueue = toQueue;
	
	if(toQueue == 1) {
		g_queue_push_head((cachePtr->recQueue)->queue, ptrInQueue);
	} else { //toQueue == 2
		g_queue_push_head((cachePtr->freqQueue)->queue, ptrInQueue);
	}
	updateTailSize(); //is it good to update tail size here?
	return ptrInQueue;
}

static unsigned int checkAndInsert(unsigned int blockAddr, char op) //returns ssdAddr.... but for what!? is this necessary?
{
	unsigned char whichQueue = 0;
	double emerOne = 0, emerTwo = 0;
	unsigned int ssdAddr = 0, thisProcessNum = 0;
	//logEntry tempLogEntry;

	node* ptrInQueue = locateNodeInTable(blockAddr, &whichQueue, &thisProcessNum, &ssdAddr);

	switch(whichQueue) {

	    case 1: //found in recQueue
			if(isInTail(ptrInQueue,whichQueue)) {
				if((emerOne = calcEmergencyOne(whichQueue,thisProcessNum)) >= 1.0) {
					recUtility += emerOne; //need to safeguard against this overflowing??? VERY edge case...
				}
				if((emerTwo = calcEmergencyTwo(cachePtr)) >= 1.0) {
					recUtility += emerTwo;
				}
			}

			blockAddr = ptrInQueue->hddAddr;

			if(isTwiceAccessed(blockAddr)) {
				toggleTwiceAccessedBit(&blockAddr); //untoggle bit
				untriggerDatedFreqAccessedBit(&blockAddr);
				enterQueue(ptrInQueue,blockAddr,whichQueue,2); //recQ ==> freqQ
			} else {
				toggleTwiceAccessedBit(&blockAddr); //toggle bit
				enterQueue(ptrInQueue,blockAddr,whichQueue,1); //recQ ==> reqQ
			}

			//tempLogEntry.op = op;
			//tempLogEntry.addr = ssdAddr;
			//g_array_append_val(ssdLog, tempLogEntry);
			return ssdAddr;
			
	    case 2: //found in freqQueue
	        if(isInTail(ptrInQueue,whichQueue)) {
				if((emerOne = calcEmergencyOne(whichQueue,thisProcessNum)) >= 1.0) {
					freqUtility += emerOne; //need to safeguard against this overflowing??? VERY edge case...
				}
				if((emerTwo = calcEmergencyTwo(cachePtr)) >= 1.0) {
					freqUtility += emerTwo;
				}
			}
			enterQueue(ptrInQueue,blockAddr,whichQueue,2); //freqQ ==> freqQ

			//tempLogEntry.op = op;
			//tempLogEntry.addr = ssdAddr;
			//g_array_append_val(ssdLog, tempLogEntry);
			return ssdAddr;
			
		case 3: //found in ghostQueue
			if(g_queue_get_length((cachePtr->freqQueue)->queue) 
				+ g_queue_get_length((cachePtr->recQueue)->queue) >= CACHE_SIZE) {
				replace();
			}

			ptrInQueue = enterQueue(ptrInQueue,blockAddr,whichQueue,2); //ghostQ ==> freqQ

			//NOTE. NEED TO WRITE TO CACHE, UPDATE ssdAddr, THEN RETURN ssdAddr

			changeFromGhostNodeTableEntry(ptrInQueue);

			//tempLogEntry.op = op;
			//tempLogEntry.addr = blockAddr;
			//g_array_append_val(hddLog, tempLogEntry);
			break;
			
	    default: //not found in queues
			if(g_queue_get_length((cachePtr->freqQueue)->queue) 
				+ g_queue_get_length((cachePtr->recQueue)->queue) >= CACHE_SIZE) {
				replace();
			}	        

			/** ERROR OCCURS HEIR!!! **/ //-->in enterQueue, the g_queue_push_head to recQueue
			ptrInQueue = enterQueue(ptrInQueue,blockAddr,whichQueue,1); //ghostQ ==> recQ

			//NOTE. NEED TO WRITE TO CACHE, UPDATE ssdAddr, THEN RETURN ssdAddr

	        insertToMgmtTable(ptrInQueue);
	        //tempLogEntry.op = op;
			//tempLogEntry.addr = blockAddr;
			//g_array_append_val(hddLog, tempLogEntry);
	}
}

/*
	elemAddr is given as a sector-number; elemSize is given in terms of kb;
	everytime you insertElem, add to the chronological linked list of nodes;
*/
int insertElem(unsigned int elemAddr, unsigned int elemSize, char op)
{
	//handle cases in which elemSize is not a standard (not power of 2?)
	
	/* for debugging purposes */
	currLineNum++;
	
	unsigned int frontBlockAddr = getFrontBlockAddr(elemAddr), numNodesToWrite;

	numNodesToWrite = (elemSize * BYTES_PER_KILOBYTE / SECTOR_SIZE) / PAGE_MULTIPLE;
	
	if(frontBlockAddr != elemAddr) {
		numNodesToWrite++;
	}
	
	if((currProcessNum + numNodesToWrite) < currProcessNum) { //safeguard against overflow of process numbering
		resetProcessNum();
	}
	
	for(int i = 0; i < numNodesToWrite; i++) {
		checkAndInsert(frontBlockAddr + (i * PAGE_MULTIPLE), op);
		updateProcessNumsAtEnds();
		currProcessNum++; //process number incrementing
	}

	printQueues();
	return numNodesToWrite;
}

// unsigned int* updateAddr(unsigned int oldAddr, unsigned int newAddr)
// {
// 	//is update just to remove and add again? think about it...
//     //...
// }

// void freeCache(cache *ptr)
// {
// 	//free stuff in queues, then call the cleanup function provided in gqueue
//     //...
// }

int initCache() 
{
    cachePtr = malloc(sizeof(cache));
    
    cachePtr->recQueue = malloc(sizeof(cacheQueue));
    (cachePtr->recQueue)->queue = g_queue_new();
    
    cachePtr->freqQueue = malloc(sizeof(cacheQueue));
    (cachePtr->freqQueue)->queue = g_queue_new();

    cachePtr->ghostQueue = g_queue_new();

    //ssdLog = g_array_new(TRUE,TRUE,sizeof(logEntry));
    //hddLog = g_array_new(TRUE,TRUE,sizeof(logEntry)); //need to free these later..
    
    cachePtr->tailSize = CACHE_SIZE/3; //RANDOM!!!
	currProcessNum = 0;
	recUtility = 1; //WHAT TO INITIALIZE TO? I GUESSED THIS. probably should put more thought into it.
	freqUtility = 1;
	
	/* for debugging purposes */
	currLineNum = 0;

	return 1;
}

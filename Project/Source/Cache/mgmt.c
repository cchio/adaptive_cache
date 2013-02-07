#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <glib.h>
#include "ssarc.h"
#include "mgmt.h"

#define CACHE_SIZE 6990506 	
#define PAGE_SIZE 8192
#define BYTES_PER_KILOBYTE 1024
#define SECTOR_SIZE 512
#define PAGE_MULTIPLE (PAGE_SIZE/SECTOR_SIZE) //i.e. how many sectors make a page
#define SIZE_PARAMETER 256
#define SIZE_OF_TWO_QUEUES (g_queue_get_length((cachePtr->recQueue)->queue) + g_queue_get_length((cachePtr->freqQueue)->queue))
#define MAX_GHOST_QUEUE_SIZE 1024
#define WRITE_BLOCK_SIZE 4194304 //4mb in bytes

GHashTable* mgmtTable;

static void printValueFn(unsigned int* key, value* tempValue, void* user_data)
{
	printf("hddAddr: %u;	ssdAddr: %u;	nodePtr: %p\n", 
		GPOINTER_TO_UINT(key), tempValue->ssdAddr, tempValue->nodePtr);
}

static void printAllTableEntries()
{
	GHFunc printFunction = (GHFunc)&printValueFn;
	printf("\n------------------MANAGEMENT TABLE ENTRIES:-------%d------\n", currLineNum);
	g_hash_table_foreach(mgmtTable, printFunction, NULL);
	printf("-----------------------alles gut-------------------------\n\n");
}

#if 0
static void printSingleEntry(unsigned int key)
{
	value* tempValue = g_hash_table_lookup(mgmtTable,GUINT_TO_POINTER(key));
	printf("hddAddr: %u;	ssdAddr: %u;	nodePtr: %p\n", 
		GPOINTER_TO_UINT(key), tempValue->ssdAddr, tempValue->nodePtr);
}
#endif

void insertToMgmtTable(node* nodePtr) //NEED TO ADD SSD ADDR
{
	//unsigned int* hddAddr = malloc(sizeof(unsigned int));
	value* cachedNode = malloc(sizeof(value));

	unsigned int ssdAddr = 0; //NEED TO CHANGE THIS!!! FUNCTION TO WRITE AND RETURN SSD ADDR

	cachedNode->nodePtr = nodePtr;
	cachedNode->ssdAddr = ssdAddr;
	//*hddAddr = nodePtr->hddAddr;

	//SOMETHING MYSTICAL HAPPENS HERE

	g_hash_table_insert(mgmtTable, GUINT_TO_POINTER(nodePtr->hddAddr), cachedNode);

	printAllTableEntries();
}

 //NEED TO ADD SSD ADDR
void updateSsdAddrInTable(unsigned int hddAddr, unsigned int ssdAddr) //only when ssd addr changes
{
	((value*)g_hash_table_lookup(mgmtTable,GUINT_TO_POINTER(hddAddr)))->ssdAddr = ssdAddr;
}

/*
void updateHddAddrInTable(unsigned int newHddAddr)
{
	value* tempValuePtr = g_hash_table_lookup(mgmtTable,GUINT_TO_POINTER(newHddAddr));
	g_hash_table_remove(mgmtTable,GUINT_TO_POINTER(newHddAddr));

	g_hash_table_insert(mgmtTable,GUINT_TO_POINTER(newHddAddr), tempValuePtr);
}
*/

void changeToGhostNodeTableEntry(unsigned int hddAddr) //to delete nodePtr, set ssdAddr to sentinal value 0
{
	value* tempValue = malloc(sizeof(value));
	tempValue->nodePtr = NULL;
	tempValue->ssdAddr = 0;
	g_hash_table_replace(mgmtTable,GUINT_TO_POINTER(hddAddr),tempValue);
}

void changeFromGhostNodeTableEntry(node* nodePtr) //NEED TO ADD SSD ADDR
{
	value* tempValue = malloc(sizeof(value));
	tempValue->nodePtr = nodePtr;

	unsigned int ssdAddr = 0; //NEED TO CHANGE THIS!!! FUNCTION TO WRITE AND RETURN SSD ADDR

	tempValue->ssdAddr = ssdAddr;
	g_hash_table_replace(mgmtTable,GUINT_TO_POINTER(nodePtr->hddAddr),tempValue);
}

node* locateNodeInTable(unsigned int blockAddr, unsigned char *whichQueue, unsigned int *thisProcessNum, unsigned int *ssdAddr)
{
	value* foundValue = g_hash_table_lookup(mgmtTable, GUINT_TO_POINTER(blockAddr));

	if(foundValue == NULL) return NULL;
	
	if(foundValue->nodePtr == NULL) {
		*whichQueue = 3;
		return NULL;
	}

	node* locatedNode = foundValue->nodePtr;
	*whichQueue = locatedNode->whichQueue;
	*thisProcessNum = locatedNode->processNum;
	*ssdAddr = foundValue->ssdAddr;
	
	return locatedNode;
}

void removeNodeFromTable(unsigned int hddAddr)
{
	g_hash_table_remove(mgmtTable,GUINT_TO_POINTER(hddAddr));
}

static int alignAddrEqual(unsigned int a, unsigned int b)
{
	return ((a & ~0x3) == (b & ~0x3));
}

/*
void findKeyWithBitInfo(unsigned int *key, value *value, unsigned int *blockAddr)
{
	if(alignAddrEqual(*key, *blockAddr))
		*blockAddr = *key;
}
*/

//after purging buffer entries to disk, update new ssd-addresses
void updateHashTableEntries(node* nodePtr)
{
	//...
}

void purgeBufferToDisk(node* nodePtr)
{
	//...
}

void addToBuffer(node* nodePtr)
{
	//...
}

static void freeElem(value* data)
{
	free(data);
}

int initMgmtPolicy()
{
	GDestroyNotify GDestroyFunction = (GDestroyNotify)&freeElem;
	GEqualFunc GEqualFn = (GEqualFunc)&alignAddrEqual;
	
	mgmtTable = g_hash_table_new_full(g_direct_hash, GEqualFn, 
		GDestroyFunction, GDestroyFunction);

	return 1;
}

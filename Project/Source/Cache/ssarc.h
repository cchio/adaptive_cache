#ifndef _SSARC_H
#define _SSARC_H
#include <glib.h>
#include <unistd.h> // for size_t

struct node {
    unsigned int processNum;
    unsigned int hddAddr;
    unsigned char whichQueue;
    unsigned char writeCount; //not used yet!!!
};

struct node;
typedef struct node node;

struct cacheQueue {
    GQueue* queue;
    double utility;
    unsigned int headProcessNum;
    unsigned int tailProcessNum;
};

typedef struct cacheQueue cacheQueue;

struct cache {
    cacheQueue* recQueue;  //recency queue
    cacheQueue* freqQueue; //frequency queue
    GQueue* ghostQueue;    //ghost queue
    size_t tailSize;
    unsigned int recMidTailProcNum;
    unsigned int freqMidTailProcNum;
};

typedef struct cache cache;

/* for debugging purposes */
unsigned int currLineNum;

/*
struct logEntry {
    char op;
    unsigned int addr;
};

typedef struct logEntry logEntry;
*/

//GArray* ssdLog;
//GArray* hddLog;

/*** PUBLIC METHODS ***/

int initCache();

int insertElem(unsigned int elemAddr, unsigned int elemSize, char op);

unsigned int getFrontBlockAddr(unsigned int elemAddr);

void printQueues();

//unsigned int* updateAddr(unsigned int oldAddr, unsigned int newAddr);

//void freeCache(cache *ptr);

#endif

#ifndef _MGMT_H
#define _MGMT_H
#include <glib.h>
//#include <unistd.h> // for size_t
#include "ssarc.h"

struct value {
    node* nodePtr;
    unsigned int ssdAddr;
};

typedef struct value value;

int initMgmtPolicy();

void insertToMgmtTable(node* nodePtr);

node* locateNodeInTable(unsigned int blockAddr, unsigned char *whichQueue, unsigned int *thisProcessNum, unsigned int *ssdAddr);

void changeFromGhostNodeTableEntry(node* nodePtr);

void changeToGhostNodeTableEntry(unsigned int hddAddr);

void removeNodeFromTable(unsigned int hddAddr);

void updateSsdAddrInTable(unsigned int hddAddr, unsigned int ssdAddr);

//void updateHddAddrInTable(unsigned int newHddAddr);

#if 0
static void printAllTableEntries();
#endif 

#endif

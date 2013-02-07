#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include "ssarc.h"
#include "mgmt.h"

#define ALLOCATION_INCREASE_QUANTUM 500
#define MAX_LINE_SIZE 50
#define PAGE_SIZE 8192 // the smallest quantum size of each node's address block is 8kb = 8192 bytes

//to use fcyc to record time?

//FILE *sofp;
//FILE *hofp;

typedef struct {
	int op; //NOTE: 1 represents read, 2 represents write
	unsigned int hddAddr;
	unsigned int size;
	unsigned int lineNum;
} request_struct;

typedef struct {
	char name[128];	//short name of script
	int numOps;	//number of requests
	request_struct *ops;	//array of requests read from the script
} script_struct;

static const char *last(const char *path, char ch) { 
	return strrchr(path, ch) ? strrchr(path, ch) + 1 : path; 
}

/* Function: readNextLine
 * ----------------------
 * Reads one line from file (up to size of buf). Skips lines that are all-white or beginning with
 * comment char #. If nRead is non-null, will increment pass-by-ref counter with number of
 * lines read/skipped. Removes trailing newline. Returns true if did read valid line, false otherwise.
 */
int readNextLine(char buf[], int bufSize, FILE *ifp, unsigned int *nRead)
{
	while(1) {
		if(fgets(buf, bufSize, ifp) == NULL) 
			return 0;
		if(nRead)
			(*nRead)++;
		if(buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';// remove trailing newline char
		char ch;
		if(sscanf(buf, " %c", &ch) == 1 && ch != '#')
			return 1;
	}
}


static void readScriptFile(char *input_file_path, script_struct *script)
{
	FILE *ifp = fopen(input_file_path, "r");
    if(ifp == NULL) {
        printf("Could not open %s in readScriptFile\n", input_file_path);
    	exit(1);
    }

    //saving script name, removing file extension
    script->name[0] = '\0';
    strncat(script->name, last(input_file_path, '/'), sizeof(script->name));
    char *dot = strrchr(script->name, '.'); // remove filename extension
    if(dot) 
    	*dot = '\0';

    script->ops = NULL;
    script->numOps = 0;

    unsigned int lineNum = 0, numOpsAllocated = 0;
    char buf[MAX_LINE_SIZE]; //temporary buffer of single line read
    char request[MAX_LINE_SIZE]; //target for request

    for(int i = 0; readNextLine(buf, sizeof(buf),ifp, &lineNum); i++) {
    	if(i == numOpsAllocated) {
    		numOpsAllocated += ALLOCATION_INCREASE_QUANTUM;
    		script->ops = realloc(script->ops, numOpsAllocated * sizeof(request_struct));
    	}

    	(script->ops[i]).lineNum = lineNum;
    	int nScanned = sscanf(buf, " %s %d %d", request, &script->ops[i].hddAddr, &script->ops[i].size);

    	if(strcmp(request,"rd") == 0 && nScanned == 3) {
    		(script->ops[i]).op = 1;
    	} else if(strcmp(request,"wr") == 0 && nScanned == 3) {
    		(script->ops[i]).op = 2;
    	} else {
    		printf("Malformed request '%s' on line %d of %s\n", buf, lineNum, script->name);
    		exit(1);
    	}
    	script->numOps = i+1;
    }

    fclose(ifp);
}

/*
static void initoutputFiles()
{
	sofp = fopen("outputSSD.txt", "w");
	hofp = fopen("outputHDD.txt", "w");

	if(hofp == NULL) {
        printf("Could not initialize %s in initHDDoutputFile\n", "outputHDD.txt");
    	exit(1);
    }
    if(sofp == NULL) {
        printf("Could not initialize %s in initSSDoutputFile\n", "outputSSD.txt");
    	exit(1);
    }
}

static void writeOutputFiles()
{
	for(int i = 0; *(int**)&(g_array_index(hddLog,logEntry,i)) != 0; i++) {
		fprintf(hofp, "%s %d\n", hddLog);
	}

	for(int i = 0; *(int**)&(g_array_index(ssdLog,logEntry,i)) != 0; i++) {
		fprintf(sofp, "%s %d\n", ssdLog);
	}
 }
 */

static int runTraceScript(script_struct *script)
{
	// if(!initCache()) {
	// 	printf("Error initializing caching policy.");
	// 	return 0;
	// }
	// if(!initMgmtPolicy()) {
	// 	printf("Error initializing cache management.");
	// 	return 0;
	// }

	for(int req = 0; req < script->numOps; req++) {
		//unsigned int addr = (script->ops[req]).lineNum;
		//unsigned int requestedSize = (script->ops[req]).size;

		switch((script->ops[req]).op) {

            case 1:
            	insertElem((script->ops[req]).hddAddr, 
            		(script->ops[req]).size, 'r');
                break;
                
            case 2:
                insertElem((script->ops[req]).hddAddr, 
            		(script->ops[req]).size, 'w');
                break;
        }
	}
	return 1;
}

int main(int argc, char *argv[])
{
	script_struct script;
	readScriptFile(argv[1], &script);
	printf( "Evaluating algorithm on %s....", script.name);
	//initoutputFiles();
	initCache();
	initMgmtPolicy();

	runTraceScript(&script);
	printf("~~~ je suis fini ~~~\n");
	//writeOutputFiles();
	//free(script.ops);
	//free(&script);

	exit(1);
}

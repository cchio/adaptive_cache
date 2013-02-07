#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ssarc.h"
#include "mgmt.h"

//to use fcyc to time the speed?

// struct for a single read/write request 
typedef struct {
    enum {READ, WRITE} op;         // type of request
    unsigned int addr;             // id for free() to use later
    unsigned int size;             // num bytes for alloc/realloc request
    unsigned int lineNum;          // which line in file
} request_t;

// struct for info of one trace script file
typedef struct {
    char name[128];     // short name of script 
    int num_ops;        // number of requests 
    request_t *ops;     // array of requests read from script
    //block_t *blocks;    // array of blocks returned by malloc when executing
} script_t;

static void fatal_error(char *format, ...) 
{
    fflush(stdout);
    fprintf(stdout, "\nFATAL ERROR: ");
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    exit(107); 
}

/* Function: ReadNextLine
 * ----------------------
 * Reads one line from file (up to size of buf). Skips lines that are all-white or beginning with
 * comment char #. If nRead is non-null, will increment pass-by-ref counter with number of
 * lines read/skipped. Removes trailing newline. Returns true if did read valid line, false otherwise.
 */
bool ReadNextLine(char buf[], int bufSize, FILE *fp, int *nRead)
{
    while (true) {
        if (fgets(buf, bufSize, fp) == NULL) return false;
        if (nRead) (*nRead)++;
        if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] ='\0'; // remove trailing newline
        char ch;
        if (sscanf(buf, " %c", &ch) == 1 && ch != '#') // scan first non-white char, check not #
            return true;
    }
}



static void readScriptFile(char *path, script_t *script)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        fatal_error("Could not open %s in ReadScriptFile\n", path);
        
    script->name[0] = '\0';
    strncat(script->name, last(path, '/'), sizeof(script->name));
    char *dot = strrchr(script->name, '.'); // remove filename extension
    if (dot) *dot = '\0';
    script->ops = NULL;
    script->num_ops = 0;
    int lineNum = 0, numOpsAllocated = 0;
    char buf[1024];

    for (int i = 0; ReadNextLine(buf, sizeof(buf), fp, &lineNum) ; i++) {
        if (i == numOpsAllocated) {
            numOpsAllocated += 500;
            script->ops = realloc(script->ops, numOpsAllocated*sizeof(request_t));
        }

        script->ops[i].lineNum = lineNum;
        char* request;
        int nScanned = sscanf(buf, " %s %d %d", &request, &script->ops[i].addr, &script->ops[i].size);
        if (request == "rd" && nScanned == 3)
            script->ops[i].op = READ;
        else if (request == 'wr' && nScanned == 3)
            script->ops[i].op = WRITE;
        else 
            fatal_error("Malformed request '%s' line %d of %s\n", buf, lineNum, script->name);
        
        script->num_ops = i+1;
    }
    fclose(fp);
}

static int runTraceScript(script_t *script)
{
	if(!initCache()) {
		printf("Error initializing caching policy.");
		return 0;
	}
	if(!initMgmtPolicy()) {
		printf("Error initializing management policy.");
		return 0;
	}

	for (int req = 0; req < script->num_ops; req++) {
        int id = script->ops[req].id;
        int requested_size = script->ops[req].size;
        int old_size = script->blocks[id].size;
        void *p, *newp, *oldp = script->blocks[id].ptr;
        
        switch (script->ops[req].op) {
                
            case READ:
                
                break;
                
            case WRITE:
            
                break;
        }
    }
    return true;
}	



void main(int argc, char *argv[])
{

	script_t script;
	readScriptFile(argv[1],&script);
	printf( "Evaluating algorithm on %s....", script.name);
	runTraceScript(&script);


	free(script.ops);
    free(script.blocks);
}

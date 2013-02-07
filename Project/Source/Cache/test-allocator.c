/*
 * Files: test-allocator.c
 * ----------------------
 * Reads and interprets text-based script files containing a sequence of
 * allocator requests. Runs the allocator on the script, validating for
 * for correctness and then evaulating allocator's utilization and throughput.
 *
 * jzelenski updated Wed Nov 23 22:19:32 PST 2011
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>     
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <valgrind/callgrind.h>
#include <malloc.h>
#include <math.h>

#include "allocator.h"
#include "segment.h"
#include "fcyc.h"

// Alignment requirement 
#define ALIGNMENT 8

// Returns true if p is ALIGNMENT-byte aligned 
#define IS_ALIGNED(p)  ((((unsigned long)(p)) % ALIGNMENT) == 0)

// default path for test scripts
#define DEFAULT_SCRIPT_DIR "/afs/ir/class/cs107/samples/assign7"
#define MAX_SCRIPTS 100

// Throughput (in Kreq/sec) of the libc malloc on reference system (myth)
// on sample set of inputs. Give as a constant here to provide
// you a stable target goal to measure your code against.
#define AVG_LIBC_THRUPUT      13000


// struct for a single allocator request 
typedef struct {
    enum {ALLOC, FREE, REALLOC} op; // type of request
    int id;                     // id for free() to use later
    int size;                   // num bytes for alloc/realloc request
    int lineNum;                // line in file
} request_t;

// struct for facts about a single malloc'ed node
typedef struct {
    void *ptr;
    int size;
} block_t;

// struct for info for one script file
typedef struct {
    char name[128];     // short name of script 
    int num_ids;        // number of alloc/realloc ids 
    int num_ops;        // number of requests 
    request_t *ops;     // array of requests read from script
    block_t *blocks;     // array of blocks returned by malloc when executing
} script_t;

// packs the params to the speed function to be timed by fcyc. 
// timed function must take single void * client data pointer
typedef struct {
    script_t *script;
    double *utilization;
    bool libc;
} perfdata_t;

// Stats from executing a script
typedef struct {
    char name[128];     // short name of script
    double ops;         // number of ops (malloc/free/realloc) in script
    int valid;          // was the script processed correctly by the allocator? 
    double secs;        // number of secs needed to execute the script 
    double utilization; // mem utilization  (percent of heap storage in use)
} stats_t; 

typedef enum {Correctness = 1, Performance = 2, LibC = 4} testTypeT;

static void GetScriptFiles(char *path, char files[][PATH_MAX], int maxcount, int *count);
static int TestAllocator(char filenames[][PATH_MAX], int n, testTypeT whatToTest);
static void ReadScriptFile(char *filename, script_t *script);
static bool EvalScriptForCorrectness(script_t *script);
static void EvalScriptForPerformance(void *clientData);
static bool VerifyBlock(void *head, int size, script_t *script, int opnum);
static bool VerifyPayload(void *head, int size, int id, script_t *script, int line , char *op);
static int PrintStatistics(stats_t stats[], int n, testTypeT whatToTest);
static void Usage(char *name);
static void fatal_error(char *format, ...);
static void allocator_error(script_t *script, int linenum, char* format, ...);
static int min(int x, int y);
static const char *last(const char *path, char ch);
static int cmplast(const void *one, const void *two);


int main(int argc, char *argv[])
{
/**
    myinit();
    int numHeights = 16;
    void* heights[numHeights];

    void* test = mymalloc(4084);
    printf("malloc %p, size 4084\n", test);
    test = myrealloc(test, 1000);
    printf("realloc %p, size 1000\n", test);
    for (int i = 0; i < numHeights; i++) {
        heights[i] = mymalloc(8);
        *(int*)(heights[i]) = i;
        printf("alloc then free %p, %d\n", (heights[i]), *(int*)heights[i]);
        myfree(heights[i]);
    }


    for (int i = 0; i < numHeights; i++) {
        heights[i] = mymalloc(8);
        *(int*)(heights[i]) = i;
        printf("alloc %p, %d\n", (heights[i]), *(int*)heights[i]);
    }

    for (int i = numHeights - 1; i >= 0; i--) {
        void* temp = heights[i];
        heights[i] = myrealloc(heights[i], 13);
        printf("realloc %p to %p, %d\n", temp, heights[i], *(int*)heights[i]);
//        myfree(heights[i]);
//      *(int*)(heights[i]) = i;
//      printf("alloc %p, %d\n", (heights[i]), *(int*)heights[i]);
    }


    for (int i = 0; i < numHeights; i++) {
        heights[i] = mymalloc(8);
        *(int*)(heights[i]) = i;
        printf("alloc %p, %d\n", (heights[i]), *(int*)heights[i]);
    }
    for (int i = 0; i < numHeights; i++) {
        if (i % 4 != 0) {
            printf("free %p, %d\n", (heights[i]), *(int*)(heights[i]));
            myfree(heights[i]);
        }
    }
    for (int i = 0; i < numHeights; i++) {
        heights[i] = mymalloc(40);
        *(int*)(heights[i]) = i;
        printf("alloc %p, %d\n", (heights[i]), *(int*)(heights[i]));
        }
    // for (int i = 0; i < numHeights; i++) {
    //  printf("%p\n", (heights[i]));
    //  printf("%d\n", *(int*)(heights[i]));
    // }
    return 0;
**/

    
    myinit();
    char paths[MAX_SCRIPTS][PATH_MAX];
        testTypeT whatToTest = Correctness | Performance;
        char c;
        int num_scripts = 0;
        
        CALLGRIND_TOGGLE_COLLECT ;// turn off profiling while we do the setup work, later turn on during simulation
        
        while ((c = getopt(argc, argv, "f:pcl")) != EOF) {
            switch (c) {
                case 'f':
                    GetScriptFiles(optarg, paths, sizeof(paths)/sizeof(paths[0]), &num_scripts);
                    break;
                case 'p':
                    whatToTest = Performance;
                    break;
                case 'c':
                    whatToTest = Correctness;
                    break;
                case 'l':
                    whatToTest = LibC | Performance;
                    break;
                default:
                    Usage(argv[0]);
            }
        }
        if (optind < argc)
            Usage(argv[0]);
        if (num_scripts == 0)
            GetScriptFiles(DEFAULT_SCRIPT_DIR, paths, sizeof(paths)/sizeof(paths[0]), &num_scripts);
        qsort(paths, num_scripts, sizeof(paths[0]), cmplast); // sort by filename
        int num_failures = TestAllocator(paths, num_scripts, whatToTest);
        return num_failures; // exit code will be zero if all success

  
}

/* Function: GetScriptFiles
 * ------------------------
 * Given a path, figures out if file or directory, and adds name(s) to the array accordingly
 */
static void GetScriptFiles(char *path, char files[][PATH_MAX], int maxcount, int *count)
{
    struct stat st;
    if (stat(path, &st) != 0) 
        fatal_error("Could not open script file \"%s\".\n", path);
    if (!S_ISDIR (st.st_mode)) {  // path is single file, not directory
        strcpy(files[(*count)++], path);
        return;
    }
    
    DIR *dirp = opendir(path);
    if (!dirp) 
        fatal_error("Could not open directory \"%s\".\n", path);
    printf("Reading scripts from %s...", path);
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL && *count < maxcount) { // read files from dir one-by-one
        // take all non-hidden files with .script extension
        if (dp->d_name[0] != '.' && strcmp(last(dp->d_name,'.'), "script") == 0)
            sprintf(files[(*count)++], "%s/%s", path, dp->d_name);
    }
    closedir(dirp);
    printf(" found %d.\n", *count);
}


/* Function: TestAllocator
 * -----------------------
 * Runs a set of scripts against the allocator.  It works script-by-script
 * and first tests correctness (unless perf_only is true) and if the script
 * had no functional errors, it runs a time trial on the same script. It
 * stores an array of statistics, which it reports on at the end.
 */
static int TestAllocator(char filenames[][PATH_MAX], int n, testTypeT whatToTest)
{
    stats_t stats[n];
    
    for (int i = 0; i < n; i++) {
        script_t script;
        ReadScriptFile(filenames[i], &script);
        if (whatToTest & Correctness) printf( "Evaluating allocator on %s....", script.name);
        fflush(stdout);
        strcpy(stats[i].name, script.name);        
        stats[i].ops = script.num_ops;
        stats[i].valid = !(whatToTest & Correctness) || EvalScriptForCorrectness(&script);
        if (stats[i].valid && (whatToTest & Performance)) {
            perfdata_t pd = (perfdata_t) {.script = &script, .utilization = &stats[i].utilization, .libc = whatToTest & LibC};
            stats[i].secs = fsecs(EvalScriptForPerformance, &pd);
        } else {
            stats[i].secs = stats[i].utilization = 0;
        }
        free(script.ops);
        free(script.blocks); 
        if (whatToTest & Correctness) printf(" done.\n");
    }
    return PrintStatistics(stats, n, whatToTest); // display table of results
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

/*
 * Fuction: ReadScriptFile
 * -----------------------
 * Parse a script file and store sequences of requests for later execution.
 */
static void ReadScriptFile(char *path, script_t *script)
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
    int lineNum = 0, numOpsAllocated = 0, max_id = 0;
    char buf[1024];

    for (int i = 0; ReadNextLine(buf, sizeof(buf), fp, &lineNum) ; i++) {
        if (i == numOpsAllocated) {
            numOpsAllocated += 500;
            script->ops = realloc(script->ops, numOpsAllocated*sizeof(request_t));
        }
        script->ops[i].lineNum = lineNum;
        char request;
        int nScanned = sscanf(buf, " %c %d %d", &request, &script->ops[i].id, &script->ops[i].size);
        if (request == 'a' && nScanned == 3)
            script->ops[i].op = ALLOC;
        else if (request == 'r' && nScanned == 3)
            script->ops[i].op = REALLOC;
        else if (request == 'f' && nScanned == 2)
            script->ops[i].op = FREE;
        else 
            fatal_error("Malformed request '%s' line %d of %s\n", buf, lineNum, script->name);
        if (script->ops[i].id > max_id) max_id = script->ops[i].id;
        script->num_ops = i+1;
    }
    fclose(fp);

    script->num_ids = max_id + 1;
    script->blocks = calloc(script->num_ids, sizeof(block_t));
    assert(script->blocks);
}


/* Function: EvalScriptForCorrectness
 * ----------------------------------
 * Check the allocator for correctness. Interprets the earlier parsed script
 * operation-by-operation and reports if it detects any "obvious"
 * errors (returning blocks outside the heap, unaligned, overlapping, etc.)
 */
static bool EvalScriptForCorrectness(script_t *script)
{
    if (myinit() != 0) {
        allocator_error(script, 0, "myinit failed.");
        return false;
    }
    memset(script->blocks, 0, script->num_ids*sizeof(script->blocks[0]));
    
    for (int req = 0; req < script->num_ops; req++) {
        int id = script->ops[req].id;
		// if(id == 0) {
		// 			printf("hello, world\n");
		// 		}
        int requested_size = script->ops[req].size;
        int old_size = script->blocks[id].size;
        void *p, *newp, *oldp = script->blocks[id].ptr;
        
        switch (script->ops[req].op) {
                
            case ALLOC:
                if ((p = mymalloc(requested_size)) == NULL && requested_size != 0) {
                    allocator_error(script, script->ops[req].lineNum, "malloc returned NULL");
                    return false;
                }
                // Test new block for correctness: must be properly aligned 
                // and must not overlap any currently allocated block. 
                if (!VerifyBlock(p, requested_size, script, script->ops[req].lineNum))
                    return false;
                
                // Fill new block with the low-order byte of new id
                // can be used later to verify data copied when realloc'ing
                memset(p, id & 0xFF, requested_size);
                script->blocks[id] = (block_t){.ptr = p, .size = requested_size};
                break;
                
            case REALLOC:
                if (!VerifyPayload(oldp, old_size, id, script, script->ops[req].lineNum, "realloc-ing"))
                    return false;
                if ((newp = myrealloc(oldp, requested_size)) == NULL && requested_size != 0) {
                    allocator_error(script, script->ops[req].lineNum, "realloc returned NULL!");
                    return false;
                }
                
                old_size = script->blocks[id].size;
                script->blocks[id].size = 0;
                if (!VerifyBlock(newp, requested_size, script, script->ops[req].lineNum))
                    return false;
                // Verify new block contains the data from the old block
                for (int j = 0; j < min(old_size, requested_size); j++) {
                    if (*((unsigned char *)newp + j) != (id & 0xFF)) {
                        allocator_error(script, script->ops[req].lineNum, "realloc did not preserve the data from old block");
                        return false;
                    }
                }
                // Fill new block with the low-order byte of new id
                memset(newp, id & 0xFF, requested_size);
                script->blocks[id] = (block_t){.ptr = newp, .size = requested_size};
                break;
                
            case FREE:
                old_size = script->blocks[id].size;
                p = script->blocks[id].ptr;
                // verify payload intact before free
                if (!VerifyPayload(p, old_size, id, script, script->ops[req].lineNum, "freeing"))
                    return false;
                script->blocks[id] = (block_t){.ptr = NULL, .size = 0};
                myfree(p);
                break;
        }
    }
    // verify payload is still intact for any block still allocated
    for (int id = 0;  id < script->num_ids;  id++)
        if (!VerifyPayload(script->blocks[id].ptr, script->blocks[id].size, id, script, -1, "at exit"))
            return false;
    return true;
}

/* Function: EvalScriptForPerformance
 * ----------------------------------
 * This is almost same code as function above, but unifying the two really
 * clouded things up and added performance penalties to this one, which I
 * wanted to streamline. This interprets the script with no additiona
 * overhead (no checking for validity) and tracks the high water mark
 * of the heap segment to report on memory utilization.  The function
 * takes a void * client pointer, since that is what is required to work
 * with the timing trial code.
 */
static void EvalScriptForPerformance(void *clientData)
{
    perfdata_t *pd = (perfdata_t *)clientData;
    int max_total_payload_size = 0, cur_total_payload_size = 0;
    int max_heap_segment_size = 0;
    script_t *script = pd->script;
    
    if (!pd->libc && myinit() != 0) {
        allocator_error(script, 0, "myinit failed.");
        return;
    }
    memset(script->blocks, 0, script->num_ids*sizeof(script->blocks[0]));
    
    CALLGRIND_TOGGLE_COLLECT;   // turn on valgrind profiler here
    for (int line = 0;  line < script->num_ops;  line++) {
        int id = script->ops[line].id;
        int requested_size = script->ops[line].size;
        
        switch (script->ops[line].op) {
                
            case ALLOC:
                script->blocks[id].ptr = pd->libc ? malloc(requested_size) : mymalloc(requested_size);
                script->blocks[id].size = requested_size;
                cur_total_payload_size += requested_size;
                break;
                
            case REALLOC:
                script->blocks[id].ptr = pd->libc ? realloc(script->blocks[id].ptr, requested_size) : myrealloc(script->blocks[id].ptr, requested_size);
                cur_total_payload_size += (requested_size - script->blocks[id].size);
                script->blocks[id].size = requested_size;
                break;
                
            case FREE:
                pd->libc? free(script->blocks[id].ptr) : myfree(script->blocks[id].ptr);
                cur_total_payload_size -= script->blocks[id].size;
                script->blocks[id] = (block_t){.ptr = NULL, .size = 0};
                break;
        }
        if (cur_total_payload_size > max_total_payload_size) max_total_payload_size = cur_total_payload_size;
        if (HeapSegmentSize() > max_heap_segment_size) max_heap_segment_size = HeapSegmentSize();
    }

    *pd->utilization = pd->libc? NAN : ((double)max_total_payload_size)/max_heap_segment_size;
    CALLGRIND_TOGGLE_COLLECT;  // turn off profiler here
}



/* Function: VerifyBlock
 * ---------------------
 * Does some simple checks on the block returned by allocator to try to
 * verify correctness.  If any problem shows up, reports an allocator error
 * with details and line from script file. The checks it performs are:
 *  -- verify block address is correctly aligned
 *  -- verify block address is within heap segment
 *  -- verify block address + size doesn't overlap any existing allocated block
 */
static bool VerifyBlock(void *head, int size, script_t *script, int opnum)
{
    // address must be ALIGNMENT-byte aligned
    if (!IS_ALIGNED(head)) {
        allocator_error(script, opnum, "New block (%p) not aligned to %d bytes", 
                        head, ALIGNMENT);
        return false;
    }
    if (head == NULL && size == 0) return true;
    
    // block must lie within the extent of the heap
    void *end = (char *)head + size;
    void *heap_end = (char *)HeapSegmentStart() + HeapSegmentSize();
    if (head < HeapSegmentStart() || end > heap_end) {
        allocator_error(script, opnum, "New block (%p:%p) not within heap (%p:%p)",
                        head, end, HeapSegmentStart(), heap_end);
        return false;
    }
    // block must not overlap any other blocks 
    for (int i = 0; i < script->num_ids; i++) {
        if (script->blocks[i].ptr == NULL || script->blocks[i].size == 0) continue;
        void *other_head = script->blocks[i].ptr;
        void *other_end = (char *)other_head + script->blocks[i].size;
        if ((head >= other_head && head < other_end) || (end > other_head && end < other_end) ||
            (head < other_head && end >= other_end)){
            allocator_error(script, opnum, "New block (%p:%p) overlaps existing block (%p:%p)",
                            head, end, other_head, other_end);
            return false;
        }
    }
    return true;
}

/* Function: VerifyPayload
 * -----------------------
 * When a a block is allocated, the payload is filled with a simple repeating pattern
 * Later when realloc'ing or freeing that block, check the payload to verify those
 * contents are still intact, otherwise raise allocator error.
 */
static bool VerifyPayload(void *ptr, int size, int id, script_t *script, int line, char *op)
{
    for (int j = 0; j < size; j++) {
        if (*((unsigned char *)ptr+ j) != (id & 0xFF)) {
            allocator_error(script, line, "invalid payload data detected when %s address %p", op, ptr);
            return false;
        }
    }
    return true;
}

/* Function: PrintStatistics
 * -------------------------
 * This pprints a summary of results on individual scripts, as well as overall
 * average utilization and throughput.
 */
static int PrintStatistics(stats_t stats[], int n, testTypeT whatToTest) 
{
    double secs = 0, ops = 0, util = 0;
    int failures = 0;
    
    // Print the individual results for each script
    printf("\n script          correct?   utilization   requests    secs     Kreq/sec\n");
    for (int i = 0; i < 80; i++) putchar('-');
    for (int i = 0; i < n; i++) {
        printf("\n%-20s %-7s ", stats[i].name, whatToTest & Correctness ? (stats[i].valid ? "yes" : "no") : "n/a" );
        if (stats[i].valid && whatToTest & Performance) {
            printf("%5.0f%% %12.0f %12.6f %8.0f",  stats[i].utilization*100.0, stats[i].ops,
                   stats[i].secs, (stats[i].ops/1000.0)/stats[i].secs);
            secs += stats[i].secs;
            ops += stats[i].ops;
            util += stats[i].utilization;
        } else {
            if (!stats[i].valid) failures++;
            printf("%5s %12s %12s %8s", "-", "-", "-","-");
        }
    }
    putchar('\n');
    for (int i = 0; i < 80; i++) putchar('-');
    printf("\n");
    
    // Print the aggregate results for all scripts
    if (whatToTest & Performance) {
       
        if (failures != n) { // if at least one script succeeded, compute overall results
            printf("\n%-20s %-7s %5.0f%% %12.0f %12.6f %8.0f\n", 
                   "Total", "", (util/n)*100.0, ops, secs, ops/(1e3*secs));
            double avg_util = util/n;
            double avg_throughput = ops/(1e3*secs);
            double p2 = ((double)avg_throughput)/AVG_LIBC_THRUPUT;
            if (whatToTest & LibC)
                printf("\nResults shown for libc throughput. libc utilization cannot be measured by our harness, averages ~70%%.\n");
            else
                printf("\nOverall performance: %.0f%% (utilization) %.0f%% (throughput, expressed relative to libc)\n",
                   avg_util*100, p2*100);
        } else  
            printf("\n%-20s %-7s %5s %12s %12s %8s\n", 
                   "Total", "", "-", "-", "-", "-");
    }
    if (failures != 0)
        printf("%d script%s exited with correctness errors.\n", failures, (failures > 1 ? "s" : ""));
    return failures;
}

static int min(int x, int y) { return (x < y ? x : y); }

static const char *last(const char *path, char ch) { return strrchr(path, ch) ? strrchr(path, ch) + 1 : path; }
static int cmplast(const void *a, const void *b) { return strcmp(last(a,'/'), last(b,'/')); }


// fatal_error - Report an error and exit
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

// Used to report errors when invoking student's allocator functions
static void allocator_error(script_t *script, int lineNum, char* format, ...) 
{
    va_list args;
    fflush(stdout);
    fprintf(stdout, "\nALLOCATOR ERROR [%s, line %d]: ", script->name, lineNum);
   va_start(args, format);
   vfprintf(stdout, format, args);
   va_end(args);
   fprintf(stdout,"\n");
}


static void Usage(char *name) 
{
   fprintf(stderr, "Usage: %s [-f <file-or-dir>]\n", name);
   fprintf(stderr, "\t-l                Run scripts using libc allocator (implies performance only).\n");
   fprintf(stderr, "\t-c                Run only the correctness tests (no checks for performance).\n");
   fprintf(stderr, "\t-p                Run only the performance tests (no checks for correctness).\n");
   fprintf(stderr, "\t-f <file-or-dir>  Use <file> as script or read all script files from <dir>.\n");
   fprintf(stderr, "Without -f option, reads scripts from default path: %s\n", DEFAULT_SCRIPT_DIR);
   exit(1);
}

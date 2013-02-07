/***************************************************************************

title          :io.c
description    :Reference async io implementation
author         :Tucker Berckmann
notes          :Derived from aio.cpp by Benjamin Segovia     
copyright      :Not sure, probably Apache license

****************************************************************************/

#define _GNU_SOURCE

#include <fcntl.h>
#include <libaio.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define SZ 512
#define JUNK_VAL 0x84
#define MAG_NUM_OFFSET1 510
#define MAG_NUM_OFFSET2 511
#define NUM_JOBS 5

char dest[NUM_JOBS][SZ] __attribute__((aligned(512)));
static const int maxEvents = 10;
const char MAG_NUM_VAL1 = 0x55;
const char MAG_NUM_VAL2 = 0xAA;
  
static void handle_error(int err, int lineNo) {
#define DECL_ERR(X) case -X: FATAL("Error "#X"\n"); break;
  switch (err) {
    DECL_ERR(EFAULT);
    DECL_ERR(EINVAL);
    DECL_ERR(ENOSYS);
    DECL_ERR(EAGAIN);
  };
  if (err < 0) FATAL("Unknown error: Line %i", lineNo);
#undef DECL_ERR
}

#define IO_RUN(LINENO,F, ...)\
  do {\
    int err = F(__VA_ARGS__);\
    handle_error(err,LINENO);\
  } while (0)
  
void check(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  if (res2 || res != SZ) FATAL("Error in async IO");
  printf("IO Completed\n");
  fflush(stdout);
}

int main() {

  size_t i;
  
  /* Attempt to read the MBR magic number from the device as a sanity check */
  FILE *file = fopen("/dev/sdd", "r");
  if (file == NULL) FATAL("Unable to open the device");
  for (i = 0; i < SZ; i++) dest[0][i] = JUNK_VAL;
  size_t nr = fread(dest[0], SZ, 1, file);
  if (nr != 1) FATAL("Unable to read from the device");
  fclose(file);
  if(dest[0][MAG_NUM_OFFSET1] != MAG_NUM_VAL1 || dest[0][MAG_NUM_OFFSET2] != MAG_NUM_VAL2)
    printf("Warning: Magic number not found during sanity check\n");
  
  /* Open the device again, but get ready to use async IO this time */
  int fd = open("/dev/sdd", O_NONBLOCK | O_DIRECT | O_RDWR, 0);
  if (fd < 0) FATAL("Error opening file");

  /* Now use *real* asynchronous IO to read multiple chunks in parallel */
  io_context_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  IO_RUN (__LINE__, io_queue_init, maxEvents, &ctx);

  /* This is the read job we asynchronously run */
  struct iocb job_data[NUM_JOBS];
  struct iocb *job = job_data;
  struct iocb *q[NUM_JOBS];
  for (i = 0; i < SZ; i++) dest[0][i] = JUNK_VAL;
  for (i = 0; i < NUM_JOBS; i++,job++) {
    if (i & 1)
       io_prep_pread(job, fd, dest[i], SZ, i * SZ);
    else
       io_prep_pwrite(job, fd, dest[i], SZ, i * SZ);
    q[i] = job;
  }
  
  /* Issue it now */
  job = job_data;
  IO_RUN (__LINE__, io_submit, ctx, NUM_JOBS, q);

  /* Wait for it */
  struct timespec timeout;
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;
  struct io_event evts[NUM_JOBS];
  IO_RUN (__LINE__, io_getevents, ctx, NUM_JOBS, NUM_JOBS, evts, &timeout);
  for (i = 0; i < NUM_JOBS; i++) {
    /* Confirm that the IOs completed successfully */
    check(ctx, evts[i].obj, evts[i].res, evts[i].res2);
  }
  
  /* Only checking the first buffer, since the other ones were read from
  other locations */
  if(dest[0][MAG_NUM_OFFSET1] != MAG_NUM_VAL1 || dest[0][MAG_NUM_OFFSET2] != MAG_NUM_VAL2)
    printf("Warning: Magic number not found during async read\n");

  close(fd);
  io_destroy(ctx);
  
  return 0;
  
}


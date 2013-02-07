/***************************************************************************

title          :disk-sync.c
description    :Synchronous IO implementation (which replaces async version)
author         :Tucker Berckmann
notes          :    
copyright      :Copyright 2012 SanDisk Corporation

****************************************************************************/

#include <stdio.h>
#include "disk.h"

static FILE *hddHandle = NULL;
static FILE *cacheHandle = NULL;

/* Attempt to open disks as files using open command */
int initializeDisks ( const char *hddPath, const char *cachePath) {

  hddHandle = fopen(hddPath, "r+");
  if (hddHandle == NULL) {
  	printf("Unable to open hard disk\n");
  	return 1;
  }
  
  cacheHandle = fopen(cachePath, "r+");
  if (cacheHandle == NULL) {
  	printf("Unable to open cache drive\n");
  	fclose(hddHandle);
  	return 1;
  }
  
  return 0;
  
}

/* Issue a series of commands to the requested disk. Unlike the parallel
implementation, this call is always blocking. */
int issueCmds( unsigned int numRequests, struct diskRequest *requests) {
	return 0;
}

/* Clean up before quitting the program */
void shutdownDisks() {

	if( hddHandle != NULL ) {
		fclose(hddHandle);
	}
	
	if( cacheHandle != NULL ) {
		fclose(cacheHandle);
	}
	
	return;
}
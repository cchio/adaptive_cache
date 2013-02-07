/***************************************************************************

title          :disk.h
description    :Interface to disk layer
author         :Tucker Berckmann 
notes          :    
copyright      :Copyright 2012 SanDisk Corporation

****************************************************************************/
#ifndef _DISK_H
#define _DISK_H

#include "common.h"

enum targetType {
	cacheTarget,
	hddTarget
};

struct diskRequest {
	enum opType op;
	enum targetType target;
	unsigned int address;
	unsigned int length;
};

int initializeDisk ( const char *hddPath, const char *cachePath); 
int issueCmds( unsigned int numRequests, struct diskRequest *requests);
void shutdownDisks();

#endif
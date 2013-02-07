/***************************************************************************

title          :common.h
description    :Common definitions for all modules
author         :Tucker Berckmann
notes          :    
copyright      :Copyright 2012 SanDisk Corporation

****************************************************************************/
#ifndef _COMMON_H
#define _COMMON_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define FATAL(...)\
  do {\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
    assert(0);\
    exit(-1);\
  } while (0)

enum opType { 
	typeRead = 1, 
	typeWrite = 2 
};

#endif
#!/bin/bash -   
#title          :get_disks.sh
#description    :Get candidate disks (HDD and Cache) for testing
#author         :Tucker Berckmann  
#usage          :./get_disks.sh
#notes          :       
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

if [ `uname` == "Darwin" ] ; then

	# List of partitions that are mounted
	partlist=`df | awk -f get_fs_part.awk` 
	

elif [ `uname` == "Linux" ] ; then

	printf "Linux support not added yet\n"
	
fi
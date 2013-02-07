#!/usr/bin/awk   
#title          :screen_disks.awk
#description    :Given the output from diskutil and a list of partitions,
#               :return the disks that don't contain any of the partitions
#author         :Tucker Berckmann  
#usage          :diskutil list | awk -f lib.awk -f scren_disks.awk \
#               : "/dev/part1 /dev/part2"
#notes          :       
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

BEGIN {
	
	if (ARGC > 2) {
		error("Too many arguments")
	}
	
	split(ARGV[1], partitions, " ")
	
	for (i in partitions) {
		if (partitions[i] !~ /^\/dev\/[^\/]*$/) {
			error("Partition is not under dev or has trailing slash")
		} else {
			# Remove the leading /dev/ to get short name
			partitions[i] = substr(partitions[i],6)		
		}
	}	
}
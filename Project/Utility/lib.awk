#!/usr/bin/awk 
#title          :lib.awk
#description    :Contains library functions used by other awk scripts
#author         :Tucker Berckmann  
#usage          :awk -f lib.awk -f <someAwkScript>
#notes          :Doesn't do anything on its own       
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

# Print the message to standard error and quit
function error(message) {
	errorcnt += 1
	print "Error:",message | "cat 1>&2"
	exit 1
}

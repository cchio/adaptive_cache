#!/usr/bin/awk   
#title          :get_fs_part.awk
#description    :Read which partitions have a filesystem and return them
#author         :Tucker Berckmann  
#usage          :df | awk -f lib.awk -f get_fs_part.awk
#notes          :       
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

BEGIN {
	errorcnt = 0; devcnt = 0;
}

# Make sure that column headers are consistent with what we expect from
# df as a sanity check. This also confirms that the input to this program
# is really the output from a successful call to df.
NR == 1 {
	if( $1 !~ /^Filesystem$/ || $3 !~ /^Used$/ || $4 !~ /^Avail/ || \
	 $6 !~ /^Mounted$/ ){
	 	errorcnt += 1
		error("Unexpected output from df")
	}
}

# Print the device path if it lives in /dev/
NR > 1 && $1 ~ /^\/dev\// { print $1; devcnt += 1 }

END {
	if (errorcnt == 0 && devcnt == 0) {
		error("Did not find any devices")
	}
}
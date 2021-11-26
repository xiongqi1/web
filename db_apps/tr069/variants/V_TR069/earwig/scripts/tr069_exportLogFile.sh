#!/bin/sh
# This script takes two arguments log type (error/full) and output log file.
# Copies the corresponding full log file to the output log file given
# Copyright (C) 2018 NetComm Wireless limited.

source /lib/utils.sh

log_type="$1"
o_filename="$2"

if [ -z "$o_filename" -o -z "$log_type" ]; then
	logErr "Wrong arguments log_type: $log_type o_filename: $o_filename"
	exit 1;
fi

# Set the right path based on error/full log requested
log_path="/var/log/messages"
if [ "$log_type" = "error" ]; then
	log_path="/log/messages"
fi

# concatenates multiple log files if present
generateFullLog() {
	rm -f -- $o_filename
	for curr_file in $(ls -r $log_path*); do
		if [ -f $curr_file ]; then
			cat $curr_file >> "$o_filename"
			if [ $? -ne 0 ]; then
				logErr "Failed to append logfile $curr_file"
				return 1
			fi
		fi
	done
}

generateFullLog || exit 1

exit 0

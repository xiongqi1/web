#!/bin/sh
#
# CWMP Lua Daemon Re-start Wrapper
#

nof=${0##*/}
source /lib/utils.sh

# Catch the TERM signal and make sure to kill the child process spawned
trap _term SIGTERM

# Kill the child process if it is spawned, when this process is killed
_term() {
	if [ -n $child_pid ]; then
		logInfo "Caught SIGTERM! sending SIGTERM to pid: $child_pid"
		kill -TERM $child_pid 2>/dev/null
		if [ $? -ne 0 ] ; then
			logErr "Could not kill process $child_pid"
			exit 1
		fi
		# exit gracefully once we killed the child process
		logInfo "Exiting gracefully after handling SIGTERM to pid: $child_pid"
		exit 0
	fi
	logErr "Could not find child pid to kill"
	exit 1
}

# generate a (16-bit) random value with upper limit
rand_limit() {
	local lim="$1"
	local val=$(head -c2 /dev/urandom) # get two random bytes
	local lsb=$(printf '%d' "'${val:0:1}")
	local msb=$(printf '%d' "'${val:1:1}")
	let "val=(msb*256+lsb)%(lim+1)"
	echo "$val"
}

# If tr069.server.random_inform.enable = 1, delay launching cwmpd by
# a random time within a window given by rdb variable
# tr069.server.random_inform.window
prog="$1"
cwmpd="cwmpd.lua"
if [ -z "${prog%%*$cwmpd}" ]; then # only delay for cwmpd
	enable=$(rdb_get service.tr069.enable)
	if [ "$enable" != "1" ]; then
		exit 0
	fi
	randinfo_en=$(rdb_get tr069.server.random_inform.enable)
	if [ "$randinfo_en" = "1" ]; then
		randinfo_win=$(rdb_get tr069.server.random_inform.window)
		case "$randinfo_win" in
			''|*[!0-9]*)
				logWarn "Bad random inform window ignored"
				;;
			*)
				snextra=$(rdb_get uboot.snextra)
				echo "$snextra" > /dev/urandom # seed with encrypted serial no.
				secs=$(rand_limit "$randinfo_win")
				logInfo "Sleeping for $secs seconds before launching $1"
				sleep "$secs"
				logInfo "Woken up. Launching $1 now"
				;;
		esac
	fi
fi

enable=$(rdb_get service.tr069.enable)
while [ "$enable" == "1" ]; do
	$1 $2 &
	child_pid=$!
	wait $child_pid
	if [ $? -ne 0 ] ; then
		logWarn "Process exited with a failure, respawning..."
	else
		logInfo "Process exited normally."
		exit 0
	fi
	enable=`rdb get service.tr069.enable`
	test "$enable" == "1" && sleep 10
done

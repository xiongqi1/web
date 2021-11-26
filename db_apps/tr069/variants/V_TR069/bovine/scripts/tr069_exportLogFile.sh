#!/bin/sh

o_fullname="$1"

if [ -z "$o_fullname" ]; then
	exit 1;
fi

bail() {
	exit 1
}

generateFullLog() {
	FULLLOGFILE="$1"
	if [ -f /opt/messages.0 ]; then
		cat /opt/messages.0 /opt/messages > "$FULLLOGFILE" || return 1
	else
		cat /opt/messages > "$FULLLOGFILE" || return 1
	fi
}

logToFile=$(rdb_get service.syslog.option.logtofile)
if [ "$logToFile" = "1" ]; then
	generateFullLog "$o_fullname" || bail
else
	logread > "$o_fullname" || bail
fi

exit 0



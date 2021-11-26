#!/bin/sh
echo -e 'Content-type: text/html\n'
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
	/usr/bin/killall -9 syslogd
	#check if syslogd is running in buffer mode or file mode
	BUFFER_MODE=`rdb_get service.syslog.option.logtofile`
	if [ "$BUFFER_MODE" = "1" ]; then
		# for Bovine
	#	rm -f /opt/message* >/dev/null 2>&1

		# for Platypus2
		rm -f /etc/message* >/dev/null 2>&1
	fi
	echo "var result='ok'"

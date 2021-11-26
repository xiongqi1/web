#!/bin/sh

. /etc/variant.sh

echo -e 'Content-type: text/html\n'
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
	/usr/bin/killall -9 syslogd
	#check if syslogd is running in buffer mode or file mode
	BUFFER_MODE=`rdb_get service.syslog.option.logtofile`
	if [ "$BUFFER_MODE" = "1" ]; then
		if [ "$V_SYSLOG_STYLE" = "generic" ]; then
			# for Serpent/Fisher
			rm -f /var/log/message* >/dev/null 2>&1
		else
			# for Bovine
			rm -f /opt/message* >/dev/null 2>&1
		fi

		# for Platypus2
		#rm -f /etc/message* >/dev/null 2>&1
	fi
	echo "var result='ok'"

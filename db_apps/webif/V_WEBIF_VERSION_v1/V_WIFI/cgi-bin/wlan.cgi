#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
echo -e 'Content-type: text/html\n'
currChan=""
configChan=$(rdb_get wlan.0.conf.channel)
# Auto Channel selected
if [ "$configChan" = "0" ];then
	currChan=$(rdb_get wlan.0.currChan)
	if [ $currChan = "" ] || [ $currChan -lt 1 ] || [ $currChan -gt 14 ] ;then
		echo "var currChan='Scanning...';"
	else
		echo "var currChan=\"$currChan\";"
	fi
else
	echo "var currChan=\"$configChan\";"
fi

#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
echo -e 'Content-type: text/html\n'

i=0
echo "["
while [ -n "`rdb_get wlan.reservation.mac.$i`" ]; do
	hostname="`rdb_get wlan.reservation.hostname.$i | base64 | tr -d '\n'`"
	mac="`rdb_get wlan.reservation.mac.$i`"
	if [ "$i" -gt "0" ]; then
		echo ","
	fi
	echo "{\"mac\":\""$mac"\",\"host\":\""$hostname"\"}"
	let "i+=1"
done;
echo "]"
exit 0

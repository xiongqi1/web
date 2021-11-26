#!/bin/sh
echo -e 'Content-type: text/html\n'

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

if [ "$QUERY_STRING" = "/www/snmp.mib" -o "$QUERY_STRING" = "/etc/ipseclog.txt" ]; then
	v=`ls "$QUERY_STRING" 2>/dev/null`
fi
echo "var result=\""$v"\";"

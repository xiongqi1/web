#!/bin/sh
echo -e 'Content-type: text/html\n'

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

if [ -z "$QUERY_STRING" ]; then
	/sbin/Prevision_Sierra_Airprime_MC.sh
else
	/sbin/Prevision_Sierra_Airprime_MC.sh "$QUERY_STRING"
fi



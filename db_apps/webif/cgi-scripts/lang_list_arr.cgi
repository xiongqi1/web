#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
echo -e 'Content-type: text/html\n'
echo "var lang= new Array();"
v=`ls -d ../lang/*/ |sed 's/..\/lang\//lang[\"/'`
echo $v |sed 's/[\/]/\"]=1;/g'


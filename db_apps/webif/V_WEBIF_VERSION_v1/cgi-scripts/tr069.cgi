#!/bin/sh

echo -e 'Content-type: text/html\n'
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

informStart=$(rdb_get tr069.informStartAt)
informEnd=$(rdb_get tr069.informEndAt)

echo "informStartStatus=\"$informStart\";"
echo "informEndStatus=\"$informEnd\";"


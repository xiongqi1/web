#!/bin/sh
# Copyright (C) 2018 NetComm Wireless Limited.

echo -e 'Content-type: text/html\n'
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
fi

# clear all diagnostic logs
/usr/bin/diaglog.sh remove

echo "var result='ok'"


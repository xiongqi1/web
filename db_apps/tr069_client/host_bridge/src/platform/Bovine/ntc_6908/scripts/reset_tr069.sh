#!/bin/sh

RDB_KEY_PATTERNS="tr069.transfer tr069.event tr069.dhcp tr069.server tr069.state"

for PATTERN in $RDB_KEY_PATTERNS; do
#	echo $PATTERN
	KEYS=`rdb list $PATTERN`
	test -n "$KEYS" && echo rdb unset $KEYS
	test -n "$KEYS" && rdb unset $KEYS
done

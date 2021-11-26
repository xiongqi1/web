#!/bin/sh
while true;do
	sleep 20
	test -z `pidof pots_bridge` && pots_reload.sh
done
exit 0
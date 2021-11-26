#!/bin/sh
#
# CWMP Lua Daemon Re-start Wrapper
#

log() {
  /usr/bin/logger -t cwmpd-wrap.sh -- $@
}

enable=`rdb get service.tr069.enable`
while [ "$enable" == "1" ]; do
	# TR-069 client will not start in the rf qualfication mode or there is no rf_alignment console
	RFMODE=`rdb get service.dccd.mode`
        paused=`rdb get service.tr069.paused`
	if [ "$paused" != "1" -a -n "$RFMODE" -a "$RFMODE" != "rf_qualification" ]; then
		$1 $2
		log "Process exited with exit code $?. Respawning..."
	fi
	enable=`rdb get service.tr069.enable`
	test "$enable" == "1" && sleep 10
done

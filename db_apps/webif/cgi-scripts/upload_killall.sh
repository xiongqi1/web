#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
kill_and_wait()
{
	let "timeout=10"
	killall -q "$1" 1>/dev/null 2>&1
	while true; do 
		alive=`ps | grep "$1" | grep -v grep`
		if [ "$alive" == "" ]; then
			break
		else
			let "timeout-=1"
			if [ "$timeout" -eq "0" ]; then
				return
			fi
			sleep 1
		fi
	done
	return
}

rdb_set service.cportal.enable 0
kill_and_wait "wwand"
#kill_and_wait "pppd"
kill_and_wait "wscd"
kill_and_wait "ntpclient"
kill_and_wait "telnetd"
kill_and_wait "cnsmgr"
kill_and_wait "upnpd"
kill_and_wait "udhcpd"
kill_and_wait "dnsmasq"
#kill_and_wait "nvram_daemon"
kill_and_wait "periodicpingd"

#modprobe -r cdcs_DD
#echo 3 > /proc/sys/vm/drop_caches
#killall cnsmgr 1>/dev/null 2>&1
exit 0
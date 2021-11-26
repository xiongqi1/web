#!/bin/sh
Usage(){
	echo
	echo -e "`basename $0`- Called by cgi to set telephony profile."
	echo -e "Internal Use Only"
	echo
}

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	Usage
	exit 0
fi

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

rdb_set pots.status
rdb_set slic_calibrated 0
#/bin/reboot_module.sh  2>/dev/null
rdb_set service.system.reset 1

echo -e 'Content-type: text/html\n'
echo ""
#killall -9 pots_bridge  2>/dev/null
#killall -9 simple_at_manager  2>/dev/null
echo "<meta http-equiv='REFRESH' content='0;url=/system_reset.html?reboot'/>"


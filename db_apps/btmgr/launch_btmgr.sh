#!/bin/sh

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
    echo ""
	echo "This shell script is used to launch btmgr via template or manually."
	echo "Dbus and bluetoothd should be runnung prior to calling this script and"
    echo "LD_LIBRARY_PATH should be set properly for shared libraries."
	exit 0
fi

log() {
	logger -t "launch_btmgr.sh" -- "$@"
}

# sanity check
DBUS_EXIST=$(which dbus-daemon)
BT_EXIST=$(which bluetoothd)
BT_EN=$(rdb_get bluetooth.conf.enable)
if [ -z "$DBUS_EXIST" -o -z "$BT_EXIST" -o "$BT_EN" != "1" ]; then
	log "not ready to launch btmgr(dbusd:$DBUS_EXIST, btd:$BT_EXIST, bt_en:$BT_EN)..."
	exit 255
fi

# set LD_LIBRARY_PATH for shared libraries if bluez was installed by IPK pacakge which
# install its libraries to /usr/local/lib.
source /etc/variant.sh
BLUEZ_IPK=$(echo $V_INCLUDED_IPK | grep bluez)
LOCAL_LDLIB_PATH=$(echo $LD_LIBRARY_PATH | grep "local")
if [ -n "$BLUEZ_IPK" -a -z "$LOCAL_LDLIB_PATH" ]; then
	export LD_LIBRARY_PATH="/usr/local/lib:"$LD_LIBRARY_PATH
	log "set local lib path=$LD_LIBRARY_PATH"
fi

# wait for dbus-daemon and bluetoothd running
timeout=60
while [ $timeout -gt 0 ]; do
	dbus_pid=$(pidof dbus-daemon)
	btd_pid=$(pidof bluetoothd)
	if [ -n "$dbus_pid" -a -n "$btd_pid" ]; then
		log "dbus-daemon[$dbus_pid] & bluetoothd[$btd_pid] are running, launch btmgr now..."
		break
	fi
	timeout=$((timeout-2))
	sleep 2
done

# give up if both of dbus-daemon and bluetoothd are not running within time limit
if [ $timeout -le 0 ]; then
	log "Both of dbus-daemon[$dbus_pid] & bluetoothd[$btd_pid] are not running, give up launching btmgr..."
	exit 255
fi

# now launch btmgr
rm /var/lock/subsys/btmgr 2>/dev/null
btmgr
exit 0

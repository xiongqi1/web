#!/bin/sh

Downloader="fwdloader_cinterion"

FirmwareImage="$1"

PORT_AT="`rdb_get wwan.0.V250_if.1`"

# BGS2-E has no USB port
. /etc/variant.sh
MODEL=`rdb_get wwan.0.model`
MODULE=`rdb_get wwan.0.module_name`
if [ "$MODEL" = "BGS2-E" -o "$MODULE" = "BGS2-E" -o "$V_MODULE" = "BGS2-E" ]; then
	# BGS2-E module firmware should be downloaded through same port where ^SFDL command is sent.
	# Also firmware file has 1 byte of length field.
	PORT_DL="/dev/ttyAPP4"
	IF_TYPE="UART"
else
	IF_TYPE="USB"

	# In ModComms there could be many mices that create & use /dev/ttyACMx so the download port module_name
	# can be other than /dev/ttyACM0 and the virtual comm port name only appears after sending "AT^SFDL"
	# firmware download command to the Cinterion module so it is impossible to give the port name as a argument
	# for fwdloader_cinterion applicaton. The best way to detect this download port is waiting for the new ACM port
	# and let fwdloader_cinterion kwow the port name via RDB variable.
	# This implementation should be revised when there are multiple Cinterion modules.
	if [ "$V_MODCOMMS" = "y" ]; then
		PORT_DL="/dev/ttyACMx"
	else
		PORT_DL="/dev/ttyACM0"
	fi
fi

log() {
 echo -e "$@"
}

logerr() {
 echo -e "$@" >&2
}

fatal() {
 echo "$@" >&2
 exit 1
}

printHelp() {
	logerr "\nCinterion firmware upload script"
	logerr "\nUsage:"

	logerr "\tfwdloader_cinterion.sh <firmware image>"

	logerr "\nOptions:"
	logerr "\tfirmware image - the image to download"

	logerr ""
}

wait_for_download_port() {
	local DEVPATH="/sys/class/tty/"
	local VID="v1E2D"
	local PID="p0054"
	local TARGETID="$VID$PID"

	# wait the port for 10 seconds
	local TIMEOUT=10
	rdb_set wwan.0.module_info.dlport
	while [ $TIMEOUT -gt 0 ]; do
		for i in ${DEVPATH}ttyACM*; do
			if [ -d "$i/device" -a -f "$i/device/modalias" ] ; then
				DEVID=$(cut -c 5-14 $i/device/modalias)
				if [ "$DEVID" = "$TARGETID" ] ; then
					# string is like /sys/class/tty/ttyACM3
					rdb_set wwan.0.module_info.dlport "/dev/"${i#*tty/}
					log "found Cinterion dl port /dev/${i#*tty/}"
					return
				fi
			fi
		done
		TIMEOUT=$((TIMEOUT-1))
		sleep 1
	done
}

termPortProcesses() {
	plist="atportmgr cnsmgr simple_at_manager wwand connection_mgr pppd runonterm modem_emulator gpsd gpsd_client periodicpingd"
	killall $plist 2> /dev/null > /dev/null

	## wait for the processes to terminate
	j=0
	while [ $j -lt 10 ]; do
		if ! pidof $plist 2> /dev/null > /dev/null; then
			break
		fi

		j=$((j+1))
		sleep 1
	done

	## terminate
	killall -9 $plist 2> /dev/null > /dev/null
}

powerCycle() {
	reboot_module.sh 2> /dev/null > /dev/null
}

wait_for_port() {
	dev_port="$1"
	timeout="$2"

	if [ -z "$dev_port" ]; then
		return
	fi

	i=0
	while [ $i -lt $timeout ]; do
		if [ -c "$1" ]; then
			break
		fi

		i=$((i+1))
		sleep 1
	done
}

wait_for_AT_port() {
	wait_for_port $PORT_AT 10
}

if [ "$1" = "-h" -o "$1" = "--help" ]; then
    printHelp
    exit 0
fi

if [ $# != 1 ]; then
	printHelp
	exit 1
fi

if [ ! -f "$FirmwareImage" ]; then
	fatal "Error: Firmware Image does not exist"
fi

extention=`echo "$FirmwareImage"|awk -F . '{print $NF}'`

if [ ! "$extention" = "usf" ]; then
	fatal "Error: Invalid Firmware File"
fi

log "Disabling supervisor"
killall -stop supervisor
## disable monitoring by supervisor
rdb_set wwan.0.activated 0
## disable launching cnsmgr by udev
rdb_set module.upgrade 1

log "Terminating processes"
termPortProcesses

if [ "$V_MODCOMMS" = "y" ]; then
	(sleep 2 && wait_for_download_port)&
fi

$Downloader -p $PORT_AT -d $PORT_DL -f $FirmwareImage -i $IF_TYPE

result="$?"

sleep 5

log "Enabling supervisor"
killall -cont supervisor
rdb_set module.upgrade 0

log "Power-cycling to normal operation mode"
powerCycle

log "Waiting..."
wait_for_AT_port

log "Launching connection_mgr"
connection_mgr

if [ "$result" == "0" ]; then
	log "Done: Success"
	exit 0
else
	log "Done: Failure"
	exit 1
fi

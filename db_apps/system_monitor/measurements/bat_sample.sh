#!/bin/sh
#
# Outputs battery samples, 1/sec
# Time is seconds since Epoch.
# Time[sec] mode[cd] filtered_voltage[mV]

OTIME=`date +%s`
TIME=$OTIME

while true; do
	while [ $OTIME -eq $TIME ]; do
		usleep 100000
		TIME=`date +%s`
	done
	OTIME=`date +%s`
	MODE=`rdb_get battery.mode`
	VOLT=`rdb_get battery.voltage`
	PERC=`rdb_get battery.percent`
	echo "$TIME $MODE $VOLT $PERC"
done

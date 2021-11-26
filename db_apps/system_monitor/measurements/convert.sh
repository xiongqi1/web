#!/bin/bash
#
# Converts irregular samples:
# date, volt, percent, Charging/Discharging
# Into interpolated regular samples (1/sec):
# second c/d volt

pick() {
	shift $1
	echo "$1"
}

P_SECONDS=0
P_VOLTAGE=0
P_STATE=0
while read line; do
	TIME=`echo $line | sed 's/.* \([0-9]\+\):\([0-9]\+\):\([0-9]\+\) .*/\1 \2 \3/'`
	HOUR=`pick 1 $TIME`
	MIN=`pick 2 $TIME`
	SEC=`pick 3 $TIME`
	SECONDS=$(((10#$HOUR*60 + 10#$MIN)*60+10#$SEC))
	VOLTAGE=`pick 7 $line | sed 's/,//'`
	if echo $line | grep -q "Charging"; then
		STATE=c;
	else
		STATE=d;
	fi

	echo "$SECONDS $STATE $VOLTAGE" 1>&2

	if [ $P_SECONDS -ne 0 ]; then
		# Linear interpolation
		TDIFF=$((10#$SECONDS-10#$P_SECONDS))
		VSTEP=$(((10#$VOLTAGE-10#$P_VOLTAGE)/10#$TDIFF))
		while [ $P_SECONDS -lt $SECONDS ]; do
			P_VOLTAGE=$((10#$P_VOLTAGE+10#$VSTEP))
			P_SECONDS=$((10#$P_SECONDS+1))
			echo "$P_SECONDS $STATE $P_VOLTAGE"
		done
	fi
	P_SECONDS=$SECONDS;
	P_VOLTAGE=$VOLTAGE
	P_STATE=$STATE

done <"$1"

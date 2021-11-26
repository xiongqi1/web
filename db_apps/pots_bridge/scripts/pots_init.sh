#!/bin/sh
set_roaming_rdb_variables() {
		# initialize roaming variable to default value
		rdb_set roaming.voice.outgoing.blocked 0
		rdb_set roaming.voice.incoming.blocked 0
		rdb_set roaming.data.blocked 0
}

wait_for_pots_initialized() {

		/sbin/voicecallenv.sh

		VOICE_DISABLED=`rdb_get "potsbridge_disabled"`

		if [ "$VOICE_DISABLED" = "1" ] || [ "$VOICE_DISABLED" = "" ]; then
				logger "##############################################"
				logger "pots_bridge disabled"
				logger "##############################################"
				rdb_set pots.status "pots_disabled"
				#rmmod ntc_pcmdrv
				rmmod si322x
		else

				# for factory test w/o 3G modem in 3G30WV
				# if there is no 3G modem, call pots_brige by force for calibration
				# and load pcm driver in master mode, set cpld to loop mode
				MOD=`grep Manufacturer /proc/bus/usb/devices| grep -v hcd | grep -v WLAN`
				if [ -z "$MOD" ]; then
						logger "#########################################################"
						logger "load pots_bridge by force for factory test w/o 3G modem"
						logger "#########################################################"
						/sbin/pots_bridge.sh
				fi

				logger  "waiting for pots_bridge initialized..."
				# if calibrated before, wait 10 seconds for basic calibration
				# if full calibration,wait for 34 seconds for Ring-Tip calibration
				let "TIMEOUT=60"
				while true; do
						if [ "`rdb_get pots.status`" = "pots_ready" ]; then
								logger "calibration succeeded!!"
								rdb_set pots.status "pots_initialized"
								break;
						fi
						if [ "`rdb_get pots.status`" = "pots_closed" ]; then
								logger "pots_bridge closed!!"
								break;
						fi
						echo -e -n "pots init : remaining $TIMEOUT s, atmgr : `rdb_get atmgr.status`, pots : `rdb_get pots.status`\r"
						let "TIMEOUT-=1"
						if [ "$TIMEOUT" -eq "0" ]; then
								logger "calibration failed!!"
								rdb_set pots.status "calibration timeout"
								break;
						fi
						sleep 1
				done
				echo -e -n "pots init : remaining $TIMEOUT s, atmgr : `rdb_get atmgr.status`, pots : `rdb_get pots.status`\r"
				let "TIME_ELAPSED=60-$TIMEOUT"
				logger "################################################################"
				logger "pots_bridge initialization process took $TIME_ELAPSED seconds"
				logger "################################################################"
		fi
}

set_roaming_rdb_variables
wait_for_pots_initialized

exit 0
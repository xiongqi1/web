#!/bin/sh

platform=`cat /etc/platform.txt | sed "s/platform=//"`

SKIN=`rdb_get system.skin`
BOARD=`rdb_get system.board`

if [ "$platform" = "Platypus" ]; then
	DB_GET="nvram_get"
	DB_SET="nvram_set"
	PROFILE_DB="wwan_0_telephony_profile"
else
	. /etc/variant.sh
	DB_GET="rdb_get"
	DB_SET="rdb_set"
	PROFILE_DB="wwan.0.telephony.profile"

	# initialize roaming variable to default value
	rdb_set roaming.voice.outgoing.blocked 0
	rdb_set roaming.voice.incoming.blocked 0
	rdb_set roaming.data.blocked 0

	# initialize supplementary call environment.
	/sbin/voicecallenv.sh
fi

VOICE_DISABLED=`$DB_GET "potsbridge_disabled"`

# Search for a model name started with 'MC' and remove it if found
# and save the result string MODEL_NON_SIERRA
# Assumption here is that any module its name is started with 'MC' 
# is a Sierra one.
MODEM_MODEL=`rdb_get "wwan.0.model"`
MODEL_NON_SIERRA=${MODEM_MODEL#MC}

#
# PCM clock source
#
#PCM_CLOCK_SOURCE=`rdb_get "voip.conf.pcm.clksrc"`

log() {
	logger -t pots_bridge.sh "$@"
}

log  "starting pots_bridge.sh..."

# delete locking file
killall pots_bridge
rm -f /var/lock/subsys/pots_bridge
rdb_set pots.status "pots_initializing"

if [ "$SKIN" = "ts" ]; then
	
	if [ "$INIT_RELAUNCHED" != "1" ]; then
		log  "dealing with telus pin policy..."
		teluspin
	fi
fi

if [ "$VOICE_DISABLED" = "1" ]; then
	log "pots bridge is disabled by nvram variable - potsbridge_diabled"
	if [ "$platform" = "Platypus" ]; then
		rmmod ntc_pcmdrv
		sleep 1
	fi
	rmmod si322x
	rdb_set pots.status "pots_disabled"
	exit 0
fi

# Check whether 3G module supports voice feature or not.
# Do not launch pots_bridge if the 3G module does not support voice feature
# even though V_SLIC and V_CALL_FORWARDING is defined.
VOICE_MODULE_LIST="MC8790V MC8792V MC8795V MC8704"
FOUND_MODEM=`echo $VOICE_MODULE_LIST | grep $MODEM_MODEL | grep -v grep`
if [ "$FOUND_MODEM" = "" ]; then
	log "$MODEM_MODEL does not support voice feature. Do not launch pots_bridge!"
	if [ "$platform" = "Platypus" ]; then
		rmmod ntc_pcmdrv
		sleep 1
	fi
	rmmod si322x
	rdb_set pots.status "pots_disabled"
	exit 0
fi


if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
	log  "loading si322x driver..."
	rmmod ntc_pcmdrv
	sleep 1
fi

rmmod si322x

if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
	insmod si322x
else
	# turn on slic power
	sys -s 1
	sleep 1
	insmod /lib/modules/si322x.ko
fi

if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
	PCM_MASTER=`rdb_get pots.pcm_master`
	if [ "$BOARD" = "3g8wv" ]; then
		PCM_DRIVER_OPTION="initial_clk_rate=3 "
	elif [ "$BOARD" = "3g38wv" ] || [ "$BOARD" = "3g38wv2" ] || [ "$BOARD" = "3g36wv" ] || [ "$BOARD" = "3g39wv" ] || [ "$BOARD" = "3g22wv" ] || [ "$BOARD" = "3g46" ]; then
		PCM_DRIVER_OPTION="initial_clk_rate=0 "
	fi

	if [ -n "$PCM_MASTER" ];then
		insmod ntc_pcmdrv pcm_master=$PCM_MASTER $PCM_DRIVER_OPTION
	else
		insmod ntc_pcmdrv $PCM_DRIVER_OPTION
	fi
	sleep 1
fi

telephony_profile=`$DB_GET $PROFILE_DB`

# if telephony profile is not defined, save to nvram according to skin
if [ "$telephony_profile" = "" ]; then
	if [ "$SKIN" = "ts" ] || [ "$SKIN" = "ro" ]; then
		telephony_profile="1"
	elif [ "$SKIN" = "int" ]; then
		telephony_profile="2"
	else
	telephony_profile="0"
	fi
	$DB_SET $PROFILE_DB $telephony_profile
	if [ "$platform" = "Platypus" ]; then
		rdb_set wwan.0.telephony.profile $telephony_profile
	fi
fi

log  "launching pots_bridge... (profile=$telephony_profile)"

if [ "$platform" = "Platypus" ]; then
	pots_bridge -vvvv -i 0 -t $telephony_profile /dev/slic0 /dev/slic1
else
	pots_bridge -vvvv -i 0 -t $telephony_profile
fi

# after initializing pots_bridge, start monitoring pots_bridge
# to restart if pots_bridge stop
killall -9 monitor_pots_bridge.sh
(sleep 60 && monitor_pots_bridge.sh)&

exit 0

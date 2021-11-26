#!/bin/sh
platform=`cat /etc/platform.txt | sed "s/platform=//"`

SKIN=`rdb_get system.skin`
BOARD=`rdb_get system.board`

if [ "$platform" = "Platypus" ]; then
# if defined GENERATED_BY_PROSLIC_V620
echo "*******************************************************************************"
echo "*   pots_reload.sh for pots_bridge and slic[Si3226] debug                     *"
echo "*                                                                             *"
echo "*   usage : pots_reload -d [slic debug mode] -l [line impedance table number] *"
echo "*                                                                             *"
echo "*           slic debug mode : '1' to display slic log message                 *"
echo "*                                                                             *"
echo "*           line impedance table number :                                     *"
used_2nd_gen_proslic=1
if [ "$used_2nd_gen_proslic" = "1" ]; then
# if defined GENERATED_BY_2ND_GEN_PROSLIC
echo "*                  generated by  BY_2ND_GEN_PROSLIC                           *"
echo "*                  0  : 600 ohm                                               *"
echo "*                  1  : 600 ohm + 1000 nF                                     *"
echo "*                  2  : 600 ohm + 2160 nF                                     *"
echo "*                  3  : 900 ohm                                               *"
echo "*                  4  : 900 ohm + 1000 nF                                     *"
echo "*                  5  : 900 ohm + 2160 nF                                     *"
echo "*                  6  : 120 ohm + (820 ohm  || 110 nF)                        *"
echo "*                  7  : 200 ohm + (680 ohm  || 100 nF)                        *"
echo "*                  8  : 220 ohm + (820 ohm  || 115 nF)                        *"
echo "*                  9  : 220 ohm + (820 ohm  || 120 nF)                        *"
echo "*                  10 : 270 ohm + (750 ohm  || 150 nF)                        *"
echo "*                  11 : 275 ohm + (780 ohm  || 115 nF)                        *"
echo "*                  12 : 275 ohm + (780 ohm  || 150 nF)                        *"
echo "*                  13 : 320 ohm + (1050 ohm || 230 nF)                        *"
echo "*                  14 : 350 ohm + (1000 ohm || 210 nF)                        *"
echo "*                  15 : 370 ohm + (820 ohm  || 110 nF)                        *"
echo "*                  16 : 370 ohm + (620 ohm  || 310 nF)                        *"
else
# if defined GENERATED_BY_PROSLIC_V620
echo "*                  generated by  BY_PROSLIC_V620                              *"
echo "*                  0  : 110 ohm + (820 ohm || 110 nF)                         *"
echo "*                  1  : 150 ohm + (510 ohm || 47 nF)                          *"
echo "*                  2  : 150 ohm + (830 ohm || 72 nF)                          *"
echo "*                  3  : 180 ohm + (630 ohm || 60 nF)                          *"
echo "*                  4  : 200 ohm + (560 ohm || 100 nF)                         *"
echo "*                  5  : 200 ohm + (680 ohm || 100 nF)                         *"
echo "*                  6  : 200 ohm + (1000 ohm || 100 nF)                        *"
echo "*                  7  : 215 ohm + (1000 ohm || 137 nF)                        *"
echo "*                  8  : 220 ohm + (820 ohm || 115 nF)                         *"
echo "*                  9  : 220 ohm + (820 ohm || 120 nF)                         *"
echo "*                  10 : 220 ohm + (820 ohm || 150 nF)                         *"
echo "*                  11 : 270 ohm + (750 ohm || 150 nF)                         *"
echo "*                  12 : 270 ohm + (910 ohm || 120 nF)                         *"
echo "*                  13 : 290 ohm + (925 ohm || 165 nF)                         *"
echo "*                  14 : 293 ohm + (1080 ohm || 230 nF)                        *"
echo "*                  15 : 300 ohm + (1000 ohm || 220 nF)                        *"
echo "*                  16 : 320 ohm + (1050 ohm || 230 nF)                        *"
echo "*                  17 : 350 ohm + (1000 ohm || 210 nF)                        *"
echo "*                  18 : 370 ohm + (620 ohm || 310 nF)                         *"
echo "*                  19 : 400 ohm + (500 ohm || 50 nF)                          *"
echo "*                  20 : 400 ohm + (500 ohm || 300 nF)                         *"
echo "*                  21 : 400 ohm + (500 ohm || 330 nF)                         *"
echo "*                  22 : 400 ohm + (700 ohm || 200 nF)                         *"
echo "*                  23 : 600 ohm + (0 ohm || 0 nF)                             *"
echo "*                  24 : 600 ohm + (INF ohm || 1000 nF)                        *"
echo "*                  25 : 600 ohm + (INF ohm || 1500 nF)                        *"
echo "*                  26 : 600 ohm + (INF ohm || 2160 nF)                        *"
echo "*                  27 : 900 ohm + (0 ohm || 0 nF)                             *"
echo "*                  28 : 900 ohm + (INF ohm || 2160 nF)                        *"
echo "*                  29 : 1200 ohm + (0 ohm || 0 nF)                            *"
fi
echo "*                                                                             *"
echo "*******************************************************************************"
echo ""

else
echo "*******************************************************************************"
echo "*   pots_reload.sh for pots_bridge and slic[Si32171] debug                    *"
echo "*                                                                             *"
echo "*   usage : pots_reload -d [slic debug mode] -l [line impedance table number] *"
echo "*                                                                             *"
echo "*           slic debug mode : '1' to display slic log message                 *"
echo "*                                                                             *"
echo "*           line impedance table number :                                     *"
echo "*                  0  : 150 ohm + (510 ohm || 47 nF)                          *"
echo "*                  1  : 180 ohm + (630 ohm || 60 nF)                          *"
echo "*                  2  : 200 ohm + (560 ohm || 100 nF)                         *"
echo "*                  3  : 200 ohm + (680 ohm || 100 nF)                         *"
echo "*                  4  : 215 ohm + (1000 ohm || 137 nF)                        *"
echo "*                  5  : 220 ohm + (820 ohm || 115 nF)                         *"
echo "*                  6  : 220 ohm + (820 ohm || 120 nF)                         *"
echo "*                  7  : 270 ohm + (750 ohm || 150 nF)                         *"
echo "*                  8  : 293 ohm + (1080 ohm || 230 nF)                        *"
echo "*                  9  : 300 ohm + (1000 ohm || 220 nF)                        *"
echo "*                  10 : 350 ohm + (1000 ohm || 210 nF)                        *"
echo "*                  11 : 370 ohm + (620 ohm || 310 nF)                         *"
echo "*                  12 : 400 ohm + (500 ohm || 50 nF)                          *"
echo "*                  13 : 400 ohm + (500 ohm || 300 nF)                         *"
echo "*                  14 : 400 ohm + (500 ohm || 330 nF)                         *"
echo "*                  15 : 400 ohm + (700 ohm || 200 nF)                         *"
echo "*                  16 : 600 ohm + (0 ohm || 0 nF)                             *"
echo "*                  17 : 600 ohm + (INF ohm || 1000 nF)                        *"
echo "*                  18 : 600 ohm + (INF ohm || 2160 nF)                        *"
echo "*                  19 : 900 ohm + (0 ohm || 0 nF)                             *"
echo "*                  20 : 900 ohm + (INF ohm || 2160 nF)                        *"
echo "*                                                                             *"
echo "*******************************************************************************"
echo ""
fi

SLIC_DRV_OPT=""

while [ "$1" ]; do
	if [ "$1" = "-d" ]; then
		SLIC_DRV_OPT=$SLIC_DRV_OPT" slic_debug_mode=$2"
		shift 2
	elif [ "$1" = "-l" ]; then
		SLIC_DRV_OPT=$SLIC_DRV_OPT" slic_li_index=$2"
		shift 2
	elif [ "$1" = "--help" ]; then
		exit 0
	else
		SLIC_DRV_OPT=$SLIC_DRV_OPT" $1"
		shift 1
	fi
done

if [ "$platform" = "Platypus" ]; then
		DB_GET="nvram_get"
		DB_SET="nvram_set"
		PROFILE_DB="wwan_0_telephony_profile"
else
		DB_GET="rdb_get"
		DB_SET="rdb_set"
		PROFILE_DB="wwan.0.telephony.profile"
fi

killall pots_bridge
sleep 2

if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
		rmmod ntc_pcmdrv
		sleep 1
fi

rmmod si322x
sleep 1

if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
		insmod si322x $SLIC_DRV_OPT
else
		# turn on slic power
		sys -s 1
		sleep 1
		insmod /lib/modules/si322x.ko $SLIC_DRV_OPT
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
fi

telephony_profile=`$DB_GET $PROFILE_DB`

# if telephony progile is not defined, save to nvram according to skin
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

echo  "launching pots_bridge... (profile=$telephony_profile)"

if [ -e "/var/lock/subsys/pots_bridge" ]; then
	rm /var/lock/subsys/pots_bridge
fi

if [ "$platform" = "Platypus" ]; then
		if [ "$1" = "0" ] || [ "$1" = "1" ]; then
				pots_bridge -vvvv -i 0 -t $telephony_profile /dev/slic$1
		else
				pots_bridge -vvvv -i 0 -t $telephony_profile /dev/slic0 /dev/slic1
		fi
else
		pots_bridge -vvvv -i 0 -t $telephony_profile
fi

# wait for pots_bridge initializing including calibration
let "timeout=60"
rdb_set pots.status
while true;do
		pots_init=`rdb_get pots.status`
		echo "pots_bridge initializing : remaining = $timeout s"
		test "$pots_init" = "pots_ready" && echo "pots_bridge is initialized" && break
		let "timeout-=1"
		test "$timeout" = "0" && echo "pots_bridge initialization timeout! Voice feature may not work" && break
		sleep 1
done

exit 0 

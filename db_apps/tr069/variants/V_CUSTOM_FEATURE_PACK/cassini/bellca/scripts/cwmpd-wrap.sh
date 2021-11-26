#!/bin/sh
#
# CWMP Lua Daemon Re-start Wrapper
#

nof=${0##*/}
source /lib/utils.sh

# generate a (16-bit) random value with upper limit
rand_limit() {
	local lim="$1"
	local val=$(head -c2 /dev/urandom) # get two random bytes
	local lsb=$(printf '%d' "'${val:0:1}")
	local msb=$(printf '%d' "'${val:1:1}")
	let "val=(msb*256+lsb)%(lim+1)"
	echo "$val"
}

# If tr069.server.random_inform.enable = 1, delay launching cwmpd by
# a random time within a window given by rdb variable
# tr069.server.random_inform.window
prog="$1"
cwmpd="cwmpd.lua"
if [ -z "${prog%%*$cwmpd}" ]; then # only delay for cwmpd
	enable=$(rdb_get service.tr069.enable)
	if [ "$enable" != "1" ]; then
		exit 0
	fi
	randinfo_en=$(rdb_get tr069.server.random_inform.enable)
	if [ "$randinfo_en" = "1" ]; then
		randinfo_win=$(rdb_get tr069.server.random_inform.window)
		case "$randinfo_win" in
			''|*[!0-9]*)
				logWarn "Bad random inform window ignored"
				;;
			*)
				snextra=$(rdb_get uboot.snextra)
				echo "$snextra" > /dev/urandom # seed with encrypted serial no.
				secs=$(rand_limit "$randinfo_win")
				logInfo "Sleeping for $secs seconds before launching $1"
				sleep "$secs"
				logInfo "Woken up. Launching $1 now"
				;;
		esac
	fi
fi

enable=$(rdb_get service.tr069.enable)

# Check WWAN network status before going into main loop.
# This is to avoid unnecessary "4 VALUE CHANGE" inform session to report ConnectionRequestURL.
# Also, invalid ConnectionRequestURL in "0 BOOTSTRAP" or "1 BOOT" inform sesion causes
# Connection Request Message failure.
sleep_dur=1
sleep_cnt=10
MAX_DUR=10
MAX_CNT=10
net_status="down"
band_revert="enabled"
extra_delay="0"
while  [ "${enable}" = "1" ] && [ "${net_status}" = "down" -o "${band_revert}" = "enabled" ]; do
	case $(rdb dump link.policy | grep status_ipv | awk '{print $3}') in
		up*) net_status="up" ;;
	esac
	case $(rdb get wwan.0.currentband.revert_selband.mode) in
		"factory_setting"|"last_good_setting"|"customized_band_setting") band_revert="enabled" ;;
		*) band_revert="disabled" ;;
	esac
	if [ "${enable}" = "1" ] && [ "${net_status}" = "down" -o "${band_revert}" = "enabled" ]; then
		extra_delay="1"
		# increase sleep interval
		if [ "${sleep_dur}" -lt "${MAX_DUR}" ]; then
			sleep_cnt=$(( sleep_cnt - 1 ))
			if [ "${sleep_cnt}" -lt 0 ]; then
				sleep_cnt=${MAX_CNT}
				sleep_dur=$(( sleep_dur + 1))
			fi
		fi
		sleep ${sleep_dur}
		enable=$(rdb get service.tr069.enable)
	elif [ "${enable}" = "1" -a "${net_status}" = "up" ]; then
		logWarn "Network is up: start $1"
	fi
done

# Applied extra delay(30 seconds) only the case network re-starts up
if [ "${extra_delay}" = "1" -a "$(rdb get service.ip_handover.enable)" = "1" ]; then
	logWarn "Apply extra delay for IP Handover"
	sleep 30
fi

while [ "$enable" == "1" ]; do
	$1 $2
	if [ $? -ne 0 ] ; then
		logWarn "Process exited with a failure, respawning..."
	else
		logInfo "Process exited normally."
		exit 0
	fi
	# TODO:: sleep duration seems to be unreasonably long, though.
	#        Could not find appropriate reason for this. Need to re-estimate the sleep duration
	#	 or re-design this loop.
	enable=$(rdb get service.tr069.enable)
	if [ "$enable" = "1" ]; then
		sleep 10
		enable=$(rdb get service.tr069.enable)
	fi
done

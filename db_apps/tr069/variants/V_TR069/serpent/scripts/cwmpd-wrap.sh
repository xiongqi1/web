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
while [ "$enable" == "1" ]; do
	# TR-069 client will not start if required WWAN interface is not ready
	# Search for default route
	PF=$(rdb_get "tr069.wan.profile.required")
	if [ "$PF" = "0" -o "$PF" = "" ]; then
		for ix in 1 2 3 4 5 6
		do
			defaultroute=$(rdb_get link.profile.${ix}.defaultroute)
			if [ "$defaultroute" = "1" ]; then
				PF=$ix
				break;
			fi
		done
	fi

	if [ -n "$PF" ]; then
		#check for valid WWAN interface
		PROFILEDEV=$(rdb get link.profile.$PF.interface)
		PROFILEADDR=$(rdb get link.profile.$PF.iplocal)
		if [ -n "$PROFILEDEV" -a -n "$PROFILEADDR" ]; then
			INFIPADDR=$(ifconfig $PROFILEDEV | awk ' $1 == "inet" { split($2,ipaddr,":"); print ipaddr[2] }')
			# non-handeover mode: WWAN interface address should be same as the link profile IP address
			# handovce mode: need more dedicate check.
			handover=$(rdb_get service.ip_handover.enable)
			handover_pf=$(rdb_get service.ip_handover.profile_index)
			if [ "$handover" = "1" -a "$handover_pf" = "$PF" ]; then
				WWANADDR=$(rdb_get service.ip_handover.last_wwan_ip)
				FAKEWWANADDR=$(rdb_get service.ip_handover.fake_wwan_address)
				test "$PROFILEADDR" = "$WWANADDR" -a "$INFIPADDR" = "$FAKEWWANADDR"
			else
				test "$PROFILEADDR" = "$INFIPADDR"
			fi
			if [ $? -eq 0 ]; then
				#RUN
				$1 $2
				if [ $? -ne 0 ] ; then
					logWarn "Process exited with a failure, respawning..."
				else
					logInfo "Process exited normally."
					exit 0
				fi
			fi
		fi
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

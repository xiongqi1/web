#!/bin/sh
#
# CWMP Lua Daemon Re-start Wrapper
#
# $1: a program or script to run
# $2: optional parameter

log() {
  logger -t cwmpd-wrap.sh -- $@
}

# Catch the TERM signal and reset iptables
trap _term SIGTERM

# With current implementation, it is not possible to retain iptables configuration
# until tr069 client is terminated, though.
# Only predicted issue is tr069 client could not complete ongoing session.
# But no matter, because the service is already disabled.
# Also tr069 client can gently handle this case internally.
_term() {
	if [ "$tmp_qos_set" = "1" ]; then
		iptables -t mangle -D OUTPUT -p tcp --dport $tmp_http_url_port -j  O_M_tr069
		iptables -t mangle -D OUTPUT -p tcp --sport $tmp_connreq_port -j  O_M_tr069
	fi
	exit 0
}

enable=`rdb get service.tr069.enable`
tmp_qos_set=0
while [ "$enable" == "1" ]; do
	# TR-069 client will not start in the rf qualfication mode or there is no rf_alignment console
	RFMODE=$(rdb_get service.wmmd.mode)
	if [ -n "$RFMODE" -a "$RFMODE" != "rf_qualification" ]; then
		# also, RDM PDN interface should be ready (i.e. IPv4 address should be assigned first) and
		# its IPv4 address should be same as the link profile 1's IP address.
		# Currently for wntdv3 link.profile.1.interface is equivalent to link.profile.1.dev and
		# link.profile.1.iplocal is equivalent to link.profile.1.address
		PROFILE1DEV=`rdb get link.profile.1.interface`
		PROFILE1ADDR=`rdb get link.profile.1.iplocal`
		if [ -n "$PROFILE1DEV" -a -n "$PROFILE1ADDR" ]; then
			INFIPADDR=`ifconfig $PROFILE1DEV | awk ' $1 == "inet" { split($2,ipaddr,":"); print ipaddr[2] }'`
			if [ "$PROFILE1ADDR" = "$INFIPADDR" ]; then

				if [ "$3" = "set_qos" ]; then
					tmp_http_url=`rdb get tr069.server.url`
					if [ "$tmp_http_url" = "" ]; then
						tmp_http_url=`grep "tr069.server.url" /etc/tr-069.conf | grep -v ^# | cut -d\" -f 4`
					fi
					tmp_http_url_host=`echo $tmp_http_url | cut -d/ -f 3`
					tmp_http_url_port=`echo $tmp_http_url_host | cut -d: -f 2`
					if [ "$tmp_http_url_port" = "$tmp_http_url_host" ]; then
						tmp_http_url_proto=`echo $tmp_http_url | cut -d: -f 1`
						if [ "$tmp_http_url_proto" = "https" ]; then
							tmp_http_url_port=443
						elif [ "$tmp_http_url_proto" = "http" ]; then
							tmp_http_url_port=80
						else
							tmp_http_url_port=""
						fi
					fi
					if [ -n "$tmp_http_url_port" ]; then
						tmp_connreq_port=`rdb get tr069.request.port`
						iptables -t mangle -A OUTPUT -p tcp --dport $tmp_http_url_port -j  O_M_tr069
						iptables -t mangle -A OUTPUT -p tcp --sport $tmp_connreq_port -j  O_M_tr069
						log "INFO: tr-069 traffic qos set api has been called : ports: d: $tmp_http_url_port s: $tmp_connreq_port return code: $?"
						tmp_qos_set=1
					else
						log "WARNING: tr-069 traffic qos cannot be set: incorrect port number or protocol not supported from $tmp_http_url"
					fi
				fi

				#RUN
				$1 $2

				if [ "$tmp_qos_set" = "1" ]; then
					iptables -t mangle -D OUTPUT -p tcp --dport $tmp_http_url_port -j  O_M_tr069
					iptables -t mangle -D OUTPUT -p tcp --sport $tmp_connreq_port -j  O_M_tr069
					tmp_qos_set=0
				fi
				log "Process ${1##*/} exited with exit code $?. Respawning..."
			else
				log "WARNING: $PROFILE1DEV interface IP address $INFIPADDR != $PROFILE1ADDR profile IP address"
			fi
		fi
	fi

	# TODO:: sleep duration seems to be unreasonably long, though.
	#        Could not find appropriate reason for this. Need to re-estimate the sleep duration
	#	 or re-design this loop.
	enable=$(rdb get service.tr069.enable)
	if [ "$enable" = "1" ]; then
		sleep 20
		enable=$(rdb get service.tr069.enable)
	fi
done

_term


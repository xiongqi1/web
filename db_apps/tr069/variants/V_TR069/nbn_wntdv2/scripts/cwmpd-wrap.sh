#!/bin/sh
#
# CWMP Lua Daemon Re-start Wrapper
#
# $1: a Lua script to run
# $2: optional parameter

log() {
  /usr/bin/logger -t cwmpd-wrap.sh -- $@
}

enable=`rdb get service.tr069.enable`
tmp_qos_set=0
while [ "$enable" == "1" ]; do
	# TR-069 client will not start in the rf qualfication mode or there is no rf_alignment console
	RFMODE=`rdb get service.dccd.mode`
	if [ -n "$RFMODE" -a "$RFMODE" != "rf_qualification" ]; then
		# also, RDM PDN interface should be ready (i.e. IPv4 address should be assigned first) and
		# its IPv4 address should be same as the link profile 1's IP address.
		PROFILE1DEV=`rdb get link.profile.1.dev`
		PROFILE1ADDR=`rdb get link.profile.1.address`
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
				log "Process exited with exit code $?. Respawning..."
			else
				log "WARNING: $PROFILE1DEV interface IP address $INFIPADDR != $PROFILE1ADDR profile IP address"
			fi
		fi	
	fi
	enable=`rdb get service.tr069.enable`
	test "$enable" == "1" && sleep 20
done

if [ "$tmp_qos_set" = "1" ]; then
	iptables -t mangle -D OUTPUT -p tcp --dport $tmp_http_url_port -j  O_M_tr069
	iptables -t mangle -D OUTPUT -p tcp --sport $tmp_connreq_port -j  O_M_tr069
fi


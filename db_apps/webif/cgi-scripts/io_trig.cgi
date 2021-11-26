#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

. /etc/variant.sh 2>/dev/null
if [ "$V_WEBIF_VERSION" = "v2" ]; then
	# CSRF token must be valid
	if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
		exit 254
	fi
fi

. /lib/utils.sh

script=$(basename "$0")
log() {
	logger -t "$script" -- "$@"
}

parse_post_data "reqtype trig_en trig_delay trig_pin trig_duration trig_level trig_profile"

# Get the device node and name (but don't get any other params yet!)
echo -e "Status: 200\nContent-type: text/plain\nPragma: no-cache\nCache-Control: no-cache\n\n"

cgi_read_io_mode() {
	echo "["
	idx=1
	while true; do
		var="sys.sensors.io.xaux$idx.mode"
		val=$(rdb_get $var)
		test -z "$val" && break
		test $idx -eq 1 || echo ","
		echo "{\"mode\":\"$val\"}"
		idx=$((idx+1))
	done
	echo "]"
}

validation_check() {
	new_val=$2
	[ ! -z "${new_val##*[!0-9]*}" ] && is_num="1" || is_num="0"
	if [ -z "$new_val" -o $is_num = "0" ]; then
		log "error : $1 should be number"
		echo "{}"
		exit 1
	fi
	if [ $new_val -lt $3 -o $new_val -gt $4 ]; then
		log "error : $1 should be between $3 and $4"
		echo "{}"
		exit 1
	fi
}

cgi_write_io_trig() {
	# write RDB variables after all changes are validated
	validation_check "service.failover.x.io_trig.en" $trig_en 0 1
	validation_check "service.failover.x.io_trig.delay" $trig_delay 0 1440
	last_pin=$(rdb_get sys.sensors.info.lastio)
	validation_check "service.failover.x.io_trig.pin" $trig_pin 1 $last_pin
	validation_check "service.failover.x.io_trig.duration" $trig_duration 1 10
	validation_check "service.failover.x.io_trig.level" $trig_level 0 1

	# update RDB variables
	rdb_set service.failover.x.io_trig.en $trig_en
	rdb_set service.failover.x.io_trig.delay $trig_delay
	rdb_set service.failover.x.io_trig.pin $trig_pin
	rdb_set service.failover.x.io_trig.duration $trig_duration
	rdb_set service.failover.x.io_trig.level $trig_level
	rdb_set service.failover.x.io_trig.profile $trig_profile

	# change IO pin status now
	#   positive : set to low
	#   negative : set to high
	if [ "$trig_en" = "1" ]; then
		io_ctl set_mode xaux$trig_pin digital_output
		if [ "$trig_level" = "1" ]; then
			io_ctl write_digital xaux$trig_pin 0
		else
			io_ctl write_digital xaux$trig_pin 1
		fi
	fi

	echo "{}"
}

case $reqtype in
	read_io_mode|write_io_trig)
		cgi_${reqtype}
		;;
	*)
		exit 1
		;;
esac

exit 0
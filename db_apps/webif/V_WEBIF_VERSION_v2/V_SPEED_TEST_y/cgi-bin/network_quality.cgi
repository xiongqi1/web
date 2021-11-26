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
	logNotice -t "$script" -- "$@"
}

parse_post_data "reqtype"

echo -e "Status: 200\nContent-type: text/plain\nPragma: no-cache\nCache-Control: no-cache\n\n"

cgi_run_speed_test() {

	# check whether already running another instance
	RUNNING=$(pidof speed_test.sh)
	if [ -n "$RUNNING" ]; then
		logErr "Another speed test application is already running"
		echo "{}"
		return
	fi

	# trigger speed_test.template which calls all necessary scripts in series as below;
	# 	network_speed.cgi --> speed_test.template --> speed_test.sh --> speedtest-cli
	# and below RDB variables updated after speed test
	# ex) service.speedtest.datetime Tue Dec 20 02:14:16 GMT 2016
	#     service.speedtest.download 5.82 Mbit/s
	#     service.speedtest.server AARNet (Sydney) [67.26 km]
	#     service.speedtest.latency 86 ms
	#     service.speedtest.upload 1.03 Mbit/s
	rdb_set service.speedtest.result
	rdb_set service.speedtest.trigger 1

	# speed test will take couple of minutes in good network
	TIMEOUT=600
	while [ $TIMEOUT -gt 0 ]; do
		RESULT=$(rdb_get service.speedtest.result)
		test "$RESULT" = "done" -o "$RESULT" = "error" && break
		sleep 1
		TIMEOUT=$((TIMEOUT-1))
	done
	echo "{}"
}

case $reqtype in
	run_speed_test)
		cgi_${reqtype}
		;;
	*)
		exit 1
		;;
esac

exit 0
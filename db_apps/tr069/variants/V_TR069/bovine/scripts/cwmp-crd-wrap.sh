#!/bin/sh
#
# CWMP-CRD Daemon Re-start Wrapper
#
# Copyright (C) 2018 NetComm Wireless Limited.

nof=${0##*/}
source /lib/utils.sh

script=$(basename "$0")

print_usage() {
	cat << EOF

This script is CWMP connection request daemon wrapper

usage>
	$script "cwmp-crd daemon" "port num of daemon"

EOF
}

if [ "$1" = "-h" -o "$1" = "--help" ]; then
	print_usage >&2
	return 1
fi

cmd="$@"
child_pid=""

# Catch the TERM signal and terminate cwmp-crd daemon
trap _term SIGTERM

_term() {
	if [ -n "$child_pid" ]; then
		kill $child_pid
		logInfo "Terminated CWMP crd daemon(PID: $child_pid)"
	fi
	exit 0
}

enable=$(rdb_get service.tr069.enable)

while [ "$enable" == "1" ]; do
	${cmd} &

	child_pid="$!"
	logInfo "start CWMP crd daemon (PID: $child_pid)"
	wait $child_pid

	enable=$(rdb get service.tr069.enable)
	if [ "$enable" = "1" ]; then
		sleep 1
		enable=$(rdb get service.tr069.enable)
	fi
done

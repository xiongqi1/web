#!/bin/sh

DB_CMD="wwan.0.umts.services.command"
DB_CMD_ST="wwan.0.umts.services.command.status"
DB_RESULT="wwan.0.umts.services.command.response"

print_usage() {
	cat << EOF

Easy CLI command - AT command terminal 

	atcmd_term.sh [auto|direct|atmgr|help]

	option:
		atmgr:		force to access via AT manager (default)
		auto:		access via AT manager if AT manager is running. Otherwise, directly access AT port
		direct:		force to directly access AT port (from rdb variable - wwan.0.V250_if.1)

		help:		print this usage screen

EOF
}

err() {
	echo -n "$@" >&2
	echo -ne "\r\n"
}

puts() {
	echo -n "$@"
	echo -ne "\r\n"
}

lsof() {
	lsof_pids=$(ls -l $(find /proc/[0-9]*/fd -type l 2> /dev/null ) 2> /dev/null | grep "$1" | sed -n 's/.* \/proc\/\([0-9]\+\)\/fd\/.*/\1/p')
	echo "$lsof_pids"

	if [ -z "$lsof_pids" ]; then
		return 1
	else
		return 0
	fi
}

suspend_pids=""
suspend_processes="supervisor connection_mgr"

display_atmgr_tasks() {
	pids=$(lsof "$ATPORT")
	if [ -n "$pids" ]; then
		pids="$pids $(pidof $suspend_processes)"
	else
		pids="$(pidof $suspend_processes)"
	fi

	for pid in $pids; do
		err "process running ' [$pid] $(cat /proc/$pid/cmdline | tr '\0' ' ')' "
	done
}

suspend_atmgr_tasks() {
	
	suspend_pids=$(lsof "$ATPORT")
	if [ -n "$suspend_pids" ]; then
		suspend_pids="$suspend_pids $(pidof $suspend_processes)"
	else
		suspend_pids="$(pidof $suspend_processes)"
	fi

	for pid in $suspend_pids; do
		err "sending SIGSTOP to ' [$pid] $(cat /proc/$pid/cmdline | tr '\0' ' ')' "
	done

	if [ -n "$suspend_pids" ];then
		kill -stop $suspend_pids 2> /dev/null
	fi
}

resume_atmgr_tasks() {
	for pid in $suspend_pids; do
		err "sending SIGCONT to ' [$pid] $(cat /proc/$pid/cmdline | tr '\0' ' ')' "
	done

	if [ -n "$suspend_pids" ];then
		kill -cont $suspend_pids 2> /dev/null
	fi
}

# get access type
ACCESS_TYPE="$1"
if [ -z "$ACCESS_TYPE" ]; then
	ACCESS_TYPE="atmgr"
fi

# get pid of atmgr
. /etc/variant.sh

if [ "$V_USE_DCCD" = "y" ]; then
	ATMGR_PID=$(pidof dccd 2> /dev/null)
else
	ATMGR_PID=$(pidof simple_at_manager 2> /dev/null)
fi

ATPORT=$(rdb_get "wwan.0.V250_if.1")
ATEMU="false"

case "$ACCESS_TYPE" in
	'auto')
		if [ -n "$ATMGR_PID" ]; then
			puts "AT port manager detected - emulation mode on"
			ATEMU=true
		else
			puts "AT port manager not detected - directly access mode on"
			ATEMU=false
		fi
		;;

	'atmgr')
		err "emulation mode on - send commands via AT manager"
		ATEMU=true
		;;

	'direct')
		err "emulation mode off - send commands directly to AT port"
		ATEMU=false
		;;

	*)
		print_usage
		exit 0
		;;
esac

CAT_IN_BG=""

# check at port rdb varaible
if [ -z "$ATPORT" ]; then
	err "RDB AT port (wwan.0.V250_if.1) not available"
	exit 1
fi 

# check at port access
if [ ! -c "$ATPORT" ]; then
	err "AT port ($ATPORT) not available or not accessible"
	exit 1
fi

# terminate at port processes
if $ATEMU; then

	display_atmgr_tasks

	if [ -z "$ATMGR_PID" ]; then
		err "AT manager is not running"
		exit 1
	fi

else
	suspend_atmgr_tasks

	stty -F "$ATPORT" raw -echo
	cat "$ATPORT" < /dev/null &

	CAT_IN_BG=$!
fi


put_crlf_at_the_end() {
	echo -n "$1"
	echo -ne "\r\n"
}

send_at_command_directly() {
	atcmd="$1"
	if [ -z "$atcmd" ]; then
		return 1
	fi

	put_crlf_at_the_end "$atcmd" > "$ATPORT"

	return 0
}

send_at_command_via_atmgr() {
	atcmd="$1"
	if [ -z "$atcmd" ]; then
		echo -en "\n\nERROR\r\n"
		return 1
	fi

	# clear command result and status
	rdb_set "$DB_RESULT" ""
	rdb_set "$DB_CMD_ST" ""

	# send command
	rdb_set "$DB_CMD" "$atcmd"

	# wait until the command is done
	i=0
	while [ $i -lt 10 ]; do
		st=$(rdb_get "$DB_CMD_ST")
		test -n "$st" && break
		sleep 1
	done

	st=$(rdb_get "$DB_CMD_ST")
	if [ -z $st ]; then
		echo -en "\n\nTIMEOUT\r\n"
		return 1
	fi

	echo -en "$atcmd\r\n"
	rdb_get "$DB_RESULT" 2> /dev/null

	echo -en "\n\nOK\r\n"
	return 0
}

clean_up() {
	if [ -n "$CAT_IN_BG" ]; then
		kill $CAT_IN_BG 2> /dev/null
	fi

	# reset cns heartbeat - for supervisor not to reset immediately
	rdb_set "wwan.0.heart_beat" 0

	resume_atmgr_tasks

	puts "Finished."
	puts ""
	exit
}

trap clean_up SIGHUP SIGINT SIGTERM
	
puts "Press CTRL-D to exit."
puts "Ready"
puts ""

while read line; do
	if [ -z "$line" ]; then
		continue
	fi

	if $ATEMU; then
		send_at_command_via_atmgr "$line"
	else
		send_at_command_directly "$line"
	fi
done

clean_up

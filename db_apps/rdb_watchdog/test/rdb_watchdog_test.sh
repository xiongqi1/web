#!/bin/sh

show() {
	echo "[$(date +%H:%M:%S)] $@"
}

usage() {
cat <<EOM
Usage:
    unit_test.sh TEST_CASE KICK_DURATION EXPIRE_DURATION
    TEST_CASE         Specify which test case will run. Two test cases are available as follows:
                       system - test against the sys.watchdog.timeToReboot variable
                       app - test against RDB variables for applications
    KICK_DURATION     Specifiy how often the watchdog daemon kicks the watchddog device
    EXPIRE_DURATION   Specifiy how soon the watchdog timer expires if it isn't kicked

    e.g.)
    unit_test.sh system 5 15
    unit_test.sh app 5 15
EOM
	exit 1
}

TEST_CASE=${1:-system}
KICK_DURATION=${2:-5}
EXPIRE_DURATION=${3:-15}

# need 3 arguments to proceed tests
test $# != 3 && usage

# simulate /etc/rdb_watchdog.conf
build_app_config() {
	show "Configure sys.watchdog.testCount $1"
cat <<EOM > /tmp/rdb_watchdog.conf
sys.watchdog.testCount $1
EOM
}

# Test the watchdog daemon meets the following requirements
#  The system can cancel the reboot when sys.watchdog.timeToReboot is set to -1.
#  The system can reboot when sys.watchdog.timeToReboot is set.
test_system_var() {
	local count
	local remaining_time
	local time_to_reboot=$1

	show "=== SYSTEM RDB TEST STARTED === "

	# Clear any existing commit timers
	rdb_set sys.watchdog.last_reboot_cause ""
	rdb set sys.watchdog.last_reboot_count 0
	rdb set sys.watchdog.reboot_cause ""
	rdb set sys.watchdog.queuedTimers "{}"

	# test whether the reboot can be cancelled
	remaining_time=$(($time_to_reboot/2))
	show "Set sys.watchdog.timeToReboot $time_to_reboot secs"
	rdb_set sys.watchdog.timeToReboot $time_to_reboot
	show "Sleep for $remaining_time secs"
	sleep $remaining_time
	remaining_time=$(($time_to_reboot*2))
	show "Cancel the reboot and sleep for $remaining_time secs"
	rdb_set sys.watchdog.timeToReboot -- -1
	sleep $remaining_time
	show "The system is still running"
	show "== REBOOT CANCELLING OK === "

	# test whether the system reboots when the timeout expires
	show "Set sys.watchdog.timeToReboot $time_to_reboot secs"
	rdb_set sys.watchdog.timeToReboot $time_to_reboot
	show "Sleep for $time_to_reboot secs"
	sleep $time_to_reboot

	count=$(($KICK_DURATION*2))
	while [ $count -gt 0 ]; do
		remaining_time=`rdb_get sys.watchdog.timeToReboot`
		if [ $remaining_time -eq 0 ]; then
			if logread | grep -q 'watchdog reboot' ; then
				show "=== REBOOT EXECUTED OK === "
				return 0
			fi
		fi
		count=$(($count-1))
		sleep 1
	done
	return 1
}

# Test the watchdog daemon meets the following requirements
#  The system runs as long as the sys.watchdog.testCount is changed.
#  The system can reboot if the sys.watchdog.testCount hasn't been changed for more than 30 secs.
test_app_var() {
	local count
	local sleep_time
	local time_to_reboot=$1

	show "=== APPILICATION RDB TEST STARTED === "
	build_app_config $time_to_reboot

	killall rdb_watchdog
	{ /sbin/rdb_watchdog -t $KICK_DURATION -T $EXPIRE_DURATION -w /dev/watchdog \
		-r /tmp/rdb_watchdog.conf -s  > /dev/null 2>&1 < /dev/null & } &

	sleep_time=$(($time_to_reboot/2))
	count=5
	while [ $count -gt 0 ]; do
		show "Update sys.watchdog.testCount"
		rdb_set sys.watchdog.testCount $count
		sleep $sleep_time
		count=$(($count-1))
	done

	show "Stop updating sys.watchdog.testCount and sleep for $1 secs"
	sleep $time_to_reboot

	count=$(($KICK_DURATION*2))
	while [ $count -gt 0 ]; do
		if logread | grep -q 'watchdog reboot' ; then
			show "=== REBOOT EXECUTED OK === "
			return 0
		fi
		count=$(($count-1))
		sleep 1
	done
	return 1
}

case $TEST_CASE in
system)
	test_system_var 30
	;;

app)
	test_app_var 30
	;;
*)
	usage
	;;
esac

if [ $? -eq 0 ]; then
	show "=== TESTS OK === "
	show "The system will reboot soon"
else
	show "=== TESTS FAIL === "
fi

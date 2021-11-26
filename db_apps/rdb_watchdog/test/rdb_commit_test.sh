#!/bin/sh

#
# Unit test for wdt_commit utility.
#
# No parameters required, and it checks
# to make sure its running on a system
# with no active timers before it starts.
#

failTest()
{
    echo "Error: $1" 1>&2
    echo "Test failed." 1>&2
    exit 1
}

failPrecon()
{
    echo "Error: $1" 1>&2
    echo "Test preconditions not met." 1>&2
    exit 1
}

assertSuccess()
{
    ! $@ > /dev/null 2>&1 && failTest "Command '$@' reported failure, expected success."
}

assertFailure()
{
    $@ > /dev/null 2>&1 && failTest "Command '$@' reported success, expected failure."
}

assertListCount()
{
    local count=$(wdt_commit list | wc -l)
    [ $count -ne $1 ] && failTest "Command 'wdt_commit list' reported $count active timer(s), expected $1."
}

assertTimeToReboot()
{
    local timer=$(rdb get sys.watchdog.timeToReboot)
    local next=$(($1-5))
    [ $timer -ne $1 ] && [ $timer -ne $next ] && failTest "Time to reboot is currently $timer second(s), expected $1 (or $next)."
}

assertMaxRebootCount()
{
    local timer=$(rdb get sys.watchdog.max_reboot_count)
    local next=$(($1-5))
    [ $timer -ne $1 ] && [ $timer -ne $next ] && failTest "Max reboot count is currently $timer second(s), expected $1 (or $next)."
}

#
# Preconditions to make sure we're in a good state to start the test.
#

! which rdb > /dev/null && failPrecon "Can't find 'rdb' utility."
! which wdt_commit > /dev/null && failPrecon "Can't find 'wdt_commit' utility."

local timeToReboot=$(rdb get sys.watchdog.timeToReboot)
local queuedTimers=$(rdb get sys.watchdog.queuedTimers)

[ -z "$timeToReboot" ] && failPrecon "Variable 'sys.watchdog.timeToReboot' is not set."
[ "$timeToReboot" -ne -1 ] && failPrecon "Variable 'sys.watchdog.timeToReboot' is already running."
[ ! -z "$queuedTimers" ] && [ "$queuedTimers" != "{}" ] && failPrecon "Variable 'sys.watchdog.queuedTimers' already contains entries."

#
# We should be reporting no active timers.
#
assertListCount 0

#
# Garbage commands should fail.
#
assertFailure "wdt_commit"
assertFailure "wdt_commit foo"
assertFailure "wdt_commit add"
assertFailure "wdt_commit add ''"
assertFailure "wdt_commit add foo"
assertFailure "wdt_commit add foo 0"
assertFailure "wdt_commit add foo -1"
assertFailure "wdt_commit add foo bar"
assertFailure "wdt_commit del"

#
# Add our first timer to the queue.
#
assertSuccess "wdt_commit add medium 60"
assertListCount 1
assertTimeToReboot 60

#
# Duplicate timer names should fail.
#
assertFailure "wdt_commit add medium 30"
assertListCount 1
assertTimeToReboot 60

#
# Shorter timers should be bumped it to the head of the queue.
#
assertSuccess "wdt_commit add short 30"
assertListCount 2
assertTimeToReboot 30

#
# Longer timers should end up at the back.
#
assertSuccess "wdt_commit add long 120"
assertListCount 3
assertTimeToReboot 30

#
# Delete the middle timer, this should not affect the reboot timer.
#
assertSuccess "wdt_commit del medium"
assertListCount 2
assertTimeToReboot 30

#
# Delete the first timer, the third and final timer should now take over.
#
assertSuccess "wdt_commit del short"
assertListCount 1
assertTimeToReboot 120

#
# Delete the final timer, this should disable the reboot timer.
#
assertSuccess "wdt_commit del long"
assertListCount 0
assertTimeToReboot -1

#
# Add a timer with a max reboot count.
#
assertSuccess "wdt_commit add max_rebooter 30 5"
assertListCount 1
assertTimeToReboot 30
assertMaxRebootCount 5

#
# Re-add the same timer with a different max reboot count.
#
assertSuccess "wdt_commit readd max_rebooter 300 4"
assertListCount 1
assertTimeToReboot 300
assertMaxRebootCount 4

#
# Delete the timer, this should disable the reboot timer.
#
assertSuccess "wdt_commit del max_rebooter"
assertListCount 0
assertTimeToReboot -1

echo "Test passed."


#!/bin/sh

#
# Demonstrates how to turn the one_shot_timer utility into a periodic timer
# When the one_shot_timer expires, it invokes this script
# To stop periodic timer, write 0 to .time_in RDB variable
# To change the period from constant to variable, user
# can adjust time_in value at any time (including in this script).
#

test_rdb_prefix="ost_test"
invoke_cmd="one_shot_timer"

# before invoking the script, set the time_in to desired value
# rdb set ost_test.time_in 1000
$invoke_cmd -t $test_rdb_prefix.time_in -x "./periodic.sh" -d -f $test_rdb_prefix.feedback

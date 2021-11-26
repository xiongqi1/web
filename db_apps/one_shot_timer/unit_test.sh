#!/bin/sh

#
# A unit test for one shot timer module
#
# All tests should pass, and also
# should result in 2 "Could not start timer"
# error entries into the system log (due to negative tests)
#

print_usage()
{
    cat << EOF

test.sh

description:
    Implements a unit test for one shot timers

usage:
    Can only run on the targer due to need for RBD
    Make sure one_shot_timer is installed on your target
    Run this script without command line arguments, e.g. unit_test
EOF
}

#
# Compare arg1 with arg2
#
check_ret_value()
{
  if [ "${1}" != "${2}" ]
  then
    echo "FAIL: Incorrect return value in $test_name, expected ${2}, return ${1}"
    unit_test_fail=1
    num_fails=$((num_fails + 1))
  fi
}

#
# arg1 should be greater or equala than arg2 and also, less of equal to
# arg3 or there will be a fail

check_arg_range()
{
  if [ ${1} -lt ${2} ]
  then
    echo "FAIL: Incorrect return value in $test_name, ${1} is less than ${2}"
    unit_test_fail=1
    num_fails=$((num_fails + 1))

  fi

  if [ ${1} -gt ${3} ]
  then
    echo "FAIL: Incorrect return value in $test_name, ${1} is greater than ${3}"
    unit_test_fail=1
    num_fails=$((num_fails + 1))
  fi
}

test_rdb_prefix="ost_test"
invoke_cmd="one_shot_timer"

basic_test()
{
    test_name="Basic test 1"

    echo "Running $test_name"

    #
    echo 'Test #1'
    # Set out var to empty
    # Start our tool with 10 second timeout
    # Check the value in out_var in 9 seconds (should still be empty)
    # Check it again in 2 more seconds
    # Should say "Value" (which is what we passed in Command line)
    #
    rdb set $test_rdb_prefix.time_in "1000"
    rdb set $test_rdb_prefix.out_var "Old"

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -d
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 9

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 2

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #2'
    # Same as above, but interrupt our timer by writing 0 to timer value
    # and then confirm that the old value did not change
    #
    rdb set $test_rdb_prefix.time_in "1000"
    rdb set $test_rdb_prefix.out_var "Old"

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -d
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 9

    # stop the one_shot tool and then sleep with some safe margin
    rdb set $test_rdb_prefix.time_in "0"
    sleep 11

    # should still have the old value
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #3'
    # Same as in test #2, but do not create out_var2 - let the tool create it
    #
    out_var_name="out_var2"
    rdb unset $test_rdb_prefix.$out_var_name
    rdb set $test_rdb_prefix.time_in "500"

    check_ret_value $(rdb get $test_rdb_prefix.$out_var_name) ""
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.$out_var_name -w "New" -d
    check_ret_value $(rdb get $test_rdb_prefix.$out_var_name) ""
    sleep 4
    check_ret_value $(rdb get $test_rdb_prefix.$out_var_name) ""
    sleep 2
    check_ret_value $(rdb get $test_rdb_prefix.$out_var_name) "New"

    #
    echo 'Test #4'
    # Test -a option
    # Start our tool with 10 second timeout
    # Check the value in out_var in 9 seconds (should still be empty)
    # Check it again in 2 more seconds
    # Should say "Value" (which is what we passed in Command line)
    #
    rdb set $test_rdb_prefix.time_in "3000" # will not be used
    rdb set $test_rdb_prefix.out_var "Old"

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    $invoke_cmd -a 1000 -o $test_rdb_prefix.out_var -w "New" -d
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 9

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 2

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

cli_test()
{
    test_name="CLI tests"
    echo "Running $test_name"


    #
    echo 'Test #0 - Make sure everything is ok if CLI is invoked correctly'
    #
    rdb set $test_rdb_prefix.time_in "100"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var
    check_ret_value $? 0

    #
    echo 'Test #1 - Do not provide -t option'
    #
    $invoke_cmd -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    #
    echo 'Test #2 - Do not provide -o option'
    #
    $invoke_cmd -t $test_rdb_prefix.time_in
    check_ret_value $? 1

    #
    echo 'Test #3 - Wrong option'
    #
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -z
    check_ret_value $? 1

    #
    echo 'Test #4 - Timeout value too large'
    #
    rdb set $test_rdb_prefix.time_in "8640001"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    # just to prove that we can start with max timeout, do a bit more here
    rdb set $test_rdb_prefix.time_in "8640000" # the absolute max value
    rdb set $test_rdb_prefix.out_var "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -d
    check_ret_value $? 0
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    rdb set $test_rdb_prefix.time_in "99" # reload to 990 ms
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    # should have expired in 1 second. Platypus utility is not too accurate so
    # give it a bit more time
    sleep 2
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #5 - Timeout value is 0'
    #
    rdb set $test_rdb_prefix.time_in "0" # an invalid value
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    #
    echo 'Test #6 - Timeout value does not exist'
    #
    $invoke_cmd -t "does.not.exist" -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    #
    echo 'Test #7 - Same name used for time and feedback variables'
    #
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -f $test_rdb_prefix.time_in
    check_ret_value $? 1

    #
    echo 'Test #8 - Cannot have both -a and -t'
    #
    $invoke_cmd -t $test_rdb_prefix.time_in -a 1000 -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    #
    echo 'Test #9 - Cannot have both -o and -x'
    #
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -x "sleep 10"
    check_ret_value $? 1

    #
    echo 'Test #10 - Cannot have -a 0'
    #
    $invoke_cmd -a 0 -o $test_rdb_prefix.out_var
    check_ret_value $? 1

    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

mpl_instance_test()
{
    test_name="Multiple instance tests"
    echo "Running $test_name"

    # Set the time RDB variables
    rdb set $test_rdb_prefix.time_in1 "500"     # 5 sec
    rdb set $test_rdb_prefix.time_in2 "1000"    # 10 sec
    rdb set $test_rdb_prefix.time_in3 "1500"    # 15 sec

    # Set the old values to output variables
    rdb set $test_rdb_prefix.out_var1 "Old1"
    rdb set $test_rdb_prefix.out_var2 "Old2"
    rdb set $test_rdb_prefix.out_var3 "Old3"

    #
    # Simultaneously, run 3 instances, one expiring in 5 secons, the second in 10
    # and the third in 15
    #
    $invoke_cmd -t $test_rdb_prefix.time_in1 -o $test_rdb_prefix.out_var1 -d -w "New1"
    $invoke_cmd -t $test_rdb_prefix.time_in2 -o $test_rdb_prefix.out_var2 -d -w "New2"
    $invoke_cmd -t $test_rdb_prefix.time_in3 -o $test_rdb_prefix.out_var3 -d -w "New3"

    # should be all Old
    check_ret_value $(rdb get $test_rdb_prefix.out_var1) "Old1"
    check_ret_value $(rdb get $test_rdb_prefix.out_var2) "Old2"
    check_ret_value $(rdb get $test_rdb_prefix.out_var3) "Old3"

    sleep 6
    # 1 of 3 should be new
    check_ret_value $(rdb get $test_rdb_prefix.out_var1) "New1"
    check_ret_value $(rdb get $test_rdb_prefix.out_var2) "Old2"
    check_ret_value $(rdb get $test_rdb_prefix.out_var3) "Old3"

    sleep 6
    # 2 of 3 should be new
    check_ret_value $(rdb get $test_rdb_prefix.out_var1) "New1"
    check_ret_value $(rdb get $test_rdb_prefix.out_var2) "New2"
    check_ret_value $(rdb get $test_rdb_prefix.out_var3) "Old3"

    sleep 6
    # all 3 should be new
    check_ret_value $(rdb get $test_rdb_prefix.out_var1) "New1"
    check_ret_value $(rdb get $test_rdb_prefix.out_var2) "New2"
    check_ret_value $(rdb get $test_rdb_prefix.out_var3) "New3"

    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

feedback_var_test()
{
    test_name="Feedback variable tests"
    echo "Running $test_name"

    rdb set $test_rdb_prefix.time_in "1000"
    rdb set $test_rdb_prefix.out_var "1"
    rdb_del $test_rdb_prefix.fb_var           # delete if exists

    # use absolute value to demonstrate that fb feature works with it as well
    $invoke_cmd -a 1000 -o $test_rdb_prefix.out_var -d -f $test_rdb_prefix.fb_var -w "1234"

    # sleep 1 second, and allow to decrement to something around 900x10 ms remaining
    sleep 1
    check_arg_range $(rdb get $test_rdb_prefix.fb_var) 850 950 # allow for some latencies

    # sleep another 10 seconds to make sure everything has been terminated
    sleep 10

    # should have exited, check all 3 RDB variables for correct values
    check_arg_range $(rdb get $test_rdb_prefix.fb_var) 0 0
    check_arg_range $(rdb get $test_rdb_prefix.time_in) 1000 1000
    check_arg_range $(rdb get $test_rdb_prefix.out_var) 1234 1234


    #
    echo 'Test #2 - Similar, but kill timer half-way'
    #
    rdb set $test_rdb_prefix.out_var "1"    # 10 sec
    rdb set $test_rdb_prefix.time_in "1000"    # 10 sec
    rdb_del $test_rdb_prefix.fb_var           # delete if exists

    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -d -f $test_rdb_prefix.fb_var -w "1234"

    # sleep 1 second, and allow to decrement to something around 900x10 ms remaining
    sleep 5
    check_arg_range $(rdb get $test_rdb_prefix.fb_var) 450 550 # allow for some latencies

    # kill the timer
    rdb set $test_rdb_prefix.time_in "0"

    # should have exited, check the RDB variables for correct values
    check_arg_range $(rdb get $test_rdb_prefix.fb_var) 0 0
    check_arg_range $(rdb get $test_rdb_prefix.out_var) 1 1

    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

#
# Test that we can use -s option
#
timestamp_test()
{
    test_name="timestamp (-s option) tests"
    echo "Running $test_name"

    #
    echo 'Test #1 - check no trigger if ts not set and no time given'
    #
    rdb set $test_rdb_prefix.out_var "Old"
    rdb unset $test_rdb_prefix.ts
    timeout -t 1 $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts
    check_ret_value $? 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #2 - check timestamp is created and that it can be killed without firing'
    #
    rdb set $test_rdb_prefix.time_in "500"
    rdb set $test_rdb_prefix.out_var "Old"
    rdb unset $test_rdb_prefix.ts

    # has to be & rather than -d so I can get the PID
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts &
    PID=$!
    ts=`date '+%s'`
    sleep 1
    echo expected ts: $(($ts+5)), actual $(rdb get $test_rdb_prefix.ts)
    check_arg_range $(rdb get $test_rdb_prefix.ts) $(($ts+4)) $(($ts+6))
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    kill $PID
    sleep 5 #total 6 of 5, but should be killed
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #3 - check timer can be restarted'
    #
    rdb set $test_rdb_prefix.ts $(($ts+10))
    $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts -d
    sleep 1 #total 7 of 10
    echo expected ts: $(($ts+10)), actual $(rdb get $test_rdb_prefix.ts)
    check_arg_range $(rdb get $test_rdb_prefix.ts) $(($ts+9)) $(($ts+11))
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 5 #total 12 of 10
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #4 - check immediate trigger if ts in past'
    #
    rdb set $test_rdb_prefix.out_var "Old"
    $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts -d
    sleep 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #5 - check no trigger if ts is 0'
    #
    rdb set $test_rdb_prefix.out_var "Old" $test_rdb_prefix.ts 0
    timeout -t 1 $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts
    check_ret_value $? 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #6 - check no trigger if ts is ""'
    #
    rdb set $test_rdb_prefix.out_var "Old" $test_rdb_prefix.ts ''
    timeout -t 1 $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts
    check_ret_value $? 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #7 - check timer can be recreated if var already exists in the past'
    #
    rdb set $test_rdb_prefix.ts $ts
    rdb set $test_rdb_prefix.out_var "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts -d
    ts=`date '+%s'`
    sleep 1
    echo expected ts: $(($ts+5)), actual $(rdb get $test_rdb_prefix.ts)
    check_arg_range $(rdb get $test_rdb_prefix.ts) $(($ts+4)) $(($ts+6))
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"

    #
    echo 'Test #8 - check timer can be extended via ts'
    #
    rdb set $test_rdb_prefix.ts $(($ts+10))
    sleep 7 #8 total of 10
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 3 #11 total of 10
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #9 - check timer can be extended via time var'
    #
    rdb set $test_rdb_prefix.out_var "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts -d
    sleep 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    rdb set $test_rdb_prefix.time_in "900"
    sleep 7 #8 total of 10
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 3 #11 total of 10
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"

    #
    echo 'Test #10 - check timer works with big time differences'
    #
    rdb set $test_rdb_prefix.ts 1
    rdb set $test_rdb_prefix.out_var "Old"
    $invoke_cmd -o $test_rdb_prefix.out_var -w "New" -s $test_rdb_prefix.ts -d
    sleep 1
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"


    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

#
# Test that we can use -x option
#
exec_test()
{
    test_name="execute (-x option) tests"
    echo "Running $test_name"

    #
    # Test #1 - set the rdb variable via executing an external app
    #
    rdb set $test_rdb_prefix.time_in "1000"
    rdb set $test_rdb_prefix.out_var "Old"

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    $invoke_cmd -t $test_rdb_prefix.time_in -x "rdb set $test_rdb_prefix.out_var New" -d
    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 9

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "Old"
    sleep 2

    check_ret_value $(rdb get $test_rdb_prefix.out_var) "New"


    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}

periodic_timer_test()
{
    test_name="periodic timer tests"
    echo "Running $test_name"

    if [ ! -x ./periodic.sh ]; then
        echo "periodic.sh needs to be in the current directory. Cannot run this test."
        check_ret_value "'not found'" "periodic.sh"
        return
    fi

    #
    # periodic.sh will restart the one_shot_timer, so we get a periodic
    # rather than one-shot timer
    #
    rdb set $test_rdb_prefix.time_in "500"
    $invoke_cmd -t $test_rdb_prefix.time_in -x "./periodic.sh" -d -f $test_rdb_prefix.feedback

    sleep 1
    check_arg_range $(rdb get $test_rdb_prefix.feedback) 350 450 # allow for some latencies

    # prove that timer is periodic by watching the feedback variable change
    sleep 5
    check_arg_range $(rdb get $test_rdb_prefix.feedback) 350 450 # allow for some latencies

    rdb set $test_rdb_prefix.time_in "1500"
    sleep 1
    check_arg_range $(rdb get $test_rdb_prefix.feedback) 1350 1450 # allow for some latencies

    # stop timer
    rdb set $test_rdb_prefix.time_in "0"
    check_arg_range $(rdb get $test_rdb_prefix.feedback) 0 0

    if [[ $unit_test_fail == 0 ]]
    then
        echo "$test_name passed"
    fi
}


#
# Script starts executing from here
#

# assume no fail. If any test fails, this is set to 1
unit_test_fail=0

# count number of failed tests
num_fails=0

# pring usage
if [ $# -ge 1 ]
then
    print_usage
    exit 1;
fi

#
# the actual test sequence
# Anything wrong and unit_test_fail will be set to non-zero value
#

basic_test
cli_test
timestamp_test
mpl_instance_test
feedback_var_test
exec_test

#
# @todo: periodic tests on basic one shot timer (Platypus)
# fail.
# Need to somehow detect this and act accordingly
#
periodic_timer_test

if [[ $unit_test_fail == 0 ]]
then
    echo "All tests PASSED..."
    exit 0;
else
    echo "FAIL: $num_fails tests have failed..."
    exit 1;
fi

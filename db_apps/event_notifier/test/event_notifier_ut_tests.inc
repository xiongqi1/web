###############################################################################
#
# event_notifier_ut_tests.inc
#
# Library of event notifier unit tests. Not intended to be run directly but to
# be included in the main unit test shell script. To add a test, create a new
# test function and then add that function name to the UT_TEST_LIST variable.
#
###############################################################################

#
# Tests basic event notification functionality.
# Operation: Generates a single notification for each event type.
# Expected Result: A notification log entry for each event type is found.
#
enoti_basic_test()
{
    local basic_test_prefix="basic_test"
    local basic_test_event_text=

    # Generate a notification for each event type
    for basic_test_event_type in ${EVENT_TYPES}
    do
        basic_test_event_text="${basic_test_prefix}${basic_test_event_type}"
        ut_elogger ${basic_test_event_type} ${basic_test_event_text}
    done

    # Verify that there is a log entry for each notification event type
    for basic_test_event_type in  ${EVENT_TYPES}
    do
        basic_test_event_text="${basic_test_prefix}${basic_test_event_type}"

        ut_check_log_present ${basic_test_event_type} ${basic_test_event_text} 1
        if [ $? -ne 0 ]
        then
            # Failed
            return 1
        fi
    done

    return 0
}

#
# Tests wrapping of event notification buffers.
# Operation: Generates enough events to cause the notification buffers to wrap
# Expected Result: All notification events should still have a log entry.
#
enoti_wrap_test()
{
    # Get configured num event buffers and add a few more to that number
    # to garauntee a buffer wrap.
    local wrap_test_conf_max_size=`rdb_get service.eventnoti.conf.max_size`
    local wrap_test_num_overflow=10
    local wrap_test_num_noti=$((${wrap_test_conf_max_size} + ${wrap_test_num_overflow}))

     # Generate repeated notifications
    local wrap_test_event_type=1
    local wrap_test_prefix="wrap_test"
    local wrap_test_event_text=
    for ix in $(seq 1 ${wrap_test_num_noti})
    do
        wrap_test_event_text="${wrap_test_prefix}${ix}"
        ut_elogger ${wrap_test_event_type} ${wrap_test_event_text}
    done

    # Verify that there is a log entry for each notification
    for ix in $(seq 1 ${wrap_test_num_noti})
    do
        wrap_test_event_text="${wrap_test_prefix}${ix}"
        ut_check_log_present ${wrap_test_event_type} ${wrap_test_event_text} 1
        if [ $? -ne 0 ]
        then
            # Failed
            return 1
        fi
    done

    return 0
}

#
# Tests generating event notifications from concurrent processes.
# Operation: Generate events from multiple processes.
# Expected Result: All notifications from each process should be logged.
#
enoti_concurrent_test()
{
    local concur_test_prefix="concurrent_test"
    local concur_test_num_events=10

    # Create two background processes to post and check events.
    # A different event type is used to easily distinguish events in the log.
    (ut_post_and_check_events 1 ${concur_test_num_events})&
    local pid1=$!
    (ut_post_and_check_events 3 ${concur_test_num_events})&
    local pid2=$!

    # Wait for each background process to complete and check their return values.
    wait $pid1
    local res1=$?
    wait $pid2
    local res2=$?
    if [ $res1 -eq 0 -a $res2 -eq 0 ]
    then
        # Passed
        return 0
    else
        # Failed
        return 1
    fi
}

#
# Negative scenario testing.
# Operation: Generates negative events
# Expected Result: Negative events should not be logged.
#
enoti_negative_test()
{
    local ix=0
    local neg_test_prefix="negative_test"

    # Test: Post an invalid (outside type range) event type notification.
    local neg_test_event_type=$((NUM_EVENT_TYPES + 1))
    local neg_test_event_text="${neg_test_prefix}${ix}"
    ut_elogger ${neg_test_event_type} ${neg_test_event_text}
    ut_check_log_not_present ${neg_test_event_type} ${neg_test_event_text}
    if [ $? -ne 0 ]
    then
        # Failed
        return 1
    fi

    # Test: Post an invalid (non-numeric) event type notification.
    ix=$((ix + 1))
    neg_test_event_type="abc"
    neg_test_event_text="${neg_test_prefix}${ix}"
    ut_elogger ${neg_test_event_type} ${neg_test_event_text}
    ut_check_log_not_present ${neg_test_event_type} ${neg_test_event_text}
    if [ $? -ne 0 ]
    then
        # Failed
        return 1
    fi

    # Test: Post a valid event to verify that event notifications are not broken
    #       after the negative cases have run.
    ix=$((ix + 1))
    neg_test_event_type=1
    neg_test_event_text="${neg_test_prefix}${ix}"
    ut_elogger ${neg_test_event_type} ${neg_test_event_text}
    ut_check_log_present ${neg_test_event_type} ${neg_test_event_text} 1
    if [ $? -ne 0 ]
    then
        # Failed
        return 1
    fi

    return 0
}

#
# Test the Event Notifier retry mechanism.
# Operation: Generate event notifications where all or some of the notifications
# fail in enotifier
# Expected Result: The failed notifications should be retried the max number of
# times and the successful notifications should not be retried.
#
enoti_retry_test()
{
    # Install a test specific enotifier
    cp /usr/bin/enotifier /usr/bin/enotifier.original
    cp ./enotifier_ut /usr/bin/enotifier

    local event_type=3
    local event_text=
    local num_events=3
    local retry_cnt=`rdb_get service.eventnoti.conf.max_retry`
    local max_poll_int=10
    local enotifier_result=
    local expected_count=

    # Test: Post a number of events where all the notifications fail
    event_text=retry_all_fail_test
    enotifier_result=0
    for ix in $(seq 1 ${num_events}); do
        ut_elogger ${event_type} ${enotifier_result}${event_text}${ix}
    done

    # Wait some time for any retries to be logged
    sleep $((${retry_cnt} * ${max_poll_int}))

    # Check the result. All events should have the max retry number of notifications
    # logged.
    enotifier_result=0
    for ix in $(seq 1 ${num_events}); do
        expected_count=5

        ut_check_log_present ${event_type} ${enotifier_result}${event_text}${ix} ${expected_count}

        if [ $? -ne 0 ]; then
            return 1
        fi
    done

    # Test: Post a number of events where the first event fails in enotifier
    event_text=retry_first_fail_test
    enotifier_result=0
    for ix in $(seq 1 ${num_events}); do
        ut_elogger ${event_type} ${enotifier_result}${event_text}${ix}
        enotifier_result=1
    done

    # Wait some time for any retries to be logged
    sleep $((${retry_cnt} * ${max_poll_int}))

    # Check the result. All events should have exactly one notification except
    # for the first one which should have the max number of retries logged. 
    enotifier_result=0
    for ix in $(seq 1 ${num_events}); do
        local expected_count=1
        if [ ${enotifier_result} -eq 0 ]; then
            expected_count=${retry_cnt}
        fi

        ut_check_log_present ${event_type} ${enotifier_result}${event_text}${ix} ${expected_count}

        if [ $? -ne 0 ]; then
            return 1
        fi

        enotifier_result=1
    done

    # Clean up
    mv /usr/bin/enotifier.original /usr/bin/enotifier

    return 0
}

# List of tests that will be run.
UT_TEST_LIST="enoti_basic_test enoti_wrap_test enoti_concurrent_test enoti_negative_test enoti_retry_test"

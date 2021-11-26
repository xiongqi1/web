###############################################################################
#
# event_notifier_ut_utils.inc
#
# Library of event notifier unit tests utilities. Not intended to be run directly
# but to be included in the main unit test shell script.
#
##############################################################################

# All event notification logs are prefixed with this string which contains
# the UT shell script process id. This makes the logs unique across different
# test runs for verification purposes.
UT_LOG_PREFIX="ENOTI_UT$$"

# ut_init will probe for the valid event notification types
EVENT_TYPES=
NUM_EVENT_TYPES=0
NO_DEST_EVENTS=

#
# Print a debug message if debugging enabled.
# Args:
#    $1 = Message to be printed
#
UT_DBG_ENABLE=0
ut_dbg()
{
    if [ ${UT_DBG_ENABLE} -eq 1 ]
    then
        echo "${CURRENT_TEST}: $1"
    fi
}

#
# Unconditionally print a message.
# Args:
#    $1 = Message to be printed
#
ut_print()
{
    echo "${CURRENT_TEST}: $1"
}

#
# Adds a prefix to the event text before sending it off to elogger.
# Args:
#    $1 = Event notification type
#    $2 = Event notification text
#
ut_elogger()
{
    local ut_elogger_event_type=$1
    local ut_elogger_event_text="<$UT_LOG_PREFIX:$2>"

    elogger $ut_elogger_event_type $ut_elogger_event_text
}

# After elogger is invoked it will take some time before the event
# notification makes its way to the end notify client. So need some sleep
# time to wait for that to occur before checking for the notification result.
SLEEP_SECS=1
CHK_MAX_RETRIES=30

#
# Checks syslog for the presence of event notifications with the given type and text.
# Args:
#    $1 = Event notification type
#    $2 = Event notification text
#    $3 = Expected number of notifications
#
ut_check_log_present()
{
    local ut_check_log_event_type=$1
    local ut_check_log_event_text="<$UT_LOG_PREFIX:$2>"
    local ut_check_log_expected_count=$3

    # Periodically look in syslog until the expected number of matched notifications
    # are found or the max retries has occurred.
    local retries=0
    while [ ${retries} -lt ${CHK_MAX_RETRIES} ]
    do
        grep_count=`logread | grep -c "\[EVENT\#${ut_check_log_event_type}\].*${ut_check_log_event_text}"`

        if [ ${grep_count} -eq ${ut_check_log_expected_count} ]
        then
            # Passed
            return 0
        fi

        sleep ${SLEEP_SECS}
        retries=$(($retries + 1))
    done

    # Failed
    ut_print "Check log failed. Event <${ut_check_log_event_type}>, Text <$ut_check_log_event_text>. Expected ${ut_check_log_expected_count} occurrences, got ${grep_count}."
    return 1
}

#
# Checks syslog for the absence of event notifications with the given text.
# Note that the event type is actually not used during log matching. This is to
# catch the cases where an event is posted with an incorrect event type.
# Args:
#    $1 = Event notification type
#    $2 = Event notification text
#
ut_check_log_not_present()
{
    local ut_check_log_not_present_event_type=$1
    local ut_check_log_not_present_event_text="<$UT_LOG_PREFIX:$2>"

    # Wait some time for the notificaiton to appear and then check.
    sleep $((${SLEEP_SECS} * ${CHK_MAX_RETRIES}))
    local grep_count=`logread | grep -c "\[EVENT.*${ut_check_log_not_present_event_text}"`

    if [ ${grep_count} -eq 0 ]
    then
        # Passed
        return 0
    else
         # Failed
        ut_print "Check log failed. Event <${ut_check_log_not_present_event_type}>, Text <$ut_check_log_not_present_event_text>. Expected 0 occurrences, got ${grep_count}."
        return 1
    fi
}

#
# Posts a given number of events of a given event type and then checks that
# are present in the syslog.
# Args:
#    $1 = Event notification type
#    $2 = Number of events to post
#
ut_post_and_check_events()
{
    local ut_pac_event_type=$1
    local ut_pac_num_events=$2
    local ut_pac_event_text="ut_pac"

    for ix in $(seq 1 ${ut_pac_num_events})
    do
        ut_elogger ${ut_pac_event_type} ${ut_pac_event_text}
    done

    ut_check_log_present ${ut_pac_event_type} ${ut_pac_event_text} ${ut_pac_num_events}
}

#
# Unit test initialisation. MUST be invoked before running any test.
# Adds the "log" client to each event type. Saves the original event client lists
# so that they can be restored at the end of the test.
#
ut_init()
{
    NUM_EVENT_TYPES=0
    local clients=
    while true
    do
        clients=`rdb_get service.eventnoti.conf.type.$((${NUM_EVENT_TYPES} + 1)).enabled_clients`
        if [ $? -ne 0 ]
        then
            # No more event types.
            break
        fi

        NUM_EVENT_TYPES=$((${NUM_EVENT_TYPES} + 1))

        eval CLIENTS_ORIGINAL${NUM_EVENT_TYPES}=\$clients

        # Only add log client if not already present
        if ! echo "${clients}" | egrep -q "log" ;
        then
            rdb_set service.eventnoti.conf.type.${NUM_EVENT_TYPES}.enabled_clients "${clients}log;"
        fi

        # Destination of hidden events will be blank after configuring on Web UI
        # that makes eloggper ignore those events.
        # In order to include those events in unit test, temporarily set destination 1.
        dest_idx=`rdb_get service.eventnoti.conf.type.${NUM_EVENT_TYPES}.dest_idx`
        if [ -z "$dest_idx" ]; then
            rdb_set service.eventnoti.conf.type.${NUM_EVENT_TYPES}.dest_idx 1
            NO_DEST_EVENTS="${NO_DEST_EVENTS} ${NUM_EVENT_TYPES}"
	fi
    done

    # Skip event 2 which is the reboot event. The reboot event has 
    # special handling which assumes that there are no further events
    # after it and hence can not be generated as a "fake" event during
    # unit testing.
    EVENT_TYPES="1 $(seq 3 ${NUM_EVENT_TYPES})"

    if [ -z "${UT_RUN_TEST_LIST}" ]; then
        # User has not specified a test list. Run the full list of tests.
        UT_RUN_TEST_LIST=${UT_TEST_LIST}
    fi
}

#
# Unit test clean up. MUST be invoked after running all tests.
# Restores the target to original state.
#
ut_fini()
{
    # Restore original client list
    local clients=
    for ut_fini_i in ${EVENT_TYPES}
    do
        eval clients=\${CLIENTS_ORIGINAL${ut_fini_i}}
        rdb_set service.eventnoti.conf.type.${ut_fini_i}.enabled_clients "${clients}"
    done

    # Restore destination of hidden events
    for ut_no_dest_event_type in ${NO_DEST_EVENTS}
    do
        rdb_set service.eventnoti.conf.type.${ut_no_dest_event_type}.dest_idx ""
    done

    # Restore original enotifier
    if [ -f /usr/bin/enotifier.original ]; then
        mv /usr/bin/enotifier.original /usr/bin/enotifier
    fi

    exit 0
}

CURRENT_TEST="<none>"
NUM_TESTS_RUN=0
NUM_TESTS_FAILED=0

#
# Runs each registered test function and collates the results.
#
ut_run()
{
    echo "#############################"
    echo "Test List: $UT_RUN_TEST_LIST"

    # Run through each requested test
    for CURRENT_TEST in ${UT_RUN_TEST_LIST}
    do
        ut_dbg "Start"

        ${CURRENT_TEST}

        # Check and log the result of the current test
        if [ $? -eq 0 ]
        then
            ut_print "PASSED"
        else
            NUM_TESTS_FAILED=$((${NUM_TESTS_FAILED} + 1))
            ut_print "FAILED"
        fi

        NUM_TESTS_RUN=$((${NUM_TESTS_RUN} + 1))

        ut_dbg "End"
    done

    echo "#############################"

    # Summarise run results
    echo "Ran ${NUM_TESTS_RUN} tests."
    if [ ${NUM_TESTS_FAILED} -eq 0 ]
    then
        echo "Congratulations, all tests passed!"
    else
        echo "${NUM_TESTS_FAILED} tests failed."
    fi
}

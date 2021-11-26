#!/bin/sh

###############################################################################
#
# event_notifier_ut.sh
#
# Event Notifier unit tests top level script.
# The tests write to and read from syslog. To ensure that the tests run
# properly it is advised to run under conditions where syslog load is low.
#
##############################################################################

. ./event_notifier_ut_utils.inc
. ./event_notifier_ut_tests.inc

usage()
{
    cat << EOF
Usage:
${0##*/} [options]

Options:
 -d                Enable unit test script debug prints
 -l                Lists all available unit test names
 -h                Print this usage message
 -t <test list>    Run the tests named in <test list>
EOF
}

parse_cmd_line()
{
    while getopts "dlht:" opt; do
        case $opt in
            h)
                usage
                exit 0
                ;;
            d)
                UT_DBG_ENABLE=1
                ;;
            l)
                echo "Available tests: ${UT_TEST_LIST}"
                exit 0
                ;;
            t)
                UT_RUN_TEST_LIST="${OPTARG}"

                if ! is_sublist "${UT_RUN_TEST_LIST}" "${UT_TEST_LIST}"; then
                    echo "Invalid test list. Valid values are: ${UT_TEST_LIST}"
                    exit 1
                fi
                ;;
            \?)
                usage
                exit 1
                ;;
        esac
    done
}

#
# Determines whether one list is a sublist of another. Note: if the two lists
# contain the same entries this is still considered to be a sublist match.
# Args:
#    $1 = The potention sub-list
#    $2 = The full list
# Returns: 0 if $1 is a sublist of $2. 1 otherwise.
#
is_sublist()
{
    local list1=$1
    local list2=$2

    for e1 in ${list1}; do
        local found=0
        for e2 in ${list2}; do
            if [ "${e1}" == "${e2}" ]; then
                found=1
                break
            fi
        done

        if [ ${found} -eq 0 ]; then
            return 1
        fi
    done

    return 0
}

trap 'ut_fini' INT TERM EXIT HUP QUIT PIPE

parse_cmd_line "$@"
ut_init
ut_run

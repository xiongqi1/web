#!/bin/sh
#
# Utility to test recovery of all UBI PEBs of a UBI device. This test is
# intended to be a "no harms" test - the UBI FS should not exhibit any error
# conditions even after recovering every single PEB.
#
# The test sequence is:
#    1. Initiate recovery of all PEBs via the UBI sysfs recover_peb facility.
#       Subsequent steps fill the UBI device with files so this current step
#       is to ensure that recovery of unallocated blocks is exercised.
#    2. Fill the UBI device with files.
#    3. Record the pre-recovery MD5 sum of all the generated files.
#    4. Initiate recovery of all PEBs via the UBI sysfs recover_peb facility.
#    5. Record the post-recovery MD5 sum of all the generated files.
#
# Test passes if the pre and post MD5 sums match.
#
# Copyright (C) 2017 NetComm Wireless limited.
#

# UBI device number. Set by command line
UBI_DEV_NUM=

# Test file details
FILE_NAME_PREFIX="random_file.$$."
DEFAULT_FILE_SIZE_KB=10240
FILE_SIZE_KB=${DEFAULT_FILE_SIZE_KB}

# Md5 files for validation
PRE_MD5SUM_FILE="/tmp/pre.$$"
POST_MD5SUM_FILE="/tmp/post.$$"

#
# Prints out a usage message.
#
usage()
{
    cat << EOF
Usage:
${0##*/} [options] ubi_dev_num

Options:
 -s        Size in KB of the test file to use. If not specified, default
           is ${DEFAULT_FILE_SIZE_KB}
 -h        Print this usage message
EOF
}

#
# Parse the command line arguments
# Args:
#    $1 = Command line arguments
#
parse_cmd_line()
{
    while getopts "hs:" opt; do
        case $opt in
            h)
                usage
                exit 0
                ;;
            s)
                FILE_SIZE_KB=${OPTARG}
                ;;
            \?)
                usage
                exit 1
                ;;
        esac
    done

    shift $((OPTIND-1))

    UBI_DEV_NUM=$1
    if [ -z "${UBI_DEV_NUM}" ]; then
        echo "Missing mandatory UBI device number"
        usage
        exit 1
    fi
}

#
# Print the mount points of a UBI device.
#
# Args:
#     $1 = UBI device number
#
get_ubi_mounts()
{
    dev=$1

    # Example mount output that is being parsed:
    #    proc on /proc type proc (rw,relatime)
    #    ubi0:local on /usr/local type ubifs (rw,relatime)
    #    ubi1:opt on /opt type ubifs (rw,relatime)
    #    ubi1:config on /usr/local/cdcs/conf type ubifs (rw,relatime)
    mount | grep "^ubi${dev}" | awk '{ print $3 }'
}

#
# Creates files in all mounted partitions of a UBI device.
#
# Args:
#     $1 = UBI device number
#
fill_ubi_dev()
{
    dev=$1
    mount_points=$(get_ubi_mounts ${dev})
    for mount in ${mount_points}; do
        echo "Filling directory ${mount}"
        local ix=0
        while head -c ${FILE_SIZE_KB}k < /dev/urandom > "${mount}/${FILE_NAME_PREFIX}${ix}" 2> /dev/null
        do
            ix=$((ix+1))
        done
    done
}

#
# Initiates recovery of all PEBs in a UBI device via the UBI sysfs
# recover_peb facility
#
# Args:
#     $1 = UBI device number
#
recover_all_pebs()
{
    dev=$1
    ubi_dev_sysfs=/sys/class/ubi/ubi${dev}
    num_pebs=$(cat ${ubi_dev_sysfs}/total_eraseblocks)
    echo "Recovering ${num_pebs} pebs in ubi${dev}"
    for i in $(seq 0 $((num_pebs-1))); do
        echo $i > ${ubi_dev_sysfs}/recover_peb
    done

    # Give the kernel some time to complete the recovery
    sleep 30
}

#
# Generates a file containing MD5 sums of all test generated files on a given
# UBI device.
#
# Args:
#     $1 = UBI device number
#     $2 = File name for the MD5 output
#
md5sum_gen() {
    dev=$1
    md5sum_file=$2
    echo "Generating MD5 sum file ${md5sum_file}"
    mount_points=$(get_ubi_mounts ${dev})
    rm -f ${md5sum_file}
    for mount in ${mount_points}; do
        md5sum ${mount}/${FILE_NAME_PREFIX}* >> ${md5sum_file}
    done
}

#
# Test clean up.
#
# Args:
#     $1 = UBI device number
#
cleanup()
{
    echo "Cleaning up"
    dev=$1
    mount_points=$(get_ubi_mounts ${dev})
    for mount in ${mount_points}; do
        rm -f ${mount}/${FILE_NAME_PREFIX}*
    done
    rm -f ${PRE_MD5SUM_FILE} ${POST_MD5SUM_FILE}

    trap - INT TERM EXIT HUP QUIT PIPE
}

#### Main ####

trap 'cleanup ${UBI_DEV_NUM}' INT TERM EXIT HUP QUIT PIPE
parse_cmd_line "$@"

# Run the test steps
recover_all_pebs ${UBI_DEV_NUM}
fill_ubi_dev ${UBI_DEV_NUM}
md5sum_gen ${UBI_DEV_NUM} ${PRE_MD5SUM_FILE}
recover_all_pebs ${UBI_DEV_NUM}
md5sum_gen ${UBI_DEV_NUM} ${POST_MD5SUM_FILE}

# Validate the result
diff ${PRE_MD5SUM_FILE} ${POST_MD5SUM_FILE}
if [ $? -eq 0 ]; then
    echo "Success!"
else
    echo "Fail!"
fi

exit ${ret}

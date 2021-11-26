#!/bin/sh
#
# Utility to test recovery of a single UBI PEB. It initiates recovery of a PEB
# via the UBI sysfs recover_peb facility and performs the following validation:
#
#   - If the PEB is unallocated: The recovery should just erase the PEB. So
#     test passes if the PEB's erase count is incremented.
#   - If the PEB is allocated: The recovery should scrub the PEB by finding
#     a new PEB to relocate the LEB to and then erases the original PEB. So
#     test passes if the original PEB becomes unallocated, the original PEB's
#     erase count is incremented and the new PEB is associated with the original
#     LEB.
#
# Copyright (C) 2017 NetComm Wireless limited.
#

UBI_DEV_NUM=
UBI_BLK_NUM=
UBI_DEV_SYSFS=

#
# Prints out a usage message.
#
usage()
{
    cat << EOF
Usage:
${0##*/} [options] ubi_dev_num

Options:
 -B                UBI block number to recover. If not provided then a random
                   UBI block will be recovered.
 -h                Print this usage message
EOF
}

#
# Parse the command line arguments
# Args:
#    $1 = Command line arguments
#
parse_cmd_line()
{
    while getopts "B:h" opt; do
        case $opt in
            h)
                usage
                exit 0
                ;;
            B)
                UBI_BLK_NUM=${OPTARG}
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
    UBI_DEV_SYSFS="/sys/class/ubi/ubi${UBI_DEV_NUM}"

    if [ -z "${UBI_BLK_NUM}" ]; then
        # No ubi block given. Choose a random one.
        num_erase_blocks=$(cat /sys/class/ubi/ubi${UBI_DEV_NUM}/total_eraseblocks)
        UBI_BLK_NUM=$(awk -v num_blocks=${num_erase_blocks} 'BEGIN { srand(); printf("%d\n",rand() * num_blocks) }')
    fi
}

#
# Runs the ubicheck utility to get info for a given UBI block.
#
# Args:
#     $1 = UBI device number
#     $2 = UBI block number
#
run_ubicheck()
{
    ubi_num=$1
    ubi_blk=$2
    mtd_num=$(cat ${UBI_DEV_SYSFS}/mtd_num)
    mtd_dev="/dev/mtd${mtd_num}"

    ubicheck -B ${ubi_blk} -d ${mtd_dev}
}

#
# Checks whether a given UBI block is currently unallocated.
#
# Args:
#     $1 = UBI device number
#     $2 = UBI block number
#
# Return: zero if block is free and non-zero otherwise.
#
is_free_check()
{
    run_ubicheck $@ | grep UNALLOC > /dev/null
    return $?
}

#
# Prints the erase count for a given UBI block.
#
# Args:
#     $1 = UBI device number
#     $2 = UBI block number
#
get_erasecount()
{
    # The last line of ubicheck is of the form:
    #    =- PEB   53, EC   38, VID UNALLOC
    # Get the EC value and remove the trailing comma.
    run_ubicheck $@ | tail -1 | awk '{ sub(/,/, "", $5);print $5 }'
}

#
# Prints the LEB for a given UBI block. This function should only
# be called for allocated PEBs.
#
# Args:
#     $1 = UBI device number
#     $2 = UBI block number
#
get_leb()
{
    # The last line of ubicheck is of the form:
    #    == PEB    0, EC   44, VID0 LEB1
    # Get the LEB value and remove the "LEB" prefix to leave just the number.
    run_ubicheck $@ | tail -1 | awk '{ sub(/LEB/, "", $7);print $7 }'
}

#
# Prints the PEB that a LEB has been moved to.
#
# Args:
#     $1 = PEB that has been moved
#
get_moved_to_peb()
{
    old_peb=$1

    # dmesg format:
    #    UBI-1: wear_leveling_worker:scrubbed PEB 0 (LEB 0:6), data moved to PEB 284
    # Get the last "moved to" message for the given PEB and print the moved to
    # PEB number.
    dmesg | grep "PEB ${old_peb}" | grep "data moved to" | tail -1 | awk '{ print $NF }'
}

#### Main ####

parse_cmd_line "$@"

# Record the erasecount, free/alloc status and LEB (if an alloced block) of the
# PEB before recovering the PEB
erasecount_pre=$(get_erasecount ${UBI_DEV_NUM} ${UBI_BLK_NUM})
is_free_check ${UBI_DEV_NUM} ${UBI_BLK_NUM}
is_free=$?
if [ ${is_free} -ne 0 ]; then
    leb_pre=$(get_leb ${UBI_DEV_NUM} ${UBI_BLK_NUM})
    status_str="INUSE"
else
    status_str="FREE"
fi

echo "Testing recovery of ${status_str} block ubi${UBI_DEV_NUM}:PEB${UBI_BLK_NUM}"
echo ${UBI_BLK_NUM} > ${UBI_DEV_SYSFS}/recover_peb

# Give the ubi scrubber some time to do its work
sleep 2

# Validate: PEB is always erased upon recovery so erase count should be
# incremented.
erasecount_post=$(get_erasecount ${UBI_DEV_NUM} ${UBI_BLK_NUM})
if [ $((${erasecount_pre}+1)) -eq ${erasecount_post} ]; then
    echo "Pass: erasecount was incremented"
else
    echo "Fail: expected post erasecount $((${erasecount_pre}+1)), got ${erasecount_post}"
    return 1
fi

# Validate: Allocated PEBs are scrubbed (LEB is moved and original PEB erased).
if [ ${is_free} -ne 0 ]; then
    # Validate: LEB should be moved
    new_peb=$(get_moved_to_peb ${UBI_BLK_NUM})
    if [ -z "${new_peb}" ]; then
        echo "Fail: In use PEB was not scrubbed"
        return 1
    fi

    # Validate: Original PEB should now be free
    if ! is_free_check ${UBI_DEV_NUM} ${UBI_BLK_NUM}; then
         echo "Fail: In use PEB not free after scrubbing"
         return 1
    fi

    # Validate: LEB associated with original PEB should now be associated
    # with the moved to PEB.
    leb_post=$(get_leb ${UBI_DEV_NUM} ${new_peb})
    if [ "${leb_pre}" = "${leb_post}" ]; then
        echo "Pass: LEB${leb_pre} was moved from PEB${UBI_BLK_NUM} to PEB${new_peb}"
    else
        echo "Fail: LEB mismatch. Expected ${leb_pre}, got ${leb_post}"
        return 1
    fi
fi

# Test passed!
return 0

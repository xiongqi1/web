#!/bin/sh
#
# bt_pair.sh:
#    Facilitates pairing of a single bluetooth device.
#
# Copyright Notice:
# Copyright (C) 2014 NetComm Pty. Ltd.
#
# This file or portions thereof may not be copied or distributed in any form
# (including but not limited to printed or electronic forms and binary or
#  object forms)
# without the expressed written consent of NetComm Wireless Pty. Ltd
# Copyright laws and International Treaties protect the contents of this file.
# Unauthorized use is prohibited.
#
#
# THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
# SUCH DAMAGE.
#
#

usage()
{
    cat << EOF
Usage:
${0##*/} <device address> <device type> <serial_num> [device name]

    <device type>: 0 = Unknown
                   1 = TD2251 Weight Scale
                   2 = Nonin 3230 Pulse Oximeter
EOF
}

if [ $# -lt 2 ]; then
    usage
    exit 0
fi

dev_addr=$1
dev_type=$2
dev_sn=$3
dev_name=$4

bt_prefix="bluetooth"
bt_dev_prefix="${bt_prefix}.device"

# Get the currently paired device and check whether current device is
# already on the list.
devices=`rdb dump ${bt_dev_prefix}`
this_dev=`echo "${devices}" | grep -E "${bt_dev_prefix}.[0-9]*.addr" | grep "${dev_addr}"`
if [ ! -z "${this_dev}" ]; then
    echo "Device already paired"
    exit 1
fi

if [ -z "${dev_name}" ]; then
    # Query the remote dev to get its name - only works for BT classic devices
    dev_name=`hcitool name ${dev_addr}`

    if [ -z "${dev_name}" ]; then
        echo "Remote device not available"
        exit 1
    fi
fi

# Get the maximum bt device index currently in use:
# Dump the bt devices | get just the name entries | get just the rdb name
# column | get the device index | sort numberically | get the last/max value.
#    - This could be done simpler if we assume that the device indexes
#    - are always contiguous (without any holes).
max_dev_index=`echo "${devices}" | grep -E "${bt_dev_prefix}.[0-9]*.name" | cut -f 1 | cut -d . -f 3 | sort -g | tail -1`

if [ -z "${max_dev_index}" ]; then
    next_dev_index=0
else
    next_dev_index=$(( max_dev_index + 1 ))
fi

# Create the require RDB bluetooth device entries.
dev_path="${bt_dev_prefix}.${next_dev_index}"
rdb set "${dev_path}.addr" "${dev_addr}"
rdb set "${dev_path}.name" "${dev_name}"
rdb set "${dev_path}.serial_num" "${dev_sn}"
rdb set "${dev_path}.type" "${dev_type}"
rdb set "${dev_path}.data.start_idx" "0"
rdb set "${dev_path}.data.count" "0"
rdb set "${dev_path}.data.max_count" "10"
rdb setflags "${dev_path}.addr" p
rdb setflags "${dev_path}.name" p
rdb setflags "${dev_path}.serial_num" p
rdb setflags "${dev_path}.type" p
rdb setflags "${dev_path}.data.start_idx" p
rdb setflags "${dev_path}.data.count" p
rdb setflags "${dev_path}.data.max_count" p

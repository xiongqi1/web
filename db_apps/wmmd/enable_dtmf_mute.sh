#!/bin/sh

# Copyright (C) 2020 NetComm Wireless limited.

#
# This script activates Qualcomm DTMF mute
#

nof=${0##*/}
source /lib/utils.sh

logNotice "enable Tx DTMF mute"
amix 'DTMF_Detect Tx mute voice enable' 1 40000 2> /dev/null > /dev/null

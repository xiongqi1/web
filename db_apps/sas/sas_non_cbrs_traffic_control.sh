#!/bin/sh
#
# Control non-CBRS traffic through CBRS link profile
#
# Copyright (C) 2019 Casa Systems Inc
#
# Parameters:
#     $1: command (block: block traffic (default); unblock: unblock traffic)
# Notice: need to call with flock:
#     flock /tmp/sas_non_cbrs_traffic_control /usr/bin/sas_non_cbrs_traffic_control.sh
#

nof=${0##*/}                        # Name of file/script.
source /lib/utils.sh

CMD="$1"
test -z "${CMD}" && CMD="block"

# find the profile number of CBRS SAS APN
SAS_PDP_PROFILE=$(rdb_get sas.config.pdp_profile)
PFNO=$(getPrfNoOfPdpFunction CBRSSAS $SAS_PDP_PROFILE)
IF_NAME=$(rdb_get link.profile.${PFNO}.interface)

if [ -z "${IF_NAME}" ]; then
  logErr "CBRS APN interface not found in link.profile.${PFNO}.interface, nothing to do"
  exit
fi

restricted_ifs=$(rdb_get admin.restricted_ifs)

if [ "${CMD}" = "block" ]; then
  if [ -z "$(echo $restricted_ifs|grep $IF_NAME)" ]; then
    # trigger sas template to rebuild iptables & route
    rdb_set sas.transmit_enabled "$(rdb_get sas.transmit_enabled)"
  fi
else
  if [ -n "$(echo $restricted_ifs|grep $IF_NAME)" ]; then
    rdb_set admin.restricted_ifs ""
  fi
fi


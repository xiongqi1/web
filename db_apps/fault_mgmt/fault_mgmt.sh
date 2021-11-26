#!/bin/sh /etc/rc.common

# Copyright (C) 2018 NetComm Wireless limited.
# Initialization script for FaultMgmt module

FAULT_MGMT_RDB_ROOT=tr069.FaultMgmt

start() {
    # Making sure that LUA_PATH is set properly for rdb command
    export LUA_PATH="/usr/share/lua/5.1/?.lua;;"
    export LUA_CPATH="/usr/lib/lua/5.1/?.so;;"

    # FaultMgmt data is currently designed to be non-persist so we can remove any
    # data from RDB in case there is old persist data still left in the system
    rdb unset $(rdb list ${FAULT_MGMT_RDB_ROOT} | grep -e "^${FAULT_MGMT_RDB_ROOT}")

    # Load FaultMgmt Supported Alarms
    echo "FaultMgmt: Booting..."
    /usr/bin/fmctl boot
}


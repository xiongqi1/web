#!/bin/sh
#
# Data Stream Bluetooth Endpoint launcher
# To be called by data_stream_mgr.template
#
# The behaviour of this script depends on whether a dsm_bluetooth_ep rpc server
# has already been running.
# If yes, it initiates an rpc call to the server and then quits.
# Otherwise, it starts the server and remains running (not returning unless
# killed or errors occurs).
#
# The PidFile records the pid of this script instead of the server subprocess.
# In order to gracefully kill this script, we should do something like:
#   kill -TERM/HUP/INT/QUIT $pid
# On receiving this signal, the script will send SIGTERM to the server
# subprocess for proper cleanup as follows:
#   pkill -TERM -P $pid
#
# We need a proper service.dsm.stream.?.kill_signo entry so that
# data_stream_mgr.template sends correct signal when it kills the script.
#
# Copyright (C) 2016 NetComm Wireless limited.
#

nof=${0##*/}
PidFile="/var/run/$nof.pid"
DSM_BTEP_SERVER=dsm_bluetooth_ep

MAX_RETRIES=3
RETRY_WAIT=0.3
RPC_SVC="dsm.btep.rpc"
RPC_TIMEOUT=2
RPC_RESULT_LEN=128

source /lib/utils.sh
source /etc/variant.sh

if [ -f $PidFile ]; then
    pid=$(cat $PidFile 2>/dev/null)
    if kill -0 $pid >/dev/null 2>&1 ; then
        logDebug "$nof is already running: $pid"
        # invoke rdb rpc client call, which should return shortly
        # retry in case rpc server is not ready
        retry=0
        until [ $retry -ge $MAX_RETRIES ]; do
            data=$(rdb invoke ${RPC_SVC} add_stream ${RPC_TIMEOUT} ${RPC_RESULT_LEN} epa_rdb_root $1 epa_type $2 epb_rdb_root $3 epb_type $4)
            [ "$data" = "Success" ] && break
            retry=$(($retry + 1))
            logDebug "Retrying # $retry"
            sleep $RETRY_WAIT
        done
        exit 0
    else
        rm -fr $PidFile
        logInfo "Removed defunct pid file: $pid"
    fi
fi

# make sure PidFile gets removed no matter how the script exits (unless SIGKILL)
trap 'rm -fr $PidFile 2> /dev/null' EXIT

# kill subprocess if this script gets killed
trap 'logDebug "aborted"; pkill -TERM -P $$' HUP INT QUIT PIPE TERM

logDebug "Starting dsm_btep_rpc server"

# Save pid only when we are about to launch the server
echo $$ > $PidFile

# Start dsm_bluetooth_ep as rpc server, which normally does not return.
# We have to launch it in background, so that signals can be caught by
# this script, which then sends SIGTERM to the rpc server for proper cleanup.
$DSM_BTEP_SERVER $@ &
wait $!

logDebug "dsm_btep_rpc server ended"

exit $?

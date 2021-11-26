#!/bin/sh
# processing rx msg

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
    echo ""
	echo "This shell script is for internal system use only."
	echo "It is used for processing incoming SMS messages."
    echo "Please do not run this script manually."
    echo ""
	exit 0
fi

# find platform
. /etc/platform.txt
if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
. /var/sms/sms_common.cfg
elif [ "$platform" = "Avian" ]; then
. /system/cdcs/usr/etc/sms/sms_common.cfg
else
. /usr/etc/sms/sms_common.cfg
fi

. $SMS_LIB_PATH/sms_utils

. $config_file
. $diag_config_file
. $ssmtp_config

atmgr_ready=`rdb_get atmgr.status`
MSG_ST_VAR="wwan.0.sms.received_message.status"

/usr/bin/sms_handler.sh rx

# set rdb variable for blocked operation
unblock_sms_rx_processing
rdb_set $MSG_ST_VAR
rdb_set wwan.0.sms.received_message.pending 0

# for cnsmgr using model, check SIM/ME memory status by calling sms.template again
# with fake sms staus
sms_if=`rdb_get wwan.0.if`
if [ "$sms_if" = "cns" ] && [ "$atmgr_ready" != "ready" ]; then
	block_sms_rx_processing
	sms_log "post rx msg proc : call sms_handler.sh rx again"
	/usr/bin/sms_handler.sh rx
	unblock_sms_rx_processing
	rdb_set wwan.0.sms.received_message.pending 0
fi

exit 0

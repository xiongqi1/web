#!/bin/sh
#
# execute pending SMS diagnostic command after reboot
#
# This script is firstly called during booting which means
# we don't have runtime sms config files in /var/... folder in Platypus system.

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
    echo ""
	echo "This shell script is for internal system use only."
	echo "It is used for processing pending SMS diagnostic commands."
    echo "Please do not run this script manually."
    echo ""
	exit 0
fi

# find platform
. /etc/platform.txt
. /usr/etc/sms/sms_common.cfg
. /usr/etc/sms/sms.cfg
. /usr/etc/sms/sms_diag.cfg
. $SMS_LIB_PATH/sms_utils

read_vars_n_create_cfg_files

case $1 in
	start)
		sms_log "Starting SMS TX counter reset daemon..."
		$SMS_BIN_PATH/sms_tx_counter.sh &
		sms_log "Checking for a pending SMS diagnostic command..."
		read_variable "smstools.pending_cmd" && PENDING_CMD=${RD_VAL}
		TRIMMED_CMD=`echo $PENDING_CMD | sed -e 's/\ //g'`
		if [ "$TRIMMED_CMD" == "" ]; then
			sms_log "No pending SMS diagnostic commands."
		exit 0
		fi
		read_variable "smstools.pending_cmd_user" && PENDING_CMD_USER=${RD_VAL}
		sms_log "Executing pending SMS diagnostic command: $PENDING_CMD_USER, $PENDING_CMD"
		write_variable "smstools.pending_cmd" ""
		write_variable "smstools.pending_cmd_user" ""
		(sleep 60; $SMS_BIN_PATH/sms_admin.sh "$PENDING_CMD_USER" "$PENDING_CMD")&
		;;
	stop)
		;;
	restart|reload)
		;;
	*)
		;;
esac

exit 0

#!/bin/sh
#
# run sms server for external sms client
#

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
    echo ""
	echo "This shell script is for internal system use only."
	echo "It is used to initialise the SMS server."
    echo "Please do not run this script manually."
    echo ""
	exit 0
fi

# This script is firstly called during booting which means
# we don't have runtime sms config files in /var/... folder in Platypus system.
# find platform
. /etc/platform.txt
. /usr/etc/sms/sms_common.cfg
. /usr/etc/sms/sms.cfg
. /usr/etc/sms/sms_diag.cfg
. $SMS_LIB_PATH/sms_utils

read_vars_n_create_cfg_files

. $config_file

if [ "$USE_EXT_SMS_CLIENT" != "1" ];then
	sms_log "External SMS client is turned off."
	exit 0
fi

if [ "$EXT_SMS_CLIENT_IP1" = "" ] && [ "$EXT_SMS_CLIENT_IP2" = "" ];then
	sms_log "External SMS client primary & secondary IP addresses are empty."
	exit 0
fi

if [ "$EXT_SMS_CLIENT_PORT" = "" ];then
	sms_log "External SMS client port number is empty."
	exit 0
fi

if [ ! -e "$SMS_BIN_PATH/sms_server" ]; then
	sms_log "Cannot find external SMS server binary."
	exit 0
fi

case $1 in
	start)
		sms_log "Starting SMS server..."
		$SMS_BIN_PATH/sms_server "$EXT_SMS_CLIENT_PORT" "$EXT_SMS_CLIENT_IP1" "$EXT_SMS_CLIENT_IP2" &
		;;
	stop)
		;;
	restart|reload)
		;;
	*)
		;;
esac

exit 0

#!/bin/sh

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo "This shell script file is for internal system use only."
	echo "It is used for setting the SMS Inbox pathname."
    echo "Please do not run this script manually."
	exit 0
fi

# find platform
. /etc/platform.txt
if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ]; then
# This script file is called very early before calling check_and_create_sms_dirs below
# and before copying sms config file to /var folder.
# Therefore should refer /usr/etc/sms location here
. /usr/etc/sms/sms_common.cfg
elif [ "$platform" = "Avian" ]; then
. /system/cdcs/usr/etc/sms/sms_common.cfg
else
. /usr/etc/sms/sms_common.cfg
fi

. $SMS_LIB_PATH/sms_utils

check_and_create_sms_dirs >/dev/null

. $config_file

# set rdb variable for inbox path name
rdb_set smstools.inbox_path ${LOCAL_INBOX}

exit 0


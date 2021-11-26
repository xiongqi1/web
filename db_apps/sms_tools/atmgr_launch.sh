#!/bin/sh

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo ""
	echo "This shell script is for internal system use only."
	echo "It is used for enabling/disabling SMS functionality."
	echo "Please do not run this script manually."
	echo ""
	exit 0
fi

sms_en=`rdb_get smstools.enable`
sms_opt=""
test "$sms_en" = "0" && sms_opt=" -x"
/system/cdcs/bin/simple_at_manager -p /dev/smd9 -m Qualcomm -i 0 $sms_opt
exit 0

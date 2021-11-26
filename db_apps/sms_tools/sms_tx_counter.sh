#!/bin/sh

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo "This is shell script is for internal system use only."
	echo "It is used for limiting the maximum number of SMS diagnostic"
    echo "TX messages."
    echo "Please do not run this script manually."
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

# reset tx sms counter at the beginning of time period
while true;do
	#CURR_YEAR=`date | awk '{print $6}'`
	#test $CURR_YEAR -lt 2011 && sleep 30 && continue
	case $MAX_DIAG_SMS_TX_LIMIT_PER in
	HOUR)
		CURR_TIME=`date | awk '{print $4}' | awk -F ":" '{print $2}'`	# find 00 minute
		if [ "$CURR_TIME" = "00" ]; then
		write_variable "smstools.diagsms_txcnt" "0"
		sms_log "reset SMS TX counter at xx:00 for HOUR period"
		fi
		;;
	DAY)
		CURR_TIME=`date | awk '{print $4}' | awk -F ":" '{print $1$2}'`	# find oo hour 00 minute
		if [ "$CURR_TIME" = "0000" ]; then
		write_variable "smstools.diagsms_txcnt" "0"
		sms_log "reset SMS TX counter at 00:00 for DAY period"
		fi
		;;
	WEEK)
		CURR_TIME=`date | awk '{print $4}' | awk -F ":" '{print $1$2}'`	# find oo hour 00 minute
		CURR_DAY=`date | awk '{print $1}'`		# find Mon
		if [ "$CURR_TIME" = "0000" ] && [ "$CURR_DAY" = "Mon" ]; then
		write_variable "smstools.diagsms_txcnt" "0"
		sms_log "reset SMS TX counter at Monday 00:00 for WEEK period"
		fi
		;;
	MONTH)
		CURR_TIME=`date | awk '{print $4}' | awk -F ":" '{print $1$2}'`	# find oo hour 00 minute
		CURR_DAY=`date | awk '{print $3}'`		# find 1
		if [ "$CURR_TIME" = "0000" ] && [ "$CURR_DAY" = "1" ]; then
		write_variable "smstools.diagsms_txcnt" "0"
		sms_log "reset SMS TX counter at 00:00 1st of month for MONTH period"
		fi
		;;
	*)
		;;
	esac
	sleep 30		# check every 1 minute
done

exit 0

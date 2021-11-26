#!/bin/sh
# $1 : incoming sms filename

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

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo ""
	echo "This shell script is for internal system use only."
	echo "It is used as part of the SMS diagnostic functionality."
	echo "Please do not run this script manually."
	echo ""
	exit 0
fi

#==========================================================================
#                     D  E  F  I  N  I  T  I  O  N  S
#==========================================================================


#==========================================================================
#                     S  U  B  R  O  U  T  I  N  E  S
#==========================================================================

#--------------------------------------------------------------------------
# delete diagnostics text message stored in SIM/ME memory and move to diag folder
# Platypus dedicated function
#--------------------------------------------------------------------------
delete_stored_diag_msg()
{
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		if [ "$DIAG_RX_ID" != "" ]; then
			sms_log "delete rxed diag msg #$DIAG_RX_ID from SIM/ME storage"
			delete_one_msg $DIAG_RX_ID
		fi
		if [ "$DIAG_TX_ID" != "" ]; then
			sms_log "delete forwarded diag msg #$DIAG_TX_ID from SIM/ME storage"
			delete_one_msg $DIAG_TX_ID
		fi
		if [ "$DIAG_RX_FILE" != "" ]; then
			sms_log "save diag msg file $DIAG_RX_FILE to local diag msg folder"
			mv $DIAG_RX_FILE $DIAG_INBOX/
		fi
		export DIAG_RX_FILE=""
		export DIAG_RX_ID=""
		export DIAG_TX_ID=""
	fi
}

#--------------------------------------------------------------------------
# increase sms tx count by 1, check max. tx limit
#--------------------------------------------------------------------------
increase_sms_tx_count_by_one()
{
	read_variable "smstools.diagsms_txcnt"
	CUR_SMS_TX_CNT=${RD_VAL}
	#sms_log "current tx count = ${RD_VAL}"
	let "CUR_SMS_TX_CNT+=1"

	sms_log "cur_cnt $CUR_SMS_TX_CNT > max cnt $MAX_DIAG_SMS_TX_LIMIT ?"
	if [ "$CUR_SMS_TX_CNT" -gt "$MAX_DIAG_SMS_TX_LIMIT" ]; then
		send_error_notification "LIMIT EXCEED"
		return 0
	fi

	write_variable "smstools.diagsms_txcnt" $CUR_SMS_TX_CNT
	return 1
}

#--------------------------------------------------------------------------
# increase sms tx count & send sms response
#--------------------------------------------------------------------------
send_diag_sms_response()
{
	# remove last trailing ';'
	REPLY_SMS=`echo ${REPLY_SMS} | sed -e 's/;$//'`
	sms_log "send reply sms : ${REPLY_SMS}"
	increase_sms_tx_count_by_one
	test "$?" = "1" && sendsms "$DEST_NO" "${REPLY_SMS}" "DIAG"
	REPLY_SMS=""
}

#--------------------------------------------------------------------------
# check if sender is in white list (compare last 9 digit only)
#--------------------------------------------------------------------------
find_in_whitelist()
{
	if [ "$USE_WHITELIST" = "0" ]; then
		sms_log "auth is disabled, bypass auth check !"
		return
	fi

	if [ "${SENDER}" = "" ]; then
		sms_log "SENDER is null, quit"
		WL_FAIL="1"
		return
	fi

	tmplen=`expr length ${SENDER}`
	let "tmpidx=tmplen-8"
	tmpsender=`expr substr ${SENDER} $tmpidx 9`
	read_variable "smstools.max_wl_no_idx" && MAX_WL_TX_IDX=${RD_VAL}
	let "WL_IDX=0"
	WL_FOUND="FALSE"
	while [ "$WL_IDX" -le "$MAX_WL_TX_IDX" ]; do
		eval wldiaguserno=\${"DIAG_USER_NO"$WL_IDX}
		eval wldiagpassword=\${"DIAG_PASSWORD"$WL_IDX}
		if [ "${wldiaguserno}" = "" ]; then
			let "WL_IDX+=1"
			continue;
		fi
		tmplen=`expr length ${wldiaguserno}`
		let "tmpidx=tmplen-8"
		tmpuser=`expr substr ${wldiaguserno} $tmpidx 9`
		if [ "$tmpsender" = "$tmpuser" ]; then
			sms_log "found '$SENDER' in whitelist [$WL_IDX], pwd '${wldiagpassword}'"
			WL_FOUND="TRUE"
			break;
		fi
		let "WL_IDX+=1"
	done

	if [ "$WL_FOUND" = "FALSE" ]; then
		sms_log "Can't find '$SENDER' in whitelist, quit"
		WL_FAIL="1"
		return
	fi
}

#--------------------------------------------------------------------------
# check & parse command
# $1 : error cause
#      "READ ONLY", "NONEXIST", "FORMAT ERROR", "AUTH FAIL", "LIMIT EXCEED",
#      "NO ACTIVE PROFILE", "BLOCKED_VARS", "BLOCKED_CMDS"
# $2 : if "NOW", send right now, else send later for multiple reply
#--------------------------------------------------------------------------
send_error_notification()
{
	if [ "$ENABLE_ERROR_NOTI" = "0" ]; then
		sms_log "SMS error noti is OFF, do not send error noti !"
		return
	fi
	if [ "$USE_FIXED_ERROR_NOTI_DEST" = "1" ]; then
		DEST_NO=$FIXED_ERROR_NOTI_DEST_NO
	else
		DEST_NO=$SENDER
	fi
	if [ "$DEST_NO" = "" ]; then
		sms_log "Destination of SMS error noti is null, can not send error noti !"
		return
	fi
	sms_log "record SMS error noti : $1"
	if [ "$1" = "READ ONLY" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot write to variable. Cause:Read only variable\";"
	elif [ "$1" = "NONEXIST" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot read/write variable. Cause:Unknown variable\";"
	elif [ "$1" = "FORMAT ERROR" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot execute command. Cause:Command format not recognized.\";"
	elif [ "$1" = "AUTH FAIL" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot execute command. Cause:Authentication failed.\";"
	elif [ "$1" = "LIMIT EXCEED" ]; then
		#REPLY_SMS="${REPLY_SMS}\"Error:Cannot send SMS. Cause:SMS transmit limit exceeded.\";"
		sms_log "${REPLY_SMS}\"Error:Cannot send SMS. Cause:SMS transmit limit exceeded.\";"
	elif [ "$1" = "NO ACTIVE PROFILE" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:No Active Profile.\";"
	elif [ "$1" = "BLOCKED_VARS" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot read/write variable. Cause:Blocked variable\";"
	elif [ "$1" = "BLOCKED_CMDS" ]; then
		REPLY_SMS="${REPLY_SMS}\"Error:Cannot execute command. Cause:Blocked command.\";"
	else
		sms_log "unknown error cause - ${REPLY_SMS}"
	fi
	if [ "$2" = "NOW" ] && [ "${REPLY_SMS}" != "" ]; then
		send_diag_sms_response
	fi
}


#--------------------------------------------------------------------------
# check & parse command
# $1 : whitelist password, $2 : password from command line
#--------------------------------------------------------------------------
check_password_match()
{
	if [ "$1" != "$2" ]; then
		sms_log "passwords do not match !"
		send_error_notification "AUTH FAIL" "NOW"
		delete_stored_diag_msg
		exit 0
	fi
	sms_log "passwords test passed !"
}


#--------------------------------------------------------------------------
# check & parse command
# $1 : command, $2 : variable/command, $3 : command result
# $4 : if "NOW", send right now, else send later for multiple reply
#--------------------------------------------------------------------------
send_result_and_ack()
{
	if [ "$USE_FIXED_ACK_DEST" = "1" ]; then
		DEST_NO=$FIXED_ACK_DEST_NO
	else
		DEST_NO=$SENDER
	fi
	if [ "$DEST_NO" = "" ]; then
		sms_log "Destination of SMS result/ack is null, can not send result/ack !"
		return
	fi
	sms_log "record SMS result/ack for $1 command"
	if [ "$1" = "get" ]; then
		sms_log "GET command done: ${2}=${3}"
		REPLY_SMS="${REPLY_SMS}\"${2}=${3}\";"
	elif [ "$1" = "set" ]; then
		if [ "$ENABLE_SET_CMD_ACK" = "1" -o "${2}" = "zerosms" ]; then
			if [ "${2}" = "zerosms" ]; then
				sms_log "Special command done : ${3}"
				REPLY_SMS="${REPLY_SMS}\"Successfully done special command : ${3}\";"
			else
				sms_log "SET command done: ${2}=${3}"
				REPLY_SMS="${REPLY_SMS}\"Successfully set ${2} to ${3}\";"
			fi
		else
			sms_log "SMS ack. is OFF, do not send ack !"
		fi
	elif [ "$1" = "execute" ]; then
		# clear
		param2_for_ack=$(echo "$3" | sed "s/^[[:space:]]*$//g")

		sms_log "EXEC command done: ${2}"
		if [ "$param2_for_ack" = "" ]; then
			REPLY_SMS="${REPLY_SMS}\"Successfully executed command ${2}\";"
		else
			REPLY_SMS="${REPLY_SMS}\"Successfully executed command ${2}:${param2_for_ack}\";"
		fi
	else
		sms_log "unknown command"
	fi
	if [ "$4" = "NOW" ] && [ "${REPLY_SMS}" != "" ]; then
		send_diag_sms_response
	fi
}


#--------------------------------------------------------------------------
# convert time format from UTC seconds
# $1 : UTC time
#--------------------------------------------------------------------------
convert_time_format()
{
	# change to integer
	input_time=`echo $1 | sed 's/\..*//g'`
	sms_log "input time in UTC : $input_time"
	seconds=$(( input_time%60 ))
	test "$seconds" -le "9" && seconds="0$seconds"
	minutes=$(( input_time/60%60 ))
	test "$minutes" -le "9" && minutes="0$minutes"
	hours=$(( input_time/60/60%24 ))
	test "$hours" -le "9" && hours="0$hours"
	days=$(( input_time/60/60/24 ))
	sms_log "converted to $days days, $hours hours, $minutes minutes, $seconds seconds"
	CONVERTED_T="$days days, $hours:$minutes:$seconds"
}



#--------------------------------------------------------------------------
# find active profile
# if profile no is selected, use the number
# else find active profile no
# if all disabled, return error
#--------------------------------------------------------------------------
find_active_profile()
{
	test "$PF_NO" != "" && return
	for i in 1 2 3 4 5 6; do
		ENABLED=`rdb_get link.profile.$i.enable`
		if [ "$ENABLED" = "1" ]; then
			sms_log "found active profile $i"
			PF_NO=$i
			return
		fi
	done
}


#--------------------------------------------------------------------------
# process predefined get command
# $1 : get/set, $2 : variable, $3 : value
#--------------------------------------------------------------------------
process_predefined_db_command()
{
	sms_log "process predefined db cmd('$1', '$2', '$3')"

	## extract profile no
	tmpvar=`expr substr $2 1 $((${#2}-1))`
	tmppno=`expr substr $2 ${#2} 1`
	# profile, apn, username, password, authtype, wanip may have profile no or not.
	if [ "$tmpvar" = "profile" ] || [ "$tmpvar" = "apn" ] ||
	[ "$tmpvar" = "username" ] || [ "$tmpvar" = "password" ] ||
	[ "$tmpvar" = "authtype" ] || [ "$tmpvar" = "wanip" ]; then
		if [ "$tmppno" -lt "0" ] || [ "$tmppno" -gt "9" ]; then
			sms_log "profile number ($tmppno) is out of range!"
			return 255
		fi
		DB_VAR="${tmpvar}"
		PF_NO="${tmppno}"
		sms_log "var = '$DB_VAR', profile no = '$PF_NO'"
	elif [ "$2" = "profile" ] || [ "$2" = "apn" ] ||
		[ "$2" = "username" ] || [ "$2" = "password" ] ||
		[ "$2" = "authtype" ] || [ "$2" = "wanip" ] ||
		[ "$2" = "rssi" ] || [ "$2" = "imei" ] ||
		[ "$2" = "usage" ] || [ "$2" = "wanuptime" ] ||
		[ "$2" = "deviceuptime" ] || [ "$2" = "band" ]; then
		DB_VAR="${2}"
		sms_log "var = '$DB_VAR'"
	else
		if [ "$ACCESS_GENERIC_RDB_VARS" = "1" ]; then
			sms_log "'$2' is not a predefined keyword, process as generic db variables"
			return 0
		else
			sms_log "'$2' is not a predefined keyword and accessing generic RDB variable is blocked, skip this"
			send_error_notification "BLOCKED_VARS"
			return 255
		fi
	fi

	##------------------------------
	## check readonly variables
	if [ "$1" = "set" ]; then
		if [ "$DB_VAR" = "wanip" ] || [ "$DB_VAR" = "rssi" ] ||
		[ "$DB_VAR" = "imei" ] || [ "$DB_VAR" = "usage" ] ||
		[ "$DB_VAR" = "wanuptime" ] || [ "$DB_VAR" = "deviceuptime" ] ||
		[ "$DB_VAR" = "band" ]; then
			sms_log "'$DB_VAR' is a readonly variable!"
			send_error_notification "READ ONLY"
			return 255
		fi
	fi

	##------------------------------
	## check profile number
	if [ "$DB_VAR" = "profile" ] || [ "$DB_VAR" = "apn" ] ||
	[ "$DB_VAR" = "username" ] || [ "$DB_VAR" = "password" ] ||
	[ "$DB_VAR" = "authtype" ] || [ "$DB_VAR" = "wanip" ]; then
		find_active_profile
		if [ "$PF_NO" = "" ]; then
			if [ "$1" = "set" ] && [ "$DB_VAR" = "apn" ] && [ "${3}" = "0" ]; then
				PF_NO=1
			else
				sms_log "No Active Profile!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
		fi
	fi

	##------------------------------
	## index 0 : profile(x) cmd
	if [ "$1" = "get" ] && [ "$DB_VAR" = "profile" ]; then
		DB_APN=`rdb_get link.profile.$PF_NO.apn`
		if [ "$platform" != "Platypus" ]; then
			DB_USER=`rdb_get link.profile.$PF_NO.user`
			DB_PASS=`rdb_get link.profile.$PF_NO.pass`
			DB_AUTH=`rdb_get link.profile.$PF_NO.auth_type`
		else
			DB_USER=`nvram_get wwan_user`
			DB_PASS=`nvram_get wwan_pass`
			DB_AUTH=`nvram_get wwan_auth`
		fi
		DB_IP=`rdb_get link.profile.$PF_NO.iplocal`
		DB_STATUS=`rdb_get link.profile.$PF_NO.status`
		RESPONSE="$PF_NO,$DB_APN,$DB_USER,$DB_PASS,$DB_AUTH,$DB_IP,$DB_STATUS"

	elif [ "$1" = "set" ] && [ "$DB_VAR" = "profile" ]; then
		DB_APN=`echo ${3} |  awk -F "," '{print $1}'`
		DB_USER=`echo ${3} |  awk -F "," '{print $2}'`
		DB_PASS=`echo ${3} |  awk -F "," '{print $3}'`
		DB_AUTH=`echo ${3} |  awk -F "," '{print $4}'`
		if [ "$DB_APN" = "" ]; then
			sms_log "wrong apn name : '$DB_APN'"
			send_error_notification "FORMAT ERROR"
			return 255
		else
			rdb_set link.profile.$PF_NO.apn "${DB_APN}"
		fi
		if [ "$platform" != "Platypus" ]; then
			rdb_set link.profile.$PF_NO.user "${DB_USER}"
			rdb_set link.profile.$PF_NO.pass "${DB_PASS}"
			if [ "$DB_AUTH" = "chap" ] || [ "$DB_AUTH" = "pap" ]; then
				rdb_set link.profile.$PF_NO.auth_type "${DB_AUTH}"
			else
				sms_log "wrong auth type : '$DB_AUTH'"
				send_error_notification "FORMAT ERROR"
				return 255
			fi
		else
			nvram_get wwan_user $DB_USER
			nvram_get wwan_pass $DB_PASS
			test "$DB_AUTH" = "chap" && DB_AUTH="CHAP"
			test "$DB_AUTH" = "pap" && DB_AUTH="PAP"
			if [ "$DB_AUTH" = "CHAP" ] || [ "$DB_AUTH" = "PAP" ]; then
				nvram_set wwan_auth "${DB_AUTH}"
			else
				sms_log "wrong auth type : '$DB_AUTH'"
				send_error_notification "FORMAT ERROR"
				return 255
			fi
		fi

	##------------------------------
	## index 1 : apn(x) cmd
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "apn" ]; then
		AUTOAPN=`rdb_get webinterface.autoapn`
		if [ "$AUTOAPN" = "1" ]; then
			RESPONSE=`rdb_get wwan.0.apn.current`
		else
			find_active_profile
			RESPONSE=`rdb_get link.profile.$PF_NO.apn`
		fi
		if [ "$RESPONSE" = "" ]; then
			RESPONSE="Blank"
		fi
	elif [ "$1" = "set" ] && [ "$DB_VAR" = "apn" ]; then
		if [ "${3}" = "0" ]; then
			rdb_set webinterface.autoapn 1
			rdb_set link.profile.$PF_NO.enable 1
		else
			rdb_set webinterface.autoapn 0
			rdb_set link.profile.$PF_NO.apn "${3}"
		fi
		PROFILE_EN=`rdb_get link.profile.$PF_NO.enable`
		if [ "$PROFILE_EN" = "1" ]; then
			rdb_set link.profile.$PF_NO.enable 0
			sleep 1
			rdb_set link.profile.$PF_NO.enable 1
		fi

	##------------------------------
	## index 2 : username(x) cmd
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "username" ]; then
		if [ "$platform" != "Platypus" ]; then
			RESPONSE=`rdb_get link.profile.$PF_NO.user`
		else
			RESPONSE=`nvram_get wwan_user`
		fi
	elif [ "$1" = "set" ] && [ "$DB_VAR" = "username" ]; then
		if [ "$platform" != "Platypus" ]; then
			rdb_set link.profile.$PF_NO.user "${3}"
		else
			nvram_set wwan_user "${3}"
		fi

	##------------------------------
	## index 3 : password(x) cmd
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "password" ]; then
		if [ "$platform" != "Platypus" ]; then
			RESPONSE=`rdb_get link.profile.$PF_NO.pass`
		else
			RESPONSE=`nvram_get wwan_pass`
		fi
	elif [ "$1" = "set" ] && [ "$DB_VAR" = "password" ]; then
		if [ "$platform" != "Platypus" ]; then
			rdb_set link.profile.$PF_NO.pass "${3}"
		else
			nvram_set wwan_pass "${3}"
		fi

	##------------------------------
	## index 4 : authtype(x) cmd
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "authtype" ]; then
		if [ "$platform" != "Platypus" ]; then
			RESPONSE=`rdb_get link.profile.$PF_NO.auth_type`
		else
			RESPONSE=`nvram_get wwan_auth`
		fi
	elif [ "$1" = "set" ] && [ "$DB_VAR" = "authtype" ]; then
		if [ "$platform" != "Platypus" ]; then
			if [ "$3" = "chap" ] || [ "$3" = "pap" ]; then
				rdb_set link.profile.$PF_NO.auth_type "${3}"
			else
				sms_log "wrong auth type : '$3'"
				send_error_notification "FORMAT ERROR"
				return 255
			fi
		else
			test "$DB_AUTH" = "chap" && DB_AUTH="CHAP"
			test "$DB_AUTH" = "pap" && DB_AUTH="PAP"
			if [ "$3" = "CHAP" ] || [ "$3" = "PAP" ]; then
				nvram_set wwan_auth "${3}"
			else
				sms_log "wrong auth type : '$3'"
				send_error_notification "FORMAT ERROR"
				return 255
			fi
		fi

	##------------------------------
	## index 5 : wanip(x) cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "wanip" ]; then
		RESPONSE=`rdb_get link.profile.$PF_NO.iplocal`

	##------------------------------
	## index 6 : rssi cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "rssi" ]; then
		RESPONSE=`rdb_get wwan.0.radio.information.signal_strength`

	##------------------------------
	## index 7 : imei cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "imei" ]; then
		RESPONSE=`rdb_get wwan.0.imei`

	##------------------------------
	## index 8 : usage cmd, Read Only
	## statistics.usage_current 1291243857,1291244009,2012,1632
	##                          start time, end time, rx, tx
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "usage" ]; then
		if [ "$atmgr_ready" != "ready" ]; then		# for platform using cnsmgr only
			USG_STR=`rdb_get statistics.usage_current`
			if [ "${USG_STR}" = "wwan down" ]; then
				RESPONSE="Rx 0 byte, Tx 0 byte, Total 0 byte"
			else
				RX_BYTES=`echo "$USG_STR" | awk -F "," '{print $3}'`
				TX_BYTES=`echo "$USG_STR" | awk -F "," '{print $4}'`
				let "TOTAL_BYTES=$RX_BYTES+$TX_BYTES"
				RESPONSE="Rx $RX_BYTES bytes, Tx $TX_BYTES bytes, Total $TOTAL_BYTES bytes"
			fi
		else
			USG_STR=`rdb_get link.profile.1.status`
			export WWAN_3G_IF=`rdb_get link.profile.1.interface`
			CURR_3G_IF=`awk "/$WWAN_3G_IF/"'{print $0}' /proc/net/dev`
			if [ "$USG_STR" != "up" ] || [ "$WWAN_3G_IF" = "" ] || [ "$CURR_3G_IF" = "" ]; then
				RESPONSE="Rx 0 byte, Tx 0 byte, Total 0 byte"
			else
				RX_BYTES=`echo "$CURR_3G_IF" | awk '{print $2}'`
				TX_BYTES=`echo "$CURR_3G_IF" | awk '{print $10}'`
				let "TOTAL_BYTES=$RX_BYTES+$TX_BYTES"
				RESPONSE="Rx $RX_BYTES bytes, Tx $TX_BYTES bytes, Total $TOTAL_BYTES bytes"
			fi
		fi

	##------------------------------
	## index 9 : wanuptime cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "wanuptime" ]; then
		if [ "$atmgr_ready" != "ready" ]; then		# for platform using cnsmgr only
			ELAPSED_T=`rdb_get statistics.wanuptime`
			if [ "$ELAPSED_T" = "wwan down" ]; then
				RESPONSE="WWAN uptime 00:00:00"
			else
				convert_time_format "$ELAPSED_T"
				RESPONSE="WWAN uptime $CONVERTED_T"
			fi
		else
			USG_STR=`rdb_get link.profile.1.status`
			if [ "$USG_STR" != "up" ]; then
				RESPONSE="WWAN uptime 00:00:00"
			else
				START_T=`rdb_get link.profile.1.starttime`
				START_T=`echo $START_T | sed 's/\..*//g'`
				END_T=`rdb_get link.profile.1.endtime`
				END_T=`echo $END_T | sed 's/\..*//g'`
				SYS_UP_TIME=`cat /proc/uptime | awk '{print $1}' | awk -F "." '{print $1}'`
				if [ "$END_T" = "" ]; then
					let "ELAPSED_T=$SYS_UP_TIME-$START_T"
				else
					let "ELAPSED_T=$END_T-$START_T"
				fi
				convert_time_format "$ELAPSED_T"
				RESPONSE="WWAN uptime $CONVERTED_T"
			fi
		fi

	##------------------------------
	## index 10 : deviceuptime cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "deviceuptime" ]; then
		SYS_UP_TIME=`cat /proc/uptime | awk '{print $1}' | awk -F "." '{print $1}'`
		convert_time_format "$SYS_UP_TIME"
		RESPONSE="system uptime $CONVERTED_T"

	##------------------------------
	## index 11 : band cmd, Read Only
	elif [ "$1" = "get" ] && [ "$DB_VAR" = "band" ]; then
		RESPONSE=`rdb_get wwan.0.system_network_status.current_band`

	else
		sms_log "Could not process this variable: '$DB_VAR'"
		return 0
	fi

	return 1
}



#--------------------------------------------------------------------------
# process predefined get command
# $1 : command (reboot, pdpcycle, pdpdown, pdpup)
#--------------------------------------------------------------------------
process_predefined_exe_command()
{
	predefined_cmd_list="reboot pdpcycle pdpdown pdpup"
	for i in $predefined_cmd_list; do
		found=`echo $1 | grep "$i" | grep -v "grep"`
		test ! -z $found && break
	done
	if [ -z $found ]; then
		if [ "$ALLOW_GENERIC_CMDS" = "1" ]; then
			sms_log "'$1' is not a predefined command, try to run as generic command"
			return 0
		else
			sms_log "'$1' is not a predefined command and execution of generic commands is blocked, skip this"
			send_error_notification "BLOCKED_CMDS"
			return 255
		fi
	fi

	sms_log "process predefined executable cmd('$1')"

	## extract profile no
	tmpcmd=`expr substr $1 1 $((${#1}-1))`
	tmppno=`expr substr $1 ${#1} 1`

	# pdpcycle, pdpdown, pdpup may have profile no or not.
	if [ "$tmpcmd" = "pdpcycle" ] || [ "$tmpcmd" = "pdpdown" ] || [ "$tmpcmd" = "pdpup" ]; then
		if [ "$tmppno" -lt "0" ] || [ "$tmppno" -gt "9" ]; then
			sms_log "profile number ($tmppno) is out of range!"
			return 255
		fi
		CMD_VAR="${tmpcmd}"
		PF_NO="${tmppno}"
		sms_log "cmd = '$CMD_VAR', profile no = '$PF_NO'"
	elif [ "$1" = "reboot" ] || [ "$1" = "pdpcycle" ] ||
		[ "$1" = "pdpdown" ] || [ "$1" = "pdpup" ]; then
		CMD_VAR="${1}"
		sms_log "cmd = '$CMD_VAR'"
	else
		if [ "$ALLOW_GENERIC_CMDS" = "1" ]; then
			sms_log "'$1' is not a predefined command, try to run as generic command"
			return 0
		else
			sms_log "'$1' is not a predefined command and execution of generic commands is blocked, skip this"
			send_error_notification "BLOCKED_CMDS"
			return 255
		fi
	fi

	##------------------------------------------------------
	## command 1 : soft reboot
	if [ "$CMD_VAR" = "reboot" ]; then
		sms_log "save remaining command and call reboot"
		TRIMMED_CMD=`echo ${CMD_LINE} | sed -e 's/\ //g'`
		if [ -z "$TRIMMED_CMD" ]; then
			sms_log "no remaining command"
		else
			sms_log "remaining command : '${LAST_VALID_PASSWORD} ${CMD_LINE}'"
			write_variable "smstools.pending_cmd_user" $SENDER
			write_variable "smstools.pending_cmd" "${LAST_VALID_PASSWORD} ${CMD_LINE}"
		fi
		send_result_and_ack "$DIAG_MODE" "reboot" "" "NOW"
		delete_stored_diag_msg
		(sleep 2; reboot)&
		return 2

	##------------------------------------------------------
	## command 2 : disconnect and reconnect 3G connection
	elif [ "$CMD_VAR" = "pdpcycle" ]; then
		sms_log "call pdpcycle"
		if [ "$PF_NO" = "" ]; then
			find_active_profile
			if [ "$PF_NO" = "" ]; then
				sms_log "no profile selected, no active profile, No need to run pdpcycle!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
			sms_log "no profile selected, try to down/up current active profile($PF_NO)"
			rdb_set link.profile.$PF_NO.enable 0
			write_variable "smstools.last_active_profile" $PF_NO
			sleep 5
			rdb_set link.profile.$PF_NO.enable 1
		else
			TGT_ACT=`rdb_get link.profile.$PF_NO.enable`
			if [ "$TGT_ACT" != "1" ]; then
				sms_log "profile $PF_NO is not activated, No need to run pdpcycle!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
			sms_log "down/up profile $PF_NO"
			rdb_set link.profile.$PF_NO.enable 0
			write_variable "smstools.last_active_profile" $PF_NO
			sleep 5
			rdb_set link.profile.$PF_NO.enable 1
		fi

	##------------------------------------------------------
	## command 3 : disconnect 3G connection
	elif [ "$CMD_VAR" = "pdpdown" ]; then
		sms_log "call pdpdown"
		if [ "$PF_NO" = "" ]; then
			find_active_profile
			if [ "$PF_NO" = "" ]; then
				sms_log "no profile selected, no active profile, No need to run pdpdown!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
			sms_log "no profile selected, try to down current active profile($PF_NO)"
			rdb_set link.profile.$PF_NO.enable 0
			write_variable "smstools.last_active_profile" $PF_NO
		else
			TGT_ACT=`rdb_get link.profile.$PF_NO.enable`
			if [ "$TGT_ACT" = "0" ]; then
				sms_log "profile $PF_NO is not activated, No need to run pdpdown!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
			sms_log "deactivate profile $PF_NO"
			rdb_set link.profile.$PF_NO.enable 0
			write_variable "smstools.last_active_profile" $PF_NO
		fi

	##------------------------------------------------------
	## command 4 : connect 3G connection
	elif [ "$CMD_VAR" = "pdpup" ]; then
		sms_log "call pdpup"
		read_variable "smstools.last_active_profile" && LAST_AP=${RD_VAL}
		if [ "$PF_NO" = "" ]; then
			sms_log "no profile selected, try to up last active profile($LAST_AP)"
			if [ "$LAST_AP" = "" ]; then
				sms_log "No last active profile saved. Give-up 3G up!"
				send_error_notification "NO ACTIVE PROFILE"
				return 255
			fi
			find_active_profile
			if [ "$PF_NO" != "" ]; then
				sms_log "already profile $PF_NO is activated. No need to run pdpup!"
				return 1
			fi
			sms_log "activate last activated profile $LAST_AP"
			rdb_set link.profile.$LAST_AP.enable 1
		else
			TGT_PF_NO=$PF_NO
			PF_NO=""
			find_active_profile
			if [ "$TGT_PF_NO" = "$PF_NO" ]; then
				sms_log "already profile $PF_NO is activated. No need to run pdpup!"
				return 1
			fi
			if [ "$PF_NO" != "" ]; then
				sms_log "profile $PF_NO is already activated. turn off it and activate profile $TGT_PF_NO!"
				rdb_set link.profile.$PF_NO.enable 0
				sleep 5
				rdb_set link.profile.$TGT_PF_NO.enable 1
			else
				sms_log "activate profile $TGT_PF_NO"
				rdb_set link.profile.$TGT_PF_NO.enable 1
			fi
		fi
	fi
	return 1
}


get_decoded_cmd_line()
{
	CODING_SCHME=`expr substr "${FULL_CMD_LINE}" 1 5`
	sms_log "coding scheme = $CODING_SCHME"
	if [ "$CODING_SCHME" != "GSM7:" -a "$CODING_SCHME" != "8BIT:" -a "$CODING_SCHME" != "UCS2:" ];then
		sms_log "message without coding schme"
		CMD_LINE="${FULL_CMD_LINE}"
		return 1
	fi
	FULL_CMD_LEN=${#FULL_CMD_LINE}
	let "RAW_CMD_LEN=$FULL_CMD_LEN-5"
	RAW_CMD_LINE=`expr substr "${FULL_CMD_LINE}" 6 $RAW_CMD_LEN`
	CMD_LINE="${RAW_CMD_LINE}"
}

#==========================================================================
#                     M  A  I  N
#==========================================================================
sms_log "========================================================="

#--------------------------------------------------------------------------
# check & parse command
#--------------------------------------------------------------------------
SENDER=$1
FULL_CMD_LINE="${2}"
MSG_FILE="${3}"
get_decoded_cmd_line
sms_log "From = '$SENDER', cmd_line = '${CMD_LINE}'"

#--------------------------------------------------------------------------
# check if sender is in white list (compare last 9 digit only)
#--------------------------------------------------------------------------
find_in_whitelist

if test -n "`echo ${CMD_LINE} | grep -i "get" | grep -v "grep"`"; then
	sms_log "found remote diag cmd keyword [get]"
	KEYWORD_FOUND="1"
elif test -n "`echo ${CMD_LINE} | grep -i "set" | grep -v "grep"`"; then
	sms_log "found remote diag cmd keyword [set]"
	KEYWORD_FOUND="1"
elif test -n "`echo ${CMD_LINE} | grep -i "execute" | grep -v "grep"`"; then
	sms_log "found remote diag cmd keyword [execute]"
	KEYWORD_FOUND="1"
else
	#checking for possible extra commands
	extra_commands=`exec process_predefined_command.awk ${CMD_LINE}`
	sms_log "[process_predefined_command.awk] result='$extra_commands'"
	if [ "$extra_commands" != "none" ] && [ "$extra_commands" != "" ]; then
		eval $extra_commands
		sms_log "extra_command = '$extra_command'"
		if [ "$extra_command" == "zerosms" ]; then
			sms_log "found wakeup command, send ack immediately here"
			send_result_and_ack "$extra_mode" "$extra_command" "$extra_reply_sms" "NOW"
		fi
	else
		sms_log "not found remote diag cmd keyword"
	fi
	exit 0
fi

# delete diag msg from SIM/ME only when this message has diag key word
if [ "$WL_FAIL" = "1" ]; then
	test "$KEYWORD_FOUND" = "1" && delete_stored_diag_msg
	exit 0
fi

# strip trailing

REPLY_SMS=""


#--------------------------------------------------------------------------
# separate each command line and parse password/command/variable
#--------------------------------------------------------------------------
let "SUB_CMD_IDX=0"
while [ "${CMD_LINE}" != "" ]; do
	let "SUB_CMD_IDX+=1"
	sms_log "---------------------------------------------------------"
	SUB_CMD=`echo "${CMD_LINE}" | awk -F ';' '{print $1}'`
	CMD_LEN=${#CMD_LINE}
	SUB_CMD_LEN=${#SUB_CMD}
	#sms_log "s $SUB_CMD_LEN"
	if [ "$CMD_LEN" = "$SUB_CMD_LEN" ]; then
		CMD_LINE=""
	else
		let "cplen=$CMD_LEN-$SUB_CMD_LEN-1"
		#sms_log "s $SUB_CMD_LEN, t $cplen"
		CMD_LINE=`expr substr "${CMD_LINE}" "$(($SUB_CMD_LEN+2))" $cplen | sed -e 's/^\ *//'`
	fi
	sms_log "sub_cmd = ${SUB_CMD}, remaining cmd = ${CMD_LINE}"

	# strip double apostrophes at the beginning and ending,
	# remove leading and trainling spaces
	SUB_CMD=`echo ${SUB_CMD} | sed -e 's/^"//' -e 's/^\ *//' -e 's/ *$//' -e 's/"$//' -e 's/ *$//'`
	#sms_log "stripped sub cmd = ${SUB_CMD}"
	SUB_CMD_LEN=${#SUB_CMD}
	#sms_log "s2 $SUB_CMD_LEN"

	# first find get/set/execute cmd in cmd_line
	if test -n "`echo ${SUB_CMD} | grep -i "execute" | grep -v "grep"`"; then
		DIAG_MODE="execute"
		SUBST_STR="[eE][xX][eE][cC][uU][tT][eE]"
	elif test -n "`echo ${SUB_CMD} | grep -i "get" | grep -v "grep"`"; then
		DIAG_MODE="get"
		SUBST_STR="[gG][eE][tT]"
	elif test -n "`echo ${SUB_CMD} | grep -i "set" | grep -v "grep"`"; then
		DIAG_MODE="set"
		SUBST_STR="[sS][eE][tT]"
	else
		sms_log "can not find command in sub cmd line!"
		continue
	fi
	sms_log "found [$DIAG_MODE] sub command"

	# separate password and cmd in cmd_line
	SUB_CMD=`echo "${SUB_CMD}" | sed "s/$SUBST_STR/$DIAG_MODE/"`
	FLD1=`echo "${SUB_CMD}" | awk -F "$DIAG_MODE" '{print $1}'`
	FLD2=`echo "${SUB_CMD}" | awk -F "$DIAG_MODE" '{for(i=2;i<=NF;i++) print $i}'`
	#sms_log "fld 1 = '${FLD1}'"
	#sms_log "fld 2 = '${FLD2}'"
	SUB_PWD=${FLD1}
	SUB_PWD_LEN=${#SUB_PWD}

	# check if password field is real password or not
	if test `expr substr $FLD1 1 5` = "Error"; then
		sms_log "found response message of diagnostic command, discard!"
		delete_stored_diag_msg
		exit 0
	fi
	# password can be any number or characters 
	#if test ! -z $(echo $FLD1 | sed 's/[0-9]//g') && test ! -z $FLD1; then
	#	sms_log "wrong password field, stop checking diag command"
	#	exit 0
	#fi

	if [ "$SUB_PWD_LEN" = "0" ]; then
		SUB_VAR=`echo ${SUB_CMD} | sed -e "s/$DIAG_MODE//" -e 's/^\ *//'`
	else
		let "cplen=$SUB_CMD_LEN-$SUB_PWD_LEN"
		SUB_PWD=`echo ${SUB_PWD} | sed -e 's/^\ *//' -e 's/ *$//'`
		#sms_log "sub pwd = '${SUB_PWD}', ${SUB_PWD_LEN}"
		test ! -z "${SUB_PWD}" && LAST_VALID_PASSWORD="${SUB_PWD}"
		SUB_VAR=`expr substr "${SUB_CMD}" $(($SUB_PWD_LEN+1)) $cplen`
		SUB_VAR=`echo ${SUB_VAR} | sed -e "s/$DIAG_MODE//" -e 's/^\ *//'`
	fi
	#sms_log "sub_var = '${SUB_VAR}'"

	# check password matching for 1st sub command
	# when the whitelist index has password
	if [ "$USE_WHITELIST" = "1" ] && [ "$SUB_CMD_IDX" -eq "1" ] && [ "$wldiagpassword" != "" ]; then
		sms_log "checking password [$wldiagpassword, $SUB_PWD]"
		check_password_match $wldiagpassword $SUB_PWD
	fi

	extra_commands=$(exec process_predefined_command.awk $DIAG_MODE "${SUB_VAR}")
	sms_log "[process_predefined_command.awk] result='$extra_commands'"
	if [ "$extra_commands" != "none" ] && [ "$extra_commands" != "" ]; then
		eval $extra_commands
		send_result_and_ack "$extra_mode" "$extra_command" "$extra_reply_sms"
		continue
	else
		# for set command, need to separate var and value
		#----------------------------------------------------------------------
		if [ "$DIAG_MODE" = "get" ]; then
			# process predefined get command
			process_predefined_db_command "$DIAG_MODE" "${SUB_VAR}"
			# process rdb/nv get command
			RESULT="$?"
			test "$RESULT" = "255" && continue
			if [ "$RESULT" = "0" ]; then
				if [ "$platform" != "Platypus" ]; then
					IS_RDB_VAR=`rdb_get -l "${SUB_VAR}"`
					if [ "$IS_RDB_VAR" != "" ]; then
						sms_log "get generic RDB variable '${SUB_VAR}'"
						RESPONSE=`rdb_get "${SUB_VAR}"`
					else
						sms_log "generic RDB variable '${SUB_VAR}' is not exist, send NONEXIST notification"
						send_error_notification "NONEXIST"
						continue
					fi
				else
					IS_RDB_VAR=`rdb_get -l "${SUB_VAR}"`
					if [ "$IS_RDB_VAR" = "" ]; then
						IS_NV_VAR=`ralink_init rt2860_nvram_show | grep "${SUB_VAR}" | grep -v "grep"`
						if [ "$IS_NV_VAR" != "" ]; then
							sms_log "get generic NV variable '${SUB_VAR}'"
							RESPONSE=`nvram_get "${SUB_VAR}"`
						else
							sms_log "generic RDB/NV variable '${SUB_VAR}' is not exist, send NONEXIST notification"
							send_error_notification "NONEXIST"
							continue
						fi
					else
						sms_log "get generic RDB variable '${SUB_VAR}'"
						RESPONSE=`rdb_get "${SUB_VAR}"`
					fi
				fi
			fi
			sms_log "response = ${RESPONSE}"
			# send result to sender
			send_result_and_ack "$DIAG_MODE" "${SUB_VAR}" "${RESPONSE}"

		#----------------------------------------------------------------------
		elif [ "$DIAG_MODE" = "set" ]; then
			eqchk=`echo "${SUB_VAR}" | awk '/=/ {print $0}'`
			if [ "$eqchk" = "" ]; then
				sms_log "can not find '=' in set command line"
				send_error_notification "FORMAT ERROR"
				continue
			fi
			SUB_VAL=`echo "${SUB_VAR}" | awk -F "=" '{if(NF>2) print $2"="$3; else print $2}' | sed -e 's/^\ *//' -e 's/ *$//'`
			SUB_VAR=`echo "${SUB_VAR}" | awk -F "=" '{print $1}' | sed -e 's/^\ *//' -e 's/ *$//'`
			# strip single/double apostrophes, quote and back tick at the beginning and ending,
			# remove leading and trainling spaces
			SUB_VAL=`echo ${SUB_VAL} | sed -e 's/^"//' -e 's/^\ *//' -e 's/ *$//' -e 's/"$//' -e 's/ *$//'`
			SUB_VAL=`echo ${SUB_VAL} | sed -e "s/^'//" -e 's/^\ *//' -e 's/ *$//' -e "s/'$//" -e 's/ *$//'`
			SUB_VAL=`echo ${SUB_VAL} | sed -e 's/^\`//' -e 's/^\ *//' -e 's/ *$//' -e 's/\`$//' -e 's/ *$//'`
			sms_log "sub_var = '${SUB_VAR}', sub_val = '${SUB_VAL}'"
			if [ "$SUB_VAR" = "" ]; then
				sms_log "variable name is null in set command line"
				send_error_notification "FORMAT ERROR"
				continue
			fi

			# prevent turn on/off of SMS via SMS
			if [ "$SUB_VAR" = "smstools.enable" ]; then
				sms_log "$SUB_VAR is read-only variable"
				send_error_notification "READ ONLY"
				continue
			fi

			# process predefined set command
			process_predefined_db_command "$DIAG_MODE" "${SUB_VAR}" "${SUB_VAL}"
			# process rdb set command
			RESULT="$?"
			test "$RESULT" = "255" && continue
			if [ "$RESULT" = "0" ]; then
				if [ "$platform" != "Platypus" ]; then
					IS_RDB_VAR=`rdb_get -l "${SUB_VAR}"`
					if [ "$IS_RDB_VAR" != "" ]; then
						sms_log "set generic RDB variable '${SUB_VAR}' to '${SUB_VAL}'"
						rdb_set -- "${SUB_VAR}" "${SUB_VAL}"
					else
						sms_log "generic RDB variable '${SUB_VAR}' is not exist, send NONEXIST notification"
						send_error_notification "NONEXIST"
						continue
					fi
				else
					IS_RDB_VAR=`rdb_get -l "${SUB_VAR}"`
					if [ "$IS_RDB_VAR" = "" ]; then
						IS_NV_VAR=`ralink_init rt2860_nvram_show | grep "${SUB_VAR}" | grep -v "grep"`
						if [ "$IS_NV_VAR" != "" ]; then
							sms_log "set generic NV variable '${SUB_VAR}' to '${SUB_VAL}'"
							nvram_set "${SUB_VAR}" "${SUB_VAL}"
						else
							sms_log "generic RDB variable '${SUB_VAR}' is not exist, send NONEXIST notification"
							send_error_notification "NONEXIST"
							continue
						fi
					else
						sms_log "set generic RDB variable '${SUB_VAR}' to '${SUB_VAL}'"
						rdb_set -- "${SUB_VAR}" "${SUB_VAL}"
					fi
				fi
			fi
			# send result to sender
			send_result_and_ack "$DIAG_MODE" "${SUB_VAR}" "${SUB_VAL}"

		#----------------------------------------------------------------------
		elif [ "$DIAG_MODE" = "execute" ]; then
			# process predefined execute command
			process_predefined_exe_command "${SUB_VAR}"
			# process normal command
			RESULT="$?"
			test "$RESULT" = "255" && continue
			# for reboot command, send result and return to reboot
			if [ "$RESULT" = "2" ]; then
				break
			fi
			# send command execution result for generic command
			if [ "$RESULT" = "0" ]; then
				sms_log "run \"${SUB_VAR}\" command"
				OUTPUT=`/bin/sh -c "${SUB_VAR}"`
				RESULT="$?"
				sms_log "general command execute result = $RESULT"
				sms_log "        output = $OUTPUT"
				if [ "$RESULT" != "0" ]; then
					send_error_notification "FORMAT ERROR"
					continue
				fi
				# send result to sender
				send_result_and_ack "$DIAG_MODE" "${SUB_VAR}" "${OUTPUT}"
			else
				# send result to sender
				send_result_and_ack "$DIAG_MODE" "${SUB_VAR}"
			fi
		fi
	fi
done

# send multiple reply in one SMS
if [ "${REPLY_SMS}" != "" ]; then
	send_diag_sms_response
fi

# delete diag msg from SIM/ME memory only when this message has diag key word
test "$KEYWORD_FOUND" = "1" && delete_stored_diag_msg

sms_log "$0 done!"
sms_log "========================================================="

exit 0

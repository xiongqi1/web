#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
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

# validate input
. /etc/variant.sh

. $SMS_LIB_PATH/sms_utils

# read nvram/rdb variables and create new cfg files
read_vars_n_create_cfg_files >/dev/null

. $config_file
. $diag_config_file
. $ssmtp_config

atmgr_ready=`rdb_get atmgr.status`

exit_with_unblock()
{
	unblock_sms_rx_processing
	exit 0
}

#-------------------------------------------------------------------------
# Read Inbox/Outbox Messages
#-------------------------------------------------------------------------
calculate_page_num()
{
	if [ "$FILECNT" = "0" ]; then
		TOTALPAGES="1"
		RSPMSGCNT="0"
		echo "MsgCnt=\"$FILECNT\";"
		echo "MsgsPp=\"$MSG_NO_PER_PAGE\";"
		echo "TotalPages=\"$TOTALPAGES\";"
		echo "RespMsgCnt=\"$RSPMSGCNT\";"
		return
	fi

	let "REMAIN=$FILECNT%$MSG_NO_PER_PAGE"
	if [ "$REMAIN" = "" ]; then
		let "REMAIN=0"
	fi
	let "TOTALPAGES=$FILECNT/$MSG_NO_PER_PAGE"
	if [ "$TOTALPAGES" = "" ]; then
		let "TOTALPAGES=1"
	else
		if [ "$REMAIN" -gt "0" ]; then
			let "TOTALPAGES+=1"
		fi
	fi
	if [ "$PAGE_NO" = "" ]; then
		let "PAGE_NO=1"
	fi
	let "STARTIDX=$MSG_NO_PER_PAGE*($PAGE_NO-1)"
	if [ "$STARTIDX" = "" ]; then
		let "STARTIDX=0"
	fi
	if [ "$PAGE_NO" -lt "$TOTALPAGES" ]; then
		let "ENDIDX=$STARTIDX+$MSG_NO_PER_PAGE"
	else
		if [ "$REMAIN" = "0" ]; then
			let "ENDIDX=$STARTIDX+$MSG_NO_PER_PAGE"
		else
			let "ENDIDX=$STARTIDX+$REMAIN"
		fi
	fi
	let "RSPMSGCNT=$ENDIDX-$STARTIDX"

	echo "MsgCnt=\"$FILECNT\";"
	echo "MsgsPp=\"$MSG_NO_PER_PAGE\";"
	echo "TotalPages=\"$TOTALPAGES\";"
	echo "RespMsgCnt=\"$RSPMSGCNT\";"
}
# READ_MSGCNT does not require authentication

if [ "$CMD" = "READ_MSGCNT" ]; then

	#-------------------------------------------------------------------------
	# response header
	#-------------------------------------------------------------------------
	echo -e 'Content-type: text/html\n'

	# block further rx sms processing to prevent cgi error
	block_sms_rx_processing

	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		test ! -e $MSGFILE && read_all_msgs
		if [ "$INOUT" = "OUTBOX" ]; then
			FILECNT=`awk '/^\+CMGR\:/ {print $0}' $MSGFILE | awk 'BEGIN {cnt=0;} /STO SENT/ {cnt++;} END {print cnt}'`
		else
			FILECNT=`awk '/^\+CMGR\:/ {print $0}' $MSGFILE | awk 'BEGIN {cnt=0;} /REC READ|REC UNREAD/ {cnt++;} END {print cnt}'`
		fi
		calculate_page_num
	else
		CURR_DIR=`pwd`
		if [ "$INOUT" = "OUTBOX" ]; then
			cd $LOCAL_OUTBOX
		else
			cd $LOCAL_INBOX
		fi
		FILECNT=`ls |wc -l` 2>/dev/null

		calculate_page_num

		cd $CURR_DIR
	fi
	exit_with_unblock
fi

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo "This shell script is for internal system use only."
	echo "It is used to interface with SMS WEBUI to provide general SMS functionality."
    echo "Please do not run this script manually."
	exit 0
fi

# block further rx sms processing to prevent cgi error
block_sms_rx_processing

# read http req. contents to temp file & include to set variables
cat > /tmp/sms_cgi_temp && cat /tmp/sms_cgi_temp | sed -r 's/&/\n/g' > /tmp/sms_cgi_temp2 && mv /tmp/sms_cgi_temp2 /tmp/sms_cgi_temp
. /tmp/sms_cgi_temp

# splits CGI query into var="value" strings
cgi_split() {
	echo "$1" | awk 'BEGIN{
		hex["0"] =  0; hex["1"] =  1; hex["2"] =  2; hex["3"] =  3;
		hex["4"] =  4; hex["5"] =  5; hex["6"] =  6; hex["7"] =  7;
		hex["8"] =  8; hex["9"] =  9; hex["A"] = 10; hex["B"] = 11;
		hex["C"] = 12; hex["D"] = 13; hex["E"] = 14; hex["F"] = 15;
		                              hex["a"] = 10; hex["b"] = 11;
		hex["c"] = 12; hex["d"] = 13; hex["e"] = 14; hex["f"] = 15;

	}
	{
		n=split ($0,EnvString,"&");
		for (i = n; i>0; i--) {
			z = EnvString[i];
			x=gsub(/\=/,"=\"",z);
			while(match(z, /%../)){
				if(RSTART > 1)
					printf "%s", substr(z, 1, RSTART-1)
				printf "%c", hex[substr(z, RSTART+1, 1)] * 16 + hex[substr(z, RSTART+2, 1)]
				z = substr(z, RSTART+RLENGTH)
			}
			x=gsub(/$/,"\"",z);
			print z;
		}
	}'
}

filtered_query_string=$(echo "$QUERY_STRING" | sed 's/&[[:blank:]]*$//g')
qlist=`cgi_split "$filtered_query_string"`

split() {
	shift $1
	echo "$1"
}

# Get the device node and name (but don't get any other params yet!)
while read V; do
	VAR="$V"
	SEP=`echo "$VAR" | tr '=' ' '`
	NAME=`split 1 $SEP`
	VAL=`split 2 $SEP`

	if [ "$NAME" = "RedirMobile" ]; then
		eval $VAR
	fi
done << EOF
$qlist
EOF


#sms_log "TxMsg = '$TxMsg'"

cgi_decode() {
	echo "$1" | awk 'BEGIN{
		hex["0"] =  0; hex["1"] =  1; hex["2"] =  2; hex["3"] =  3;
		hex["4"] =  4; hex["5"] =  5; hex["6"] =  6; hex["7"] =  7;
		hex["8"] =  8; hex["9"] =  9; hex["A"] = 10; hex["B"] = 11;
		hex["C"] = 12; hex["D"] = 13; hex["E"] = 14; hex["F"] = 15;
	}
	{
		z = $0;
		while(match(z, /%../)){
			if(RSTART > 1)
				printf "%s", substr(z, 1, RSTART-1)
			first=hex[substr(z, RSTART+1, 1)]
			second=hex[substr(z, RSTART+2, 1)]
			if (first >= 0 && first <= 15 && second >= 0 && second <= 15) {
				printf "%c", first * 16 + second
			} else {
				printf "%s", substr(z, RSTART, RLENGTH)
			}
			z = substr(z, RSTART+RLENGTH)
		}
		print z;
	}'
}

TxMsg=`cgi_decode "$TxMsg"`
#sms_log "decoded TxMsg = '$TxMsg'"

#-------------------------------------------------------------------------
# parse command
#-------------------------------------------------------------------------
echo -e 'Content-type: text/html\n'


update_memory_status()
{
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		SIM_ST=`rdb_get wwan.0.sim.status.status`
		if [ "$SIM_ST" != "SIM OK" ]; then
			ST1="${SIM_ST}"
			ST2="${SIM_ST}"
		else
			let "retry_cnt=10"
			while [ $retry_cnt -gt 0 ]; do
				test "$1" = "readall" && read_all_msgs >/dev/null
				ST1=`rdb_get wwan.0.sms.storage.status`       # used:6 total:10
				ST2=`rdb_get wwan.0.sms.message.status`       # Total:6 read:5 unread:0 sent:1 unsent:0
				# read all msgs to update memory/message status
				if [ "$ST1" = "" ] || [ "$ST2" != "" ]; then
					read_all_msgs "FORCE">/dev/null
					ST1=`rdb_get wwan.0.sms.storage.status`       # used:6 total:10
					ST2=`rdb_get wwan.0.sms.message.status`       # Total:6 read:5 unread:0 sent:1 unsent:0
				fi
				FAKE_NOTI=`echo "$ST2" | sed -n '/Total:xxx/p'`
				test -z "$FAKE_NOTI" && break
				sms_log "Found fake notification for incoming sms template, wait 1 second and check storage status again."
				sleep 1
		let "retry_cnt-=1"
			done
		fi
	fi
	echo "MemStat=\"$ST1\";"
	echo "MsgStat=\"$ST2\";"
}

#-------------------------------------------------------------------------
# Get SMS Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "SMS_CONF_GET" ]; then
	echo "RedirMobile=\"$REDIRECT_MOB\";"
	echo "RedirEmail=\"$REDIRECT_EMAIL\";"
	echo "RedirTCP=\"$REDIRECT_TCP\";"
	echo "TCPport=\"$REDIRECT_TCP_PORT\";"
	echo "RedirUDP=\"$REDIRECT_UDP\";"
	echo "UDPport=\"$REDIRECT_UDP_PORT\";"
	echo "EncodingScheme=\"$CODING_SCHEME\";"
	echo "MoService=\"$MO_SERVICE\";"
	echo "RemoteCommand=\"$ENABLE_REMOTE_CMD\";"
	echo "MsgsPerPage=\"$MSG_NO_PER_PAGE\";"
	echo "UseExtSmsClient=\"$USE_EXT_SMS_CLIENT\";"
	echo "ExtSmsClientIp1=\"$EXT_SMS_CLIENT_IP1\";"
	echo "ExtSmsClientIp2=\"$EXT_SMS_CLIENT_IP2\";"
	echo "ExtSmsClientPort=\"$EXT_SMS_CLIENT_PORT\";"
	# read all messages in SIM/ME memory to update memory status
	update_memory_status "readall"
	exit_with_unblock
fi


#-------------------------------------------------------------------------
# Set SMS Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "SMS_CONF_SET" ]; then
	pre_batch_nv_write
	# save changed variables only
	test "${REDIRECT_MOB}" != "${RedirMobile}" && write_variable "smstools.conf.redirect_mob" "${RedirMobile}" $nvflag
	test "${REDIRECT_EMAIL}" != "${RedirEmail}" && write_variable "smstools.conf.redirect_email" "${RedirEmail}" $nvflag
	test "${REDIRECT_TCP}" != "$RedirTCP" && write_variable "smstools.conf.redirect_tcp" "$RedirTCP" $nvflag
	test "${REDIRECT_TCP_PORT}" != "$TCPport" && write_variable "smstools.conf.redirect_tcp_port" "$TCPport" $nvflag
	test "$REDIRECT_UDP" != "$RedirUDP" && write_variable "smstools.conf.redirect_udp" "$RedirUDP" $nvflag
	test "$REDIRECT_UDP_PORT" != "$UDPport" && write_variable "smstools.conf.redirect_udp_port" "$UDPport" $nvflag
	test "$CODING_SCHEME" != "$EncodingScheme" && write_variable "smstools.conf.coding_scheme" "$EncodingScheme" $nvflag
	test "$MO_SERVICE" != "$MoService" && write_variable "smstools.conf.mo_service" "$MoService" $nvflag
	test "$ENABLE_REMOTE_CMD" != "$RemoteCommand" && write_variable "smstools.conf.enable_remote_cmd" "$RemoteCommand" $nvflag
	test "$MSG_NO_PER_PAGE" != "$MsgsPerPage" && write_variable "smstools.conf.msg_no_per_page" "$MsgsPerPage" $nvflag
	test "$USE_EXT_SMS_CLIENT" != "$UseExtSmsClient" && CHGD="1" && write_variable "smstools.conf.use_ext_client" "$UseExtSmsClient" $nvflag
	test "$EXT_SMS_CLIENT_IP1" != "$ExtSmsClientIp1" && CHGD="1" && write_variable "smstools.conf.ext_client_ip1" "$ExtSmsClientIp1" $nvflag
	test "$EXT_SMS_CLIENT_IP2" != "$ExtSmsClientIp2" && CHGD="1" && write_variable "smstools.conf.ext_client_ip2" "$ExtSmsClientIp2" $nvflag
	test "$EXT_SMS_CLIENT_PORT" != "$ExtSmsClientPort" && CHGD="1" && write_variable "smstools.conf.ext_client_port" "$ExtSmsClientPort" $nvflag
	post_batch_nv_write

	# recreate sms.cfg file from nvram/rdb variable
	create_sms_cfg_file

	test "$CHGD" = "1" && restart_sms_server

	exit_with_unblock
fi


#-------------------------------------------------------------------------
# Turn On/Off SMS functions
#-------------------------------------------------------------------------
if [ "$CMD" = "SMS_ONOFF" ]; then
	if [ "$platform" = "Platypus" ]; then
		rdb_set "smstools.enable" "$OnOff"
	fi
	write_variable "smstools.enable" "$OnOff"
	if [ "$OnOff" = "1" ]; then
		# wait 10 seconds for simple_at_manager launching
		# In some case, PDP context activation never successes with no module system reboot. Need more time.
		sleep 25
	fi
	echo "OnOff=$OnOff;"
	exit_with_unblock
fi


#-------------------------------------------------------------------------
# clear all messages in SIM/ME memory
#-------------------------------------------------------------------------
if [ "$CMD" = "SMS_SIM_RESET" ]; then
	delete_all_msgs >/dev/null
	update_memory_status
	exit_with_unblock
fi


#-------------------------------------------------------------------------
# Get Max Tx Destination Index
#-------------------------------------------------------------------------
if [ "$CMD" = "GET_MAX_TX_IDX" ]; then
	read_variable "smstools.max_dst_no_idx" "9" && MAX_TX_IDX=${RD_VAL}
	if [ "$MAX_TX_IDX" = "" ]; then
		MAX_TX_IDX="9"
	fi
	echo "MaxTxDstIdx=\"$MAX_TX_IDX\";"

	# read all messages in SIM/ME memory to update memory status
	update_memory_status "readall"
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Set Max Tx Destination Index
#-------------------------------------------------------------------------
if [ "$CMD" = "SET_MAX_TX_IDX" ]; then
	write_variable "smstools.max_dst_no_idx" "$new_idx"
	echo "MaxTxDstIdx=\"$new_idx\";"
	exit_with_unblock
fi


#-------------------------------------------------------------------------
# Send Message
#-------------------------------------------------------------------------
if [ "$CMD" = "SEND_MSG" ]; then
	read_variable "smstools.max_dst_no_idx" "9" && MAX_TX_IDX=${RD_VAL}
	if [ "$MAX_TX_IDX" = "" ]; then
		MAX_TX_IDX="9"
	fi
	let "i=0"
	while [ "$i" -lt "100" ]; do
		eval idx=\${"MobileNo"$i}
		if [ "$idx" = "" ]; then
			echo "tx_result[$i]=\"\";"
			let "i+=1"
			continue
		fi
		$SMS_BIN_PATH/sendsms $idx "${TxMsg}"
		SEND_RESULT=`rdb_get wwan.0.sms.cmd.send.status`
		if [ "${SEND_RESULT}" = "[done] send" ]; then
			echo "tx_result[$i]=\"Success\";"
		else
			echo "tx_result[$i]=\"Failure\";"
		fi
		let "i+=1"
	done

	# read all messages in SIM/ME memory to update memory status
	update_memory_status

	# call sms_handler.rx again to process latest rx msg
	#sleep 1 && process_latest_rx_msg&

	exit_with_unblock
fi





# process special characters '\' & '"' that can not
# pass through ajax response
encode_url()
{
	TMPMSG=""
	let "i=1"
	len=${#MSGBODY}
	#sms_log "orig str = $MSGBODY, len = $len"
	while true; do
		c=`expr substr "${MSGBODY}" $i 1`
		#sms_log "i = $i, c = $c"
		if [ "$c" = "\\" ] || [ "$c" = "\"" ]; then
			TMPMSG=${TMPMSG}"\\$c"
		else
			TMPMSG=${TMPMSG}"$c"
		fi
		#sms_log "==> $TMPMSG"
		let "i+=1"
		test "$i" -gt "$len" && break
	done
	#sms_log "conv str = $TMPMSG"
	MSGBODY="${TMPMSG}"
}

read_smsinbox()
{
	CURR_DIR=`pwd`
	cd $LOCAL_INBOX
	FILECNT=`ls | awk 'END {print NR}'`
	#FILECNT=`ls |wc -l`
	#echo "MsgCnt=\"$FILECNT\";"
	#if [ "$USE_SIM_STORAGE" = "YES" ]; then
		FILELIST=`ls | sort -r` 2>/dev/null
	#else
	#   FILELIST=`ls -c` 2>/dev/null
	#fi

	calculate_page_num

	let "IDX=0"
	let "MSGIDX=1"
	for FILENAME in $FILELIST; do
		if [ "$MSGIDX" -le "$STARTIDX" ]; then
			let "MSGIDX+=1"
			continue
		fi

		TEMPSTR=`awk '/^From:/ {print $2; exit;}' $FILENAME`
		echo "MobNum[$IDX]=\"$TEMPSTR\";"

		TEMPSTR=`awk '/^From_SMSC:/ {print $2; exit;}' $FILENAME`
		echo "SMSCNum[$IDX]=\"$TEMPSTR\";"

		TEMPSTR=`awk '/^Sent:/ {print $2,$3,$4,$5,$7; exit;}' $FILENAME`
		echo "TxTime[$IDX]=\"$TEMPSTR\";"

		TEMPSTR=`awk '/^Received:/ {print $2,$3,$4,$5,$7; exit;}' $FILENAME`
		echo "RxTime[$IDX]=\"$TEMPSTR\";"

		dos2unix $FILENAME
		awk '{if (NR>7) print $0}' $FILENAME > /tmp/smstmp && grep . /tmp/smstmp | awk '{printf "%s\\n",$0}!//{print}' > /tmp/smstmp2
		cat /tmp/smstmp2 | sed -r 's/\\n*$//g' > /tmp/smstmp
		MSGBODY=$(base64 /tmp/smstmp2 2>/dev/null | tr -d '\n')

		echo "MsgBody[$IDX]=\"${MSGBODY}\";"

		echo "FileName[$IDX]=\"$FILENAME\";"

		let "MSGIDX+=1"
		let "IDX+=1"
		if [ "$MSGIDX" -gt "$ENDIDX" ]; then
			break;
		fi
	done
	cd $CURR_DIR

	# read all messages in SIM/ME memory to update memory status
	update_memory_status
}

# concatenate splitted msg files
concatenate_msgs()
{
	if [ "$1" = "OUTBOX" ]; then
		TGT_DIR=$LOCAL_OUTBOX
	else
		TGT_DIR=$LOCAL_INBOX
	fi

	CURR_DIR=`pwd`
	cd $TGT_DIR
	#if [ "$USE_SIM_STORAGE" = "YES" ]; then
		FILELIST=`ls 2>/dev/null | sort`
	#else
	#   FILELIST=`ls -cr 2>/dev/null`
	#fi

	CONCAT_FILE=""
	SIM_INDEXES=""
	for FILENAME in $FILELIST; do
		if [ "$1" = "OUTBOX" ]; then
			FIND_CONCAT=`awk '{if (NR==3) print $0}' $FILENAME | grep -- "^CONCAT-" | grep -v grep 2>/dev/null`
		else
			FIND_CONCAT=`awk '{if (NR==9) print $0}' $FILENAME | grep -- "^CONCAT-" | grep -v grep 2>/dev/null`
		fi
		test -z "$FIND_CONCAT" && continue

		SIM_IDX=`echo $FILENAME | awk -F "_" '{print $3}' 2>/dev/null`
		if [ -z "$SIM_INDEXES" ]; then
			SIM_INDEXES="$SIM_IDX"
		else
			SIM_INDEXES="$SIM_INDEXES-$SIM_IDX"
		fi
		# first concatenate file needs header as well
		if [ -z "$CONCAT_FILE" ]; then
			CONCAT_FILE=$FILENAME
			cat $FILENAME | sed -e 's/CONCAT-MID://' -e 's/CONCAT-LAST://' > $FILENAME.new 2>/dev/null
			echo "" > $FILENAME
			cp /dev/null $FILENAME
			line_count=`awk 'END {print NR}' $FILENAME.new`
			let "line_idx=0"
			while read line
			do
				let "line_idx+=1"
				# eliminate trailing carridge return for last line
				if [ $line_count -eq $line_idx ]; then
					echo -n "$line" >> $FILENAME
				else
					echo "$line" >> $FILENAME
				fi
			done <$FILENAME.new
			rm $FILENAME.new 2>/dev/null
			continue
		fi

		CONCAT_LAST=`echo $FIND_CONCAT | grep "CONCAT-LAST:" | grep -v grep`
		# copy msg body to concatenating file
		if [ "$1" = "OUTBOX" ]; then
			awk '{if (NR>2) print $0}' $FILENAME | sed -e 's/CONCAT-MID://' -e 's/CONCAT-LAST://' -e 's/GSM7://' -e 's/8BIT://' -e 's/UCS2://'> $FILENAME.new 2>/dev/null
		else
			awk '{if (NR>8) print $0}' $FILENAME | sed -e 's/CONCAT-MID://' -e 's/CONCAT-LAST://' -e 's/GSM7://' -e 's/8BIT://' -e 's/UCS2://'> $FILENAME.new 2>/dev/null
		fi
		line_count=`awk 'END {print NR}' $FILENAME.new`
		let "line_idx=0"
		while read line
		do
			let "line_idx+=1"
			# eliminate trailing carridge return for last line
			if [ $line_count -eq $line_idx ]; then
				echo -n "$line" >> $CONCAT_FILE
			else
				echo "$line" >> $CONCAT_FILE
			fi
		done <$FILENAME.new
		rm $FILENAME.new 2>/dev/null

		if [ -n "$CONCAT_LAST" ]; then
			# rename concatenated file
			NEW_F_NAME=`echo $CONCAT_FILE | awk -F "_" '{print $1"_"$2"_"}'`$SIM_INDEXES
			echo "" >> $CONCAT_FILE
			mv $CONCAT_FILE $NEW_F_NAME 2>/dev/null
			CONCAT_FILE=""
			SIM_INDEXES=""
		fi
		rm $FILENAME 2>/dev/null
	done
	cd $CURR_DIR
}

create_sms_msg_files()
{
	if [ "$1" = "OUTBOX" ]; then
		TGT_DIR=$LOCAL_OUTBOX
	else
		TGT_DIR=$LOCAL_INBOX
	fi
	rm -f $TGT_DIR/*

	line_count=`cat $MSGFILE | awk 'END {print NR}'`
	while read line
	do
		if [ "$line" = "" ]; then
			continue
		fi

		# make filename
		if [ "$1" = "OUTBOX" ]; then
			F_IDX=`echo "$line" | awk '/^\+CMGR\:/ {print $0}' | awk '/STO SENT/ {print}'`
			TEMPSTR=`echo "$line" | awk '/^\+CMGR\:/ {print $0}' | awk '/REC READ|REC UNREAD/ {print}'`
			test "$TEMPSTR" != "" && SKIP="1"
		else
			F_IDX=`echo "$line" | awk '/^\+CMGR\:/ {print $0}' | awk -F "," '/REC READ|REC UNREAD/ {print $5$6}' | awk '{gsub(/:/, "");gsub(/"/, "");gsub(/\//, "");gsub(/\+/, "");print}'`
			TEMPSTR=`echo "$line" | awk '/^\+CMGR\:/ {print $0}' | awk '/STO SENT/ {print}'`
			test "$TEMPSTR" != "" && SKIP="1"
		fi

		if [ "$F_IDX" != "" ]; then
			# create msg file
			S_IDX=`echo "$line" | awk -F "," '{print $1}' | awk '{gsub(/\+CMGR: /, "");print}'`
			if [ "$1" = "OUTBOX" ]; then
				read_variable "sms_txrx_time_$S_IDX" && TEMPSTR=${RD_VAL}
				F_IDX=`echo $TEMPSTR | awk '{gsub(/ /, "");gsub(/-/, "");gsub(/:/, "");print}'`
			fi
			F_NAME=`echo "$TGT_DIR/sms_"$F_IDX"_"$S_IDX`
			LAST_F_IDX=$F_IDX
			touch $F_NAME
			TEMPSTR=`echo "$line" | awk -F "," '{gsub(/"/,"");print $3}'`
			if [ "$1" = "OUTBOX" ]; then
				echo "To:  $TEMPSTR" > $F_NAME
				echo "" >> $F_NAME
			else
				echo "From: $TEMPSTR" > $F_NAME
				echo "From_SMSC:" >> $F_NAME
				DATESTR=`echo "$line" | awk -F "," '{gsub(/"/,"");gsub(/\+/, "");gsub(/\//, "-");printf "%s\n",$5}'`
				TIMESTR=`echo "$line" | awk -F "," '{gsub(/"/,"");gsub(/\+/, "");printf "%.8s\n",$6}'`
				test `echo $DATESTR | grep ^[0-9][0-9][^0-9]` && DATESTR="20"$DATESTR
				echo "Sent: $DATESTR $TIMESTR" >> $F_NAME
				read_variable "sms_txrx_time_$S_IDX" && TEMPSTR=${RD_VAL}
				echo "Received: $TEMPSTR" >> $F_NAME
				echo "Subject: GSM1" >>  $F_NAME
				echo "Alphabet:  ISO" >>  $F_NAME
				echo "UDH: false" >>  $F_NAME
				echo "" >> $F_NAME
			fi
			SKIP="0"
		else
			# copy msg body
			test "$SKIP" = "0" && echo "$line" >> $F_NAME
		fi
	done <$MSGFILE

	# concatenate splitted msg files
	concatenate_msgs "$1"
}

read_smsoutbox()
{
	CURR_DIR=`pwd`
	cd $LOCAL_OUTBOX
	FILECNT=`ls | awk 'END {print NR}'`
	#FILECNT=`ls |wc -l`
	#echo "MsgCnt=\"$FILECNT\";"
	#if [ "$USE_SIM_STORAGE" = "YES" ]; then
		FILELIST=`ls | sort -r` 2>/dev/null
	#else
	#   FILELIST=`ls -c` 2>/dev/null
	#fi

	calculate_page_num

	let "IDX=0"
	let "MSGIDX=1"
	for FILENAME in $FILELIST; do
		if [ "$MSGIDX" -le "$STARTIDX" ]; then
			let "MSGIDX+=1"
			continue
		fi

		DESTNO=`awk '/To:/ {print $2}' $FILENAME`
		echo "MobNum[$IDX]=\"$DESTNO\";"

		if [ "$USE_SIM_STORAGE" = "YES" ]; then
			S_IDX=`echo $FILENAME | awk '{ split($0, a , "_");print a[3] }' | awk -F "-" '{print $1}'`
			read_variable "sms_txrx_time_$S_IDX" && TEMPSTR=${RD_VAL}
			DATETIME=$TEMPSTR
		else
			DATETIME=`ls -le $FILENAME | awk '{ print $6,$7,$8,$9,$10 }'`
		fi

		echo "TxTime[$IDX]=\"$DATETIME\";"

		awk '{if (NR>2) print $0}' $FILENAME > /tmp/smstmp && grep . /tmp/smstmp | awk '{printf "%s\\n",$0}!//{print}' > /tmp/smstmp2
		cat /tmp/smstmp2 | sed -r 's/\\n*$//g' > /tmp/smstmp
		MSGBODY=$(base64 /tmp/smstmp2 2>/dev/null | tr -d '\n')


		echo "MsgBody[$IDX]=\"${MSGBODY}\";"

		echo "FileName[$IDX]=\"$FILENAME\";"

		let "MSGIDX+=1"
		let "IDX+=1"
		if [ "$MSGIDX" -gt "$ENDIDX" ]; then
			break;
		fi
	done
	cd $CURR_DIR

	# read all messages in SIM/ME memory to update memory status
	update_memory_status
}

if [ "$CMD" = "READ_SMSBOX" ]; then
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		read_all_msgs >/dev/null
		create_sms_msg_files "$INOUT"
	fi
	if [ "$INOUT" = "OUTBOX" ]; then
		read_smsoutbox
	else
		read_smsinbox
	fi
	exit_with_unblock
fi



#-------------------------------------------------------------------------
# Delete Outbox/Inbox Messages
#-------------------------------------------------------------------------
if [ "$CMD" = "DELETE_MSG" ]; then
	CURR_DIR=`pwd`
	if [ "$INOUT" = "OUTBOX" ]; then
		working_dir=${LOCAL_OUTBOX}
	else
		working_dir=${LOCAL_INBOX}
	fi
	cd "${working_dir}"

	# some models which use simple_at_manager can delete multiple message at once
	DEL_LIST=""
	for tgt in ${fnlist}; do
		# delete a sms message from SIM/ME memory
		if [ "$USE_SIM_STORAGE" = "YES" ]; then
			S_IDX=`echo $tgt | awk '{ split($0, a , "_");print a[3] }'`
			if [ "$platform" = "Platypus" ] || [ "$platform" = "Platypus2" ] || [ "$platform" = "Avian" ] || [ "$atmgr_ready" = "ready" ]; then
				DEL_LIST=$DEL_LIST" "$S_IDX
				#sms_log "delete file list '$DEL_LIST'"
			else
				delete_one_msg $S_IDX >/dev/null
			fi
		fi
		# check file name, delete only if file name is valid
		# valid sms file name format : rxmsg_xxxxx_read|unread, txmsg_xxxxx
		FILTERED_NAME=$(basename "$tgt" | grep '^[r|t]xmsg_[0-9]\{5\}.*$')
		if [ -n "$FILTERED_NAME" -a -f "$FILTERED_NAME" ]; then
			rm "$FILTERED_NAME"
			sms_log "delete message file '$FILTERED_NAME'"
		else
			sms_log "ignore invalid file name '$tgt'"
		fi
	done
	if [ "$DEL_LIST" != "" ]; then
		sms_log "delete message idx '$DEL_LIST'"
		delete_one_msg "$DEL_LIST" >/dev/null
	fi

	# sms msg file name does not have any meaning for the model using SIM/ME memory
	# because it always rebuild inbox/outbox contents from SIM/ME memory when read inbox/outbox
	# but for other models we need to give some index to sort out.
	if [ "$USE_SIM_STORAGE" != "YES" ]; then
		rebuild_msg_box_index "${working_dir}"
	fi

	FILECNT=`ls | awk 'END {print NR}'`

	calculate_page_num

	cd $CURR_DIR
	exit_with_unblock
fi



#-------------------------------------------------------------------------
# Get SSMTP Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "SSMTP_CONF_GET" ]; then
	echo "SsmtpMailHub=\"$mailhub\";"
	echo "SsmtpHostName=\"$hostname\";"
	echo "SsmtpAuthUser=\"$AuthUser\";"
	echo "SsmtpAuthPass=\"$AuthPass\";"
	echo "SsmtpFromSender=\"$FromSender\";"
	exit_with_unblock
fi



#-------------------------------------------------------------------------
# Set SSMTP Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "SSMTP_CONF_SET" ]; then
	pre_batch_nv_write
	# save changed variables only
	test "$mailhub" != "${SsmtpMailHub}" && write_variable "smstools.ssmtpconf.mailhub" "${SsmtpMailHub}" $nvflag
	test "$hostname" != "${SsmtpHostName}" && write_variable "smstools.ssmtpconf.hostname" "${SsmtpHostName}" $nvflag
	test "$AuthUser" != "${SsmtpAuthUser}" && write_variable "smstools.ssmtpconf.AuthUser" "${SsmtpAuthUser}" $nvflag
	test "$AuthPass" != "${SsmtpAuthPass}" && write_variable "smstools.ssmtpconf.AuthPass" "${SsmtpAuthPass}" $nvflag
	test "$FromSender" != "${SsmtpFromSender}" && write_variable "smstools.ssmtpconf.FromSender" "${SsmtpFromSender}" $nvflag
	post_batch_nv_write

	# recreate ssmtp.cfg file from nvram/rdb variable
	create_ssmtp_cfg_file
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Get SMS Diag Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "DIAG_CONF_GET" ]; then
	echo "UseWhiteList=\"$USE_WHITELIST\";"
	echo "EnableSetCmdAck=\"$ENABLE_SET_CMD_ACK\";"
	echo "UseFixedAckDest=\"$USE_FIXED_ACK_DEST\";"
	echo "FixedAckDestNo=\"$FIXED_ACK_DEST_NO\";"
	echo "EnableErrorNoti=\"$ENABLE_ERROR_NOTI\";"
	echo "UseFixedErrorNotiDest=\"$USE_FIXED_ERROR_NOTI_DEST\";"
	echo "FixedErrorNotiDestNo=\"$FIXED_ERROR_NOTI_DEST_NO\";"
	echo "MaxDiagSmsTxLimit=\"$MAX_DIAG_SMS_TX_LIMIT\";"
	echo "MaxDiagSmsTxLimitPer=\"$MAX_DIAG_SMS_TX_LIMIT_PER\";"
	echo "AccessGenericRdbVars=\"$ACCESS_GENERIC_RDB_VARS\";"
	echo "AllowGenericCmds=\"$ALLOW_GENERIC_CMDS\";"
	read_variable "smstools.diagsms_txcnt" && SMS_TX_CNT=${RD_VAL}
	echo "SmsTxCnt=\"$SMS_TX_CNT\";"
	read_variable "smstools.max_wl_no_idx" && MAX_WL_TX_IDX=${RD_VAL}
	if [ "$MAX_WL_TX_IDX" = "" ]; then
		MAX_WL_TX_IDX="0"
	fi
	echo "MaxWlTxDstIdx=\"$MAX_WL_TX_IDX\";"

	let "i=0"
	while [ "$i" -le "$MAX_WL_TX_IDX" ]; do
		eval tmpdiaguserno=\${"DIAG_USER_NO"$i}
		echo "DiagUserNo[$i]=\"$tmpdiaguserno\";"
		eval tmpdiagpassword=\${"DIAG_PASSWORD"$i}
		echo "DiagPassword[$i]=\"$tmpdiagpassword\";"
		let "i+=1"
	done

	exit_with_unblock
fi



#-------------------------------------------------------------------------
# Set Diag SMS Configuration
#-------------------------------------------------------------------------
if [ "$CMD" = "DIAG_CONF_SET" ]; then

	if [ "$V_SKIN" = "VDF" -o "$V_SKIN" = "VDF2" ]; then
		# MR3 Security Requirement 14.01:
		# 1. The embedded GDSP numbers should be possible to have/not to have PWD protection
		# 2. Any MO number that is whitelisted SHOULD have PWD protection. This should be accompanied
		# by a warning message that warns users to the necessity of entering PWD before the option is saved
		#
		# This should have already enforced by web page's javascript.
		# If some non-GDSP numbers are submitted without passwords at this point, someones must be doing nasty things.
		# In this case just exit.
		local max_index=$(rdb get smstools.max_wl_no_idx)
		test -z "$max_index" && max_index=0

		local i=0
		while [ "$i" -le "$max_index" ]; do
			eval local diag_number=\${"DiagUserNo"$i}
			eval local diag_password=\${"DiagPassword"$i}

			local need_password="1"

			case "$diag_number" in
				310000214|310000202|8823993560000|8823903560000)
					# It is not required to specify password with GDSP numbers
					need_password="0"
					;;
				*)
					need_password="1"
					;;
			esac

			if [ "$need_password" = "1" -a -n "$diag_number" -a -z "$diag_password" ]; then
				sms_log "Error: A non-GDSP diagnostic number is submitted without password. The request is dropped."
				exit_with_unblock
			fi

			i=$((i+1))
		done
	fi

	pre_batch_nv_write
	# save changed variables only
	test "$USE_WHITELIST" != "${UseWhiteList}" && write_variable "smstools.diagconf.use_whitelist" "${UseWhiteList}" $nvflag
	test "$ENABLE_SET_CMD_ACK" != "${EnableSetCmdAck}" && write_variable "smstools.diagconf.enable_set_cmd_ack" "${EnableSetCmdAck}" $nvflag
	test "$USE_FIXED_ACK_DEST" != "${UseFixedAckDest}" && write_variable "smstools.diagconf.use_fixed_ack_dest" "${UseFixedAckDest}" $nvflag
	test "$FIXED_ACK_DEST_NO" != "${FixedAckDestNo}" && write_variable "smstools.diagconf.fixed_ack_dest_no" "${FixedAckDestNo}" $nvflag
	test "$ENABLE_ERROR_NOTI" != "${EnableErrorNoti}" && write_variable "smstools.diagconf.enable_error_noti" "${EnableErrorNoti}" $nvflag
	test "$USE_FIXED_ERROR_NOTI_DEST" != "${UseFixedErrorNotiDest}" && write_variable "smstools.diagconf.use_fixed_error_noti_dest" "${UseFixedErrorNotiDest}" $nvflag
	test "$FIXED_ERROR_NOTI_DEST_NO" != "${FixedErrorNotiDestNo}" && write_variable "smstools.diagconf.fixed_error_noti_dest_no" "${FixedErrorNotiDestNo}" $nvflag
	test "$MAX_DIAG_SMS_TX_LIMIT" != "${MaxDiagSmsTxLimit}" && write_variable "smstools.diagconf.max_diag_sms_tx_limit" "${MaxDiagSmsTxLimit}" $nvflag
	test "$MAX_DIAG_SMS_TX_LIMIT_PER" != "${MaxDiagSmsTxLimitPer}" && write_variable "smstools.diagconf.max_diag_sms_tx_limit_per" "${MaxDiagSmsTxLimitPer}" $nvflag
	test "$ACCESS_GENERIC_RDB_VARS" != "${AccessGenericRdbVars}" && write_variable "smstools.diagconf.access_generic_rdb_vars" "${AccessGenericRdbVars}" $nvflag
	test "$ALLOW_GENERIC_CMDS" != "${AllowGenericCmds}" && write_variable "smstools.diagconf.allow_generic_cmds" "${AllowGenericCmds}" $nvflag

	read_variable "smstools.max_wl_no_idx" && MAX_WL_TX_IDX=${RD_VAL}
	if [ "$MAX_WL_TX_IDX" = "" ]; then
		sms_log "reset MAX_WL_TX_IDX to 0"
		MAX_WL_TX_IDX="0"
	fi

	let "i=0"
if [ "$V_PRODUCT" = "ntc_220" ]; then
    dummyPW="**********"
	while [ "$i" -le "$MAX_WL_TX_IDX" ]; do
		eval olddiaguserno=\${"DIAG_USER_NO"$i}
		eval newdiaguserno=\${"DiagUserNo"$i}
		test "${olddiaguserno}" != "${newdiaguserno}" && write_variable "smstools.diagconf.diag_user_no$i" "${newdiaguserno}" $nvflag
		eval olddiagpassword=\${"DIAG_PASSWORD"$i}
		eval pass64=\${"DiagPassword"$i}
		if [ "$pass64" = "$dummyPW" ]; then
            newdiagpassword="$dummyPW"
		else
            newdiagpassword=$(openssl passwd -1 $pass64 2>/dev/null | base64)
		fi
		test "${olddiagpassword}" != "${newdiagpassword}" -a "${newdiagpassword}" != "$dummyPW" && write_variable "smstools.diagconf.diag_password$i" "${newdiagpassword}" $nvflag
		let "i+=1"
	done
else
	while [ "$i" -le "$MAX_WL_TX_IDX" ]; do
		eval olddiaguserno=\${"DIAG_USER_NO"$i}
		eval newdiaguserno=\${"DiagUserNo"$i}
		test "${olddiaguserno}" != "${newdiaguserno}" && write_variable "smstools.diagconf.diag_user_no$i" "${newdiaguserno}" $nvflag
		eval olddiagpassword=\${"DIAG_PASSWORD"$i}
        eval encodediagpass=\${"DiagPassword"$i}
        newdiagpassword=$(base64 -d "$encodediagpass")
        echo $encodediagpass >/tmp/smstmp2
        newdiagpassword=$(base64 -d /tmp/smstmp2 2>/dev/null)
        test "${olddiagpassword}" != "${newdiagpassword}" && write_variable "smstools.diagconf.diag_password$i" "${newdiagpassword}" $nvflag
		let "i+=1"
	done
fi

	post_batch_nv_write

	# recreate sms_diag.cfg file from nvram/rdb variable
	create_sms_diag_cfg_file
	exit_with_unblock
fi




#-------------------------------------------------------------------------
# Get Max Tx Destination Index
#-------------------------------------------------------------------------
if [ "$CMD" = "GET_MAX_WL_TX_IDX" ]; then
	read_variable "smstools.max_wl_no_idx" && MAX_WL_TX_IDX=${RD_VAL}
	if [ "$MAX_WL_TX_IDX" = "" ]; then
		MAX_TX_IDX="0"
	fi
	echo "MaxWlTxDstIdx=\"$MAX_WL_TX_IDX\";"
	exit_with_unblock
fi




#-------------------------------------------------------------------------
# Set Max Whitelist Tx Destination Index
#-------------------------------------------------------------------------
if [ "$CMD" = "SET_MAX_WL_TX_IDX" ]; then
	write_variable "smstools.max_wl_no_idx" "$new_idx"
	echo "MaxWlTxDstIdx=\"$new_idx\";"
	exit_with_unblock
fi




#-------------------------------------------------------------------------
# Add mobile numbers from Inbox/Outbox to While List
#-------------------------------------------------------------------------
if [ "$CMD" = "DIAG_ADD_WL" ]; then
	read_variable "smstools.max_wl_no_idx" && MAX_WL_TX_IDX=${RD_VAL}
	if [ "$MAX_WL_TX_IDX" = "" ]; then
		MAX_WL_TX_IDX="0"
	fi

	let "TGT_CNT=0"
	for i in ${numlist}; do
		let "TGT_CNT+=1"
	done

	# find first empty white list location
	let "i=0"
	while [ "$i" -lt "20" ]; do
		eval tmpdiaguserno=\${"DIAG_USER_NO"$i}
		test "$tmpdiaguserno" = "" && break;
		let "i+=1"
	done

	# check again if enough space left to store
	dt=`date`
	let "total_cnt=$i+$TGT_CNT"
	if [ "$total_cnt" -gt "20" ]; then
		sms_log "can't add to white list, total count $total_cnt exceed max limit"
		echo "created=\"$dt\";"
		exit_with_unblock
	fi

	pre_batch_nv_write

	# add to white list
	for tgt in ${numlist}; do
		write_variable "smstools.diagconf.diag_user_no$i" "${tgt}" $nvflag
		write_variable "smstools.diagconf.diag_password$i" "" $nvflag
		let "i+=1"
	done
	let "i-=1"
	test "$i" -gt "$MAX_WL_TX_IDX" && write_variable "smstools.max_wl_no_idx" "$i" $nvflag

	post_batch_nv_write

	# recreate sms_diag.cfg file from nvram/rdb variable
	create_sms_diag_cfg_file

	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Reset Sent SMS Count
#-------------------------------------------------------------------------
if [ "$CMD" = "RESET_TX_SMS_CNT" ]; then
	pre_batch_nv_write
	write_variable "smstools.diagsms_txcnt" "0" $nvflag
	write_variable "smstools.first_diagsms_time" "0" $nvflag
	post_batch_nv_write
	echo "MaxWlTxDstIdx=\"$MAX_WL_TX_IDX\";"
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Mark read msg as 'read' by changing its name
#-------------------------------------------------------------------------
if [ "$CMD" = "MARK_READMSG" ]; then
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		echo "ReadMsgIndex=\"$index\";"
		echo "NewReadMsgFileName=\"$fname\";"
	else
		change_read_file_name $fname
		echo "ReadMsgIndex=\"$index\";"
		echo "NewReadMsgFileName=\"$NEW_FILE_NAME\";"
	fi
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Mark read msg as 'read' by changing its name
#-------------------------------------------------------------------------
if [ "$CMD" = "GET_UNREADMSG_CNT" ]; then
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		echo "unread=\"0\";"
	else
		UNREADMSGCNT=`ls $LOCAL_INBOX | grep "unread" | grep -v "grep" | awk 'END {print NR}'`
		echo "unread=\"$UNREADMSGCNT\";"
	fi
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Save SMSC Address
#-------------------------------------------------------------------------
if [ "$CMD" = "SAVE_SMSC_ADDR" ]; then
	save_smsc_addr "$NEW_SMSC_ADDR"
	RESULT="$?"
	ADDR=`rdb_get wwan.0.sms.smsc_addr`
	echo "NewSmscAddr=\"$ADDR\";"
	echo "Result=\"$RESULT\";"
	exit_with_unblock
fi

#-------------------------------------------------------------------------
# Save SMSC Address
#-------------------------------------------------------------------------
if [ "$CMD" = "SMS_UPDATE_UNREAD_CNT" ]; then
	CURR_DIR=`pwd`
	cd $LOCAL_INBOX
	FILECNT=`ls *_unread 2>/dev/null | awk 'END {print NR}'`
	echo "UnreadMsgCnt=\"$FILECNT\";"
	cd $CURR_DIR
	exit_with_unblock
fi




#!/bin/sh

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

. /etc/variant.sh

# read nvram/rdb variables and create new cfg files
read_vars_n_create_cfg_files

. $config_file
. $diag_config_file
. $ssmtp_config

atmgr_ready=`rdb_get atmgr.status`

tmp_mail=/tmp/mail

#echo "sms handler is called!"

if [ "$1" = "rx" ]; then

	# set rdb variable for blocked operation
	#block_sms_rx_processing

	# 2012.12
	# USE_SIM_STORAGE : YES : Platypus1, Platypus2
	# USE_SIM_STORAGE : NO  : Bovine, 6908
	if [ "$USE_SIM_STORAGE" = "YES" ]; then
		# create a rx sms file from rdb variables
		#save_one_msg
		#export DIAG_RX_ID=`rdb_get wwan.0.sms.cmd.param.message_id`
		export DIAG_TX_ID=""
	else
		# read all unread sms and store spool directory
		if [ "$platform" != "Avian" ] && [ "$atmgr_ready" != "ready" ] && [ "$V_USE_DCCD" != 'y' ]; then
			# only 6908 use below function at the moment
			read_and_save_sms
		fi
	fi

	cd $IN_SPOOL_PATH
	FILELIST=$(ls rxmsg*)
	FILECNT=$(echo $FILELIST | awk 'END {print NR}')
	test "$FILECNT" != "0" && sms_log "SMS message received!"

	for i in $FILELIST ; do
		if [ "$i" = "*" ]; then break; fi

		sms_log "------ processing RX msg $i --------"

		if [ "$USE_SIM_STORAGE" != "YES" ]; then
			cp ${i} $LOCAL_INBOX/ 2>/dev/null
		fi

		if [ "$REDIRECT_MOB" != "" ]; then
			sms_log "forward SMS file $i to mobile $REDIRECT_MOB"
			touch tmp
			SENDER=`awk '{ if (NR == 1) print $0}' $i`
			SENDT=`awk '{ if (NR == 3) print $0}' $i`
			RECVT=`awk '{ if (NR == 4) print $0}' $i`
			# remove coding type string to avoid confusion
			LINE_1=`awk '{ if (NR == 8) print $0}' $i| sed -e 's/GSM7://' -e 's/8BIT://' -e 's/UCS2://'`
			TXT=`awk '{ if (NR > 8) print $0}' $i`
			echo $SENDER >> tmp
			echo $SENDT >> tmp
			#echo $RECVT >> tmp
			echo $LINE_1 >> tmp
			echo $TXT >> tmp
			VAR=`cat tmp`
			sms_log "forwarding msg: $VAR"
			# for SIM/ME memory model, do not save forwarding msg
			$SMS_BIN_PATH/sendsms $REDIRECT_MOB "$VAR" "DIAG"
			rm tmp
			if [ "$USE_SIM_STORAGE" = "YES" ]; then
				# set tx id to delete in sms_admin.sh
				export DIAG_TX_ID=`rdb_get wwan.0.sms.cmd.param.message_id`
			fi
		fi

		if [ "$REDIRECT_EMAIL" != "" ]; then
			if [ "$i" = "*" ]; then break; fi
			sms_log "forwarding SMS file $i to $REDIRECT_EMAIL"
			echo "T0: $REDIRECT_EMAIL" > $tmp_mail
			echo "From: $FromSender" >> $tmp_mail
			echo "Subject: sms forward" >> $tmp_mail
			echo -e -n "\r\n" >> $tmp_mail
			#echo "message content!!" >> $tmp_mail
			VAR=`cat $i`
			echo "$VAR" >> $tmp_mail
			cat $tmp_mail | $SMS_BIN_PATH/ssmtp $REDIRECT_EMAIL
			rm $tmp_mail
		fi

		if [ "$REDIRECT_TCP" != "" ]; then
			if [ "$i" = "*" ]; then break; fi
			sms_log "forwarding SMS file $i to TCP $REDIRECT_TCP:$REDIRECT_TCP_PORT"
			cat $i | nc $REDIRECT_TCP $REDIRECT_TCP_PORT
		fi

		if [ "$REDIRECT_UDP" != "" ]; then
			if [ "$i" = "*" ]; then break; fi
			sms_log "forwarding SMS file $i to UDP $REDIRECT_UDP:$REDIRECT_UDP_PORT"
			cat $i | socat -u - udp4:$REDIRECT_UDP:$REDIRECT_UDP_PORT
		fi

		if [ "$ENABLE_REMOTE_CMD" = "1" ]; then
			#if enable sms script call
			if [ "$i" = "*" ]; then break; fi
			SENDER=`awk '/From:/ {print $2 }' $i`
			CMD_LINE=`awk '{if (NR>7) print $0}' $i`
			if [ "$USE_SIM_STORAGE" = "YES" ]; then
				export DIAG_RX_ID=`echo $i | sed 's/.*_0*\([0-9]\).*/\1/'`
				export DIAG_RX_FILE="$i"
			fi
			rm ${i} 2>/dev/null
			$SMS_BIN_PATH/sms_admin.sh "$SENDER" "${CMD_LINE}" "${i}"
			#sms_log "remote command $i"
		fi

		rm ${i} 2>/dev/null
	done

fi

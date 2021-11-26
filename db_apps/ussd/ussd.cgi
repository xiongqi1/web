#!/bin/sh
# USSD (Unstructured Supplementary Service Data) test script

# $1 USSD command : 
# 	init - Telstra USSD dialing
#		wwan.0.ussd.command dial
#
#	sel  - [0-9] or 00 for home/end
#		wwan.0.ussd.command select
# 		wwan.0.ussd.messages [0-9|00]
#		
#	status query
#		wwan.0.ussd.command status
#
# rdb variables between simple_at_manager
# 	wwan.0.ussd.command		- command buffer
# 	wwan.0.ussd.command.status	- command status
# 	wwan.0.ussd.messages		- server message

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

USSD_CMD="wwan.0.ussd.command"
USSD_CMD_ST="wwan.0.ussd.command.status"
USSD_CMD_RT="wwan.0.ussd.command.result"
USSD_MSGS="wwan.0.ussd.messages"

#-------------------------------------------------------------------------------
#
#   SUNROUTINES for USSD MODULE 
#
#-------------------------------------------------------------------------------
ussd_log()
{
    logger "ussd: ${1}"
}

check_storage()
{
    # check if nvram utility exist to determine to use nvram or rdb
    if [ "$DB_SET_CMD" = "" ] || [ "$DB_GET_CMD" = "" ]; then

	# find platform
	tmp=""
	PLATFORM="Unknown"
	if [ -e "/etc_ro/motd" ]; then
	    tmp=`cat /etc_ro/motd | grep -i platypus | grep -v "grep"`
	    if [ "$tmp" != "" ]; then
		PLATFORM="Platypus"
		NVRAMD_PATH="/bin"
	    fi
	elif  [ -e "/variant.sh" ]; then
	    tmp=`cat //variant.sh | grep -i finch | grep -v "grep"`
	    if [ "$tmp" != "" ]; then
		PLATFORM="Avian"
		NVRAMD_PATH="/system/cdcs/bin"
	    fi
	elif  [ -e "/etc/variant.sh" ]; then
	    PLATFORM="Bovine"
	    NVRAMD_PATH="/bin"
	fi

        has_nvram=`ls $NVRAMD_PATH | grep nvram_daemon | grep -v "grep"`
        if [ "$has_nvram" = "" ]; then
            export DB_SET_CMD="rdb_set"
            export DB_GET_CMD="rdb_get"
        else
            export DB_SET_CMD="nvram_set"
            export DB_GET_CMD="nvram_get"
        fi
    fi
}

# read a variable from nvram or rdb
# $1 : variable name, $2 : default value
read_variable()
{
    if [ "$DB_GET_CMD" = "" ]; then
        check_storage
    fi

    RD_VAL=`$DB_GET_CMD ${1}` 2>/dev/null
    # if has default nvram value, then set default value
    if [ "${2}" != "" ] && [ "$DB_GET_CMD" = "nvram_get" ] && [ "$RD_VAL" = "" ]; then
        $DB_SET_CMD "${1}" "${2}" >/dev/null
	RD_VAL=${2}
    fi
}

# write a variable to nvram or rdb
# $1 : variable name, $2 : default value
write_variable()
{
    if [ "$DB_SET_CMD" = "" ]; then
        check_storage
    fi
    ussd_log "$DB_SET_CMD ${1} ${2}"
    $DB_SET_CMD "${1}" "${2}" >/dev/null
}

ussd_rdb_var_clear()
{
    rdb_set $USSD_CMD
    rdb_set $USSD_CMD_ST
    rdb_set $USSD_CMD_RT
    #rdb_set $USSD_MSGS
}

encode_ussd_msg_body()
{
    test ! -e "/tmp/ussd_msgs" && USSD_MENU=""
    if [ "$USSD_MENU" = "" ]; then
	echo "UssdMsgBody=\"$USSD_MENU\";"
	return
    fi
    grep . /tmp/ussd_msgs | awk '{printf "%s\\n",$0}!//{print}' > /tmp/ussd_msgs2
    cat /tmp/ussd_msgs2 | sed -r 's/\\n*$//g' > /tmp/ussd_msgs
    cat /tmp/ussd_msgs | sed -e "s/\`/\\\`/g" -e 's/\"/\\\"/g' -e 's/\!/\\\!/g'> /tmp/ussd_msgs2
    USSD_MENU=`cat /tmp/ussd_msgs2`
    echo "UssdMsgBody=\"$USSD_MENU\";"
}

get_ussd_status()
{
    ussd_rdb_var_clear
    rdb_set $USSD_CMD status
    let "TIMEOUT=5"
    while true; do
        READ_RESULT=`rdb_get $USSD_CMD_RT | awk '{print $1}'`
        if [ "$READ_RESULT" = "[done]" ]; then
	    USSD_ST=`rdb_get $USSD_MSGS`
	    if [ -e "/tmp/ussd_msgs" ]; then
		USSD_MENU=`cat /tmp/ussd_msgs`
	    else
		USSD_MENU=""
	    fi
            return 1
        else
            let "TIMEOUT-=1"
            if [ $TIMEOUT -eq 0 ] || [ "$READ_RESULT" = "[error]" ]; then
                ussd_log "failed ussd config command"
		USSD_ST=""
		USSD_MENU=""
                return 0
            fi
            sleep 1
        fi
    done
}


get_ussd_config()
{
    get_ussd_status >/dev/null
    if [ "$USSD_ST" = "1" ]; then
	echo "UssdStatus=\"Active\";"
    else
	USSD_MENU=""
	echo "UssdStatus=\"Inactive\";"
    fi
    encode_ussd_msg_body
}

send_ussd_cmd()
{
    ussd_rdb_var_clear
    rdb_set $USSD_CMD $1
    let "TIMEOUT=10"
    while true; do
        READ_RESULT=`rdb_get $USSD_CMD_RT | awk '{print $1}'`
        if [ "$READ_RESULT" = "[done]" ]; then
                break
        else
            let "TIMEOUT-=1"
            if [ $TIMEOUT -eq 0 ] || [ "$READ_RESULT" = "[error]" ]; then
                ussd_log "failed [$1] command"
		CMD_RESULT="failure"
                break
            fi
            sleep 1
        fi
    done
    get_ussd_status >/dev/null
    USSD_MENU=""
    if [ "$USSD_ST" = "1" ]; then
	test -e "/tmp/ussd_msgs" && USSD_MENU=`cat /tmp/ussd_msgs`
	echo "UssdStatus=\"Active\";"
    else
	echo "UssdStatus=\"Inactive\";"
    fi

    # clear previous menu contents after session end
    if [ "$1" = "end" ]; then
        USSD_MENU=""
        test -e "/tmp/ussd_msgs" && rm "/tmp/ussd_msgs"
    fi

    encode_ussd_msg_body
    echo "UssdCmdResult=\"$CMD_RESULT\";"
}

#-------------------------------------------------------------------------------
#
#   MAIN ROUTINE for USSD MODULE 
#
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------
# parse command
#-------------------------------------------------------------------------
echo -e 'Content-type: text/html\n'

ussd_log "query str = $QUERY_STRING"

case $CMD in
    'USSD_CONF_GET')
	    get_ussd_config
	    ;;
    'USSD_START')
	    rdb_set $USSD_MSGS $UssdMenuSelection
	    send_ussd_cmd "dial"
	    ;;
    'USSD_END')
	    send_ussd_cmd "end"
	    ;;
    'USSD_SELECTION')
	    rdb_set $USSD_MSGS $UssdMenuSelection
	    send_ussd_cmd "select"
	    ;;
    *)
	    ussd_log "unknown cgi command"
	    ;;
esac

exit 0
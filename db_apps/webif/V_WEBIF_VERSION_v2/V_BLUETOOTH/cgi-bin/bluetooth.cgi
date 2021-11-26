#!/bin/sh

###############################################################################
#
# bluetooth.cgi
#
# Parses html post data for a bluetooth command request and invokes the
# appropriate RPC to execute the requested command.
# Returns result in an html response with an "rval" of 0 being success and
# any other value as a failure. Any command specific result string is returned
# in the "data" value.
#
##############################################################################

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
    exit 0
fi

SCRIPT_NAME=${0##*/}

#
# Converts the hex percentage chars in a URL encoded string into the
# corresponding ASCII characters.
#
# Args:
#    $1 = The URL encoded string
# Returns:
#    The URL encoded string with the percentage hex values converted to ASCII
#    characters.
#
convert_percentage_chars()
{
    echo "$1" | awk '
    #
    # Converts a string representation of a two digit hex number into
    # its numeric equivalent
    #
    function hex_str_to_num(hh) {
        hh_lower = tolower(hh)
        h1 = substr(hh_lower, 1, 1)
        h0 = substr(hh_lower, 2, 1)
        hexstr = "123456789abcdef"
        return index(hexstr, h1) * 16 + index(hexstr, h0)
    }

    {
        str = $0;
        while(match(str, /%../)) {
            if (RSTART > 1) {
               # emit everything before the %
               printf "%s", substr(str, 1, RSTART - 1)
            }

            # emit the two digit hex as an ascii char
            printf "%c", hex_str_to_num(substr(str, RSTART + 1, 2));

            # move past the parsed part of the string
            str = substr(str, RSTART + RLENGTH);
        }

        # emit the remaining string
        print str
    }'
}

parse_post_data()
{
    # Read the post data and split out individual key value pairs.
    local post_data=
    read -n ${CONTENT_LENGTH} post_data
    local split_data="`echo ${post_data} | tr "&" "\n"`"

    # Store each value into a variable that has the same name as the key
    for data in ${split_data}
    do
        local key="${data%=*}"
        local val="${data#*=}"
        case "${key}" in
            command|enable|time|address|passkey|confirm)
                export "${key}"="$(convert_percentage_chars "${val}")"
                ;;
            *)
                logger "${SCRIPT_NAME}: Invalid post data ${data}"
                ;;
        esac
    done
}

RPC_TIMEOUT=2
RPC_RESULT_LEN=4096
RPC_SVC="btmgr.rpc"

parse_post_data

#
# Send off the requested command to the btmgr rpc server.
#
rval=
data=
case "${command}" in
    get_devices)
        data=$(rdb invoke ${RPC_SVC} get_devices ${RPC_TIMEOUT} ${RPC_RESULT_LEN})
        rval=0
        ;;
    discoverable)
        if [ -z "${enable}" ]; then
            logger "${SCRIPT_NAME}: btmgr discoverable: missing enable param"
            rval=1
        else
            rdb invoke ${RPC_SVC} discoverable ${RPC_TIMEOUT} ${RPC_RESULT_LEN} enable ${enable}
            rval=$?
            data=$(rdb get bluetooth.conf.discoverable_timeout)
        fi
        ;;
    scan)
        if [ -z "${time}" ]; then
           logger "${SCRIPT_NAME}: btmgr scan: missing scan time"
           rval=1
        else
            rdb invoke ${RPC_SVC} scan ${RPC_TIMEOUT} ${RPC_RESULT_LEN} time ${time}
            rval=$?
        fi
        ;;
    pair)
        if [ -z "${address}" ]; then
            logger -t user.notice "btmgr bluetooth pair: missing address param"
            rval=1
        else
            data=$(rdb invoke ${RPC_SVC} pair ${RPC_TIMEOUT} ${RPC_RESULT_LEN} address ${address})
            rval=$?
        fi
        ;;
    pair_status)
        if [ -z "${address}" ]; then
            logger -t user.notice "btmgr bluetooth pair_status: missing address param"
            rval=1
        else
            data=$(rdb invoke ${RPC_SVC} pair_status ${RPC_TIMEOUT} ${RPC_RESULT_LEN} address ${address})
            rval=$?
        fi
        ;;
    passkey_confirm)
        if [ -z "${address}" -o -z "${passkey}" -o -z "${confirm}" ]; then
            logger -t user.notice "btmgr bluetooth passkey_confirm: missing params"
            rval=1
        else
            data=$(rdb invoke ${RPC_SVC} passkey_confirm ${RPC_TIMEOUT} ${RPC_RESULT_LEN} address ${address} passkey ${passkey} confirm ${confirm})
            rval=$?
        fi
        ;;
    unpair)
        if [ -z "${address}" ]; then
            logger -t user.notice "btmgr bluetooth unpair: missing address param"
            rval=1
        else
            data=$(rdb invoke ${RPC_SVC} unpair ${RPC_TIMEOUT} ${RPC_RESULT_LEN} address ${address})
            rval=$?
        fi
        ;;
    *)
        logger "${SCRIPT_NAME}: btmgr unknown command: ${command}"
        rval=1
        ;;
esac

#
# escape \ and double-quote in ${data} according to json spec
#
data=$(echo ${data} | sed -e 's/\\/\\\\/g' -e 's/\"/\\\"/g')

cat << EOF
Status: 200
Content-type: application/json
Cache-Control: no-cache
Connection: keep-alive

{
    "rval": "${rval}",
    "data": "${data}"
}
EOF


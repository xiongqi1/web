#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

OPENVPN_KEY_DIR="/usr/local/cdcs/openvpn-keys"
KEY_DIR="$OPENVPN_KEY_DIR/server"
KEY_FILE="$OPENVPN_KEY_DIR/server/server.crt"
SN_FILE="$OPENVPN_KEY_DIR/server/serial.old"

htmlWrite() {
	echo -n -e "$@"
}

htmlWrite "Status: 200\n"
htmlWrite "Content-type: text/plain\n"
htmlWrite "Cache-Control: no-cache\n"
htmlWrite "Connection: keep-alive\n\n"

SERIAL_NUM=""
test -e "$SN_FILE" && SERIAL_NUM=`cat $SN_FILE`

EXP_DATE=""
test -e "$KEY_FILE" && EXP_DATE=`grep "Not After" $KEY_FILE | cut -b 25-`

if [ ! -e "$KEY_FILE" -o -z "$SERIAL_NUM" -o -z "$EXP_DATE" ]; then
	htmlWrite "var result='ng';\n"
	exit 0
fi

htmlWrite "var serial_no='$SERIAL_NUM';\n"
htmlWrite "var exp_date='$EXP_DATE';\n"
htmlWrite "var result='ok';\n"
exit 0

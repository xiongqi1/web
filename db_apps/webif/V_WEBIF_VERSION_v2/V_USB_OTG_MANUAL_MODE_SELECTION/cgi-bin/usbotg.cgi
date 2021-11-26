#!/bin/sh

log() {
	logger -t "usbotg.cgi" -- "$@"
}

case $cmd in
	set_mode) info_cmd=0 ;;
	info) info_cmd=1 ;;
	*)
		# bypass if it is an unknown command
		log "unknown command specified - cmd='$cmd', opt1='$opt1', opt2='$opt2', opt3='$opt3'"

		# return error
		cat << EOF
{
	"cgiresult":255
}
EOF

		exit 0
		;;
esac

if [ "$info_cmd" = "0" ]; then
	if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
		exit 0
	fi

fi

# CSRF token
csrfTokenValid=0
if [ "$csrfToken" != "" -a "$csrfTokenGet" != "" -a "$csrfToken" = "$csrfTokenGet" ]; then
	csrfTokenValid=1
fi

encodeJSON() {
	arg="$1"
	i=0
	while [ $i -lt ${#arg} ]; do c=${arg:$i:1}; printf '\u%04X' "'$c'"; i=$((i+1)); done
}

uri_decode() {
	arg="$1"
	i="0"
	while [ "$i" -lt ${#arg} ]; do
		c0=${arg:$i:1}
		if [ "x$c0" = "x%" ]; then
			c1=${arg:$((i+1)):1}
			c2=${arg:$((i+2)):1}
			printf "\x$c1$c2"
			i=$((i+3))
		else
			echo -n "$c0"
			i=$((i+1))
		fi
	done
}

print_commands() {
	cat << EOF
info
set_mode
EOF
}

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteReply() {
	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: text/plain\n"
	htmlWrite "Cache-Control: no-cache\n"
	htmlWrite "Connection: keep-alive\n\n"
}

htmlCatTo() {
	target="$1"

	# download - max 64kb
	head -c $((64*1024*1024)) > "$target"

	return $?
}

htmlCatFrom() {
	source="$1"
	filename="$2"

	if [ -z "$filename" ]; then
		filename=$(basename "$source")
	fi

	if [ ! -r "$source" ]; then
		htmlWriteLog "cannot access file \'$source\': Permission denied"
		return 1;
	fi

	file_size=$(stat -c %s "$1")

	log "filename=$filename , file_size=$file_size"

	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: application/download\n";
	htmlWrite "Content-length: $file_size\n";
	htmlWrite "Content-transfer-encodig: binary\n";
	htmlWrite "Content-disposition: attachment; filename=\"$filename\"\n";
	htmlWrite "Connection: close\n\n"

	if ! cat "$source"; then
		htmlWriteLog "cannot cat file \'$source\': Return code $?"
		return 1;
	fi

	return 0
}

htmlReadUntilBlank() {
	while read line; do
		if ! echo "$line" | tr -d '\r' | grep -q "^$"; then
			continue
		fi

		return 0
	done

	return 1
}

htmlParsePostData() {
	while read line; do
		if echo "$line" | grep -q "Content-Disposition: .* filename="; then
			continue
		elif echo "$line" | grep -q "Content-Type: "; then
			htmlReadUntilBlank
			return 0
		elif echo "$line" | grep -q "Content-Disposition: form-data; name="; then
			variable=$(echo "$line" | sed -n 's/.* name="\([a-zA-Z_]*\)".*/\1/p')

			htmlReadUntilBlank

			read line
			value=$(echo "$line" | tr -d '\r')

			log "Disposition: $variable=$value detected"

			case "$variable" in
				'subaction')
					vpn_cgi_subaction="$value"
					;;

				'param')
					vpn_cgi_param="$value"
					;;

				*)
					;;
			esac
		fi

	done

	return 1
}

htmlWriteJSONB() {
	# print json output
	echo "{"
}

htmlWriteJSONE() {
	echo "\"cgiresult\":$cgiresult"
	echo "}"

}

usbotg_cgi_set_mode() {
	# CSRF token must be valid
	if [ "$csrfTokenValid" != "1" ]; then
		exit 254
	fi

	htmlWriteReply

	htmlWriteJSONB

	local rc=0
	local mode
	local bus
	local input_bus_valid=0

	mode="$opt1"
	bus="$opt2"

	source /etc/variant.sh

	# input must be valid
	if [ "$mode" = "otg" -o "$mode" = "host" -o "$mode" = "device" -o "$mode" = "down" ]; then
		if [ "$V_USB_OTG_BUS" = "none" ]; then
			rc=1
		else
			for valid_bus in $(echo "$V_USB_OTG_BUS" | tr ',' '\n'); do
				if [ "$bus" = "$valid_bus" ]; then
					intput_bus_valid=1
					break
				fi
			done
			if [ "$intput_bus_valid" = "1" ]; then
				log << EOF
cmd='$cmd'
mode='$opt1'
bus='$opt2'
EOF
				rdb_set -p "usb.otg.bus.$bus.mode" "$mode"
				rdb_set "usb.otg.bus.trigger" "1"
			else
				rc=1
			fi
		fi
	else
		rc=1
	fi

	cgiresult=$rc

	htmlWriteJSONE
}

usbotg_cgi_info() {
	htmlWriteReply

	htmlWriteJSONB

	source /etc/variant.sh

	local rc=0
	local sys_res
	local mode=""

	if [ "$V_USB_OTG_BUS" = "none" ]; then
		rc=1
	else
		echo '"otg_bus":['

		# for each of bus
		echo "$V_USB_OTG_BUS" | tr ',' '\n' | while read bus; do

			# bypass blank bus
			if [ -z "$bus" ]; then
				continue
			fi

			# print comma
			if [ -n "$mode" ]; then
				echo ","
			fi

			# get result from /usr/sbin/sys
			sys_res=$(sys -otg id "$bus")
			if echo "$sys_res" | grep -q 'host mode'; then
				mode="host"
			elif echo "$sys_res" | grep -q 'device mode'; then
				mode="device"
			elif echo "$sys_res" | grep -q 'USB port is disabled'; then
				mode="down"
			else
				mode="unknown"
			fi

			cfg=$(rdb_get "usb.otg.bus.$bus.mode")

			echo "	{"
			echo "		\"bus\":\"$bus\","
			echo "		\"mode\":\"$mode\","
			echo "		\"cfg\":\"$cfg\""
			echo "	}"

		done

		echo '],'

		rc=0
	fi

	cgiresult=$rc

	htmlWriteJSONE
}



if [ "$info_cmd" = "0" ]; then
	log "starting command... [cmd='${cmd}',opt1='${opt1}']"
fi

# start command
case $cmd in
	set_mode) usbotg_cgi_set_mode 2> /dev/null ;;
	info) usbotg_cgi_info 2> /dev/null ;;
esac

if [ "$info_cmd" = "0" ]; then
	if [ $cgiresult -eq 0 ]; then
		log "finishing command... succ ['${cmd}']"
	else
		log "finishing command... fail ['${cmd}',cgiresult:$cgiresult]"
	fi
fi

exit 0

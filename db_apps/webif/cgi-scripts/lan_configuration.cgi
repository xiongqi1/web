#!/bin/sh
# LAN configuration
# currently support:
# - configure
# return result in JSON format in HTTP response
# return code is indicated in JSON variable "rval": 0 for success; otherwise error

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
fi

. /lib/utils.sh

# print result in JSON format in HTTP response
print_output() {
	cat << EOF
Status: 200 OK
Content-type: application/json
Cache-Control: no-cache

{
    "rval": "$1"
}
EOF
}

lan_configure() {
	# validate input
	! validate_ip_address "$lanAddress" && print_output 1 && return
	! validate_netmask "$lanNetmask" && print_output 1 && return
	test -n "$lanHostName" && ! echo "$lanHostName" | grep -qE "^[.0-9A-Z_a-z-]+$" && print_output 1 && return
	test "$lanDnsMasquerade" != "0" -a "$lanDnsMasquerade" != "1" && print_output 1 && return
	! validate_ip_address "$lanDhcpRange" "," "{1}" && print_output 1 && return
	test -n "$aliasIp" && ! validate_ip_address "$aliasIp" && print_output 1 && return

	# set RDB variables
	if [ -z "$ipHandover" -o "$ipHandover" == "0" ]; then
		rdb_set link.profile.0.address "$lanAddress"
		rdb_set link.profile.0.netmask "$lanNetmask"
		rdb_set link.profile.0.hostname "$lanHostName"
		rdb_set service.dns.masquerade "$lanDnsMasquerade"
		rdb_set service.dhcp.range.0 "$lanDhcpRange"
	fi
	test -n "$aliasIp" && rdb_set service.alias_ip_address "$aliasIp"

	print_output 0
}

parse_post_data "lanAction lanAddress lanNetmask lanHostName lanDnsMasquerade lanDhcpRange ipHandover aliasIp"

case "$lanAction" in
	configure)
		lan_configure
		;;
esac

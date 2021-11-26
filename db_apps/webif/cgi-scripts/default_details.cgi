#!/bin/sh
source /etc/variant.sh 2>/dev/null
echo -e 'Content-type: text/html\n'

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

redirect_ip=""
# If alias IP exists in runtime config file, then use that address as redirect address
if [ "$V_RUNTIME_CONFIG" = "y" -a -n "$(rdb get service.runtime_config.current)" ]; then
	if [ -f "/usr/local/cdcs/conf/keep/runtime.conf" ]; then
		redirect_ip=`cat "/usr/local/cdcs/conf/keep/runtime.conf" 2> /dev/null |sed -n 's/^service.alias_ip_address;0;0;0;32;\(.*\)/\1/p' | tail -n 1`
	fi
fi

if [ "$redirect_ip" = "" ]; then
	# If alias IP exists then use this address as default LAN IP address
	redirect_ip=`cat /etc/cdcs/conf/default.conf 2> /dev/null |sed -n 's/^service.alias_ip_address;0;0;0;32;\(.*\)/\1/p' | tail -n 1`
	if [ -z "$redirect_ip" ]; then
		redirect_ip=`cat /etc/cdcs/conf/default.conf 2> /dev/null |sed -n 's/^link.profile.0.address;0;0;0;32;\(.*\)/\1/p' | tail -n 1`
	fi
fi

echo "var default_ip=\""$redirect_ip"\";"

v=`cat /etc/cdcs/conf/default.conf 2> /dev/null |sed -n 's/^admin.user.root;0;0;0;[0-9][0-9];\(.*\)/\1/p' | tail -n 1`
echo "var default_root_pass=\""$v"\";"

if [ "$V_SKIN" = "VDF" ]; then
	v=`cat /etc/cdcs/conf/default.conf 2> /dev/null |sed -n 's/^admin.user.user;0;0;0;96;\(.*\)/\1/p' | tail -n 1`
else
	v=`cat /etc/cdcs/conf/default.conf 2> /dev/null |sed -n 's/^admin.user.admin;0;0;0;96;\(.*\)/\1/p' | tail -n 1`
fi

echo "var default_admin_pass=\""$v"\";"

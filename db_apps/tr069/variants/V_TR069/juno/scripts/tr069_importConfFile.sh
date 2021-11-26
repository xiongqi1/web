#!/bin/sh
#
# Copyright (C) 2021 Casa Systems
#

o_fullname=$1

if [ "${o_fullname}x" = "x" ]; then
	exit 1
fi

bail() {
	rm -rf ${workingDir}
	exit 1
}

workingDir="$(mktemp -d)"
password=''

if [ "${workingDir}x" = "x" ]; then
	exit 1
fi

o_filename="$(basename ${o_fullname})"

cd ${workingDir} && tar -xzf ${o_fullname} || bail
dbcfg_import -i ${workingDir}/*.cfg -p "${password}" || bail

## Remove rdb object for tr069 transfers and events from original override.conf.
## And restore those from current rdb object.
overridefile="/usr/local/cdcs/conf/override.conf"
if [ -e "${overridefile}" ];then

	exList="
		tr069.event.
		tr069.transfer.
		"
	exPattrn=""
	tempfile=$(mktemp)

	while read rdbPrefix; do
		# trim whitespaces
		rdbPrefix=$(echo $rdbPrefix |awk '{$1=$1}1')
		if [ -n "$rdbPrefix" ]; then
			if [ -z "${exPattrn}" ]; then
				exPattrn="^${rdbPrefix}"
			else
				exPattrn="${exPattrn}\|^${rdbPrefix}"
			fi
		fi
	done << EOF
${exList}
EOF

	# remove rdb object.
	if [ -n "${exPattrn}" ]; then
		grep -v "${exPattrn}" "${overridefile}" > "${tempfile}"
	else
		cp -f "${overridefile}" "${tempfile}"
	fi

	# tr-069 pending transfers and events need to survive after reboot according to CWMP specs
	while read rdbPrefix; do
		# trim whitespaces
		rdbPrefix=$(echo $rdbPrefix |awk '{$1=$1}1')
		if [ -n "$rdbPrefix" ]; then
			rdb list "${rdbPrefix}" | grep "^${rdbPrefix}" | while read rdbName; do
				echo "${rdbName};0;0;0xf11;0x20;$(rdb get ${rdbName})" >> ${tempfile}
			done
		fi
	done << EOF
${exList}
EOF

	cp -f "${tempfile}" "${overridefile}"
	rm "${tempfile}"
fi

rm -rf ${workingDir}

sync

exit 0

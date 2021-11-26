#!/bin/sh

fullName="$1"
password=""

log() {
  /usr/bin/logger -t tr069_gzConfFile.sh -- $@
}

if [ ! -f $fullName ]; then
	log "Invalid argument: [${fullName}]"
	exit 1
fi

rm -f /opt/cdcs/upload/*.des3
rm -f /opt/cdcs/upload/*.cfg

cd /opt/cdcs/upload && tar -zxvf $fullName 2>/dev/null 1>/dev/null

cfg_filename="`ls /opt/cdcs/upload/*.cfg`"

if [ ! -f $cfg_filename ]; then
	log "Invalid configuration file: [${cfg_filename}]"
	exit 1
fi

cd /opt/cdcs/upload/ && dd if=/opt/cdcs/upload/vpn.des3 2>/dev/null |openssl des3 -d -k "${password}" 2>/dev/null |tar -C /usr/local/cdcs -zxf - 2>/dev/null 

st="$?"

if [ ! "$st" = "0" ]; then
	exit $st
fi

/usr/lib/tr-069/scripts/tr069_rebuildTR069Cfg.lua $cfg_filename
if [ ! "$?" = "0" ]; then
	exit 1
fi

dbcfg_import -i $cfg_filename -p "$password"

st="$?"

rm -f /opt/cdcs/upload/*.des3
rm -f /opt/cdcs/upload/*.cfg
rm -f $fullName

exit $st

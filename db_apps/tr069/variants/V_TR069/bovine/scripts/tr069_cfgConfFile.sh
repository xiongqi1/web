#!/bin/sh

fullName="$1"
password=""

log() {
  /usr/bin/logger -t tr069_cfgConfFile.sh -- $@
}

if [ ! -f $fullName ]; then
	log "Invalid argument: [${fullName}]"
	exit 1
fi

/usr/lib/tr-069/scripts/tr069_rebuildTR069Cfg.lua $fullName

if [ ! "$?" = "0" ]; then
	exit 1
fi

dbcfg_import -i $fullName -p "$password"

st="$?"

rm -f $fullName

if [ "$st" = "0" ]; then
	exit 0
else
	exit 255
fi
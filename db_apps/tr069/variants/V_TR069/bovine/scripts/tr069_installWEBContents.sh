#!/bin/sh

log() {
  /usr/bin/logger -t tr069_installManual.sh -- $@
}


if [ ! "$#" = "2" ]; then
	log "Usage: tr069_installManual.sh SrcFile DestFile"
	exit 1
fi

SRCFILE="$1"
DESFILE="$2"

if [ ! -f "$SRCFILE" ]; then
	log "ERROR: Source file is not available"
	exit 1
fi

DESDIR="`dirname \"$DESFILE\"`"

cd $DESDIR || mkdir -p $DESDIR

mv -f $SRCFILE $DESFILE

rm -f $SRCFILE
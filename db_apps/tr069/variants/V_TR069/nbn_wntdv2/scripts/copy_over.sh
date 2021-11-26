#!/bin/sh

SRC_DIR=/mnt/Arachnid-${1:-trunk}/Arachnid_src/db_apps/tr069

. /etc/variant.sh
PLATFORM=`head -2 /etc/version.txt | tail -1 | cut -d, -f1`
DEST_DIR=/usr/lib/tr-069


# core
cp $SRC_DIR/cwmpd/src/*.lua $DEST_DIR
for dir in $SRC_DIR/cwmpd/src/classes/* ; do
	dir=`basename $dir`
	mkdir -p $DEST_DIR/classes/$dir
	cp $SRC_DIR/cwmpd/src/classes/$dir/*.lua $DEST_DIR/classes/$dir
done
cp $SRC_DIR/cwmpd/src/handlers/*.lua $DEST_DIR/handlers

# platform
cp $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/core/* $DEST_DIR
for dir in $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/classes/* ; do
	dir=`basename $dir`
	if [ -d $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/classes/$dir ] ; then
		mkdir -p $DEST_DIR/classes/$dir
		cp $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/classes/$dir/*.lua $DEST_DIR/classes/$dir
	fi
done
cp $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/handlers/* $DEST_DIR/handlers
cp $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/scripts/* $DEST_DIR/scripts
cp $SRC_DIR/platforms/$PLATFORM/$V_PRODUCT/tr-069.conf /etc

# log files
rm -f /NAND/tr-069/cwmpd.out /NAND/tr-069/cwmpd.err
rm -f /NAND/tr-069/download.tr069

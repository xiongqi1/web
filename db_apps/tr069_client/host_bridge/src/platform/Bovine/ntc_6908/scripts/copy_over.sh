#!/bin/sh

SRC_DIR=/mnt/Bovine-trunk/Bovine_src/db_apps/tr069_client
DEST_DIR=/usr/lib/tr-069

cp $SRC_DIR/dimclient4/dimclient /usr/bin

cp -r $SRC_DIR/host_bridge/src/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Bovine/ntc_6908/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Bovine/ntc_6908/handlers/* $DEST_DIR/handlers
cp $SRC_DIR/host_bridge/src/platform/Bovine/ntc_6908/scripts/* $DEST_DIR/scripts

cp $SRC_DIR/host_bridge/src/platform/Bovine/ntc_6908/tr-069.conf /etc

rm -f /usr/local/DEBUG.log
rm -f /usr/local/TEST.log
rm -f /usr/local/out
rm -f /opt/cdcs/upload/download.tr069

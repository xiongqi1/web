#!/bin/sh

SRC_DIR=/mnt/Arachnid-new/Arachnid_src/db_apps/tr069_client
DEST_DIR=/usr/lib/tr-069

cp $SRC_DIR/dimclient4/dimclient /usr/bin

cp -r $SRC_DIR/host_bridge/src/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Arachnid/ntc_wntu/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Arachnid/ntc_wntu/handlers/* $DEST_DIR/handlers
cp $SRC_DIR/host_bridge/src/platform/Arachnid/ntc_wntu/scripts/* $DEST_DIR/scripts

cp $SRC_DIR/host_bridge/src/platform/Arachnid/ntc_wntu/tr-069.conf /etc

rm -f /tmp/DEBUG.log
rm -f /tmp/TEST.log
rm -f /tmp/out
rm -f /tmp/download.tr069

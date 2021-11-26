#!/bin/sh

SRC_DIR=/mnt/Arachnid-trunk/Arachnid_src/db_apps/tr069_client
DEST_DIR=/usr/lib/tr-069

cp $SRC_DIR/dimclient4/dimclient /usr/bin

cp -r $SRC_DIR/host_bridge/src/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Arachnid/nbn_wntd/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/Arachnid/nbn_wntd/handlers/* $DEST_DIR/handlers
cp $SRC_DIR/host_bridge/src/platform/Arachnid/nbn_wntd/scripts/* $DEST_DIR/scripts

cp $SRC_DIR/host_bridge/src/platform/Arachnid/nbn_wntd/tr-069.conf /etc

rm -f /tmp/DEBUG.log
rm -f /tmp/TEST.log
rm -f /tmp/out
rm -f /tmp/download.tr069

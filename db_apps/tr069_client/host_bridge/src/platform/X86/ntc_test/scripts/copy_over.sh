#!/bin/sh

SRC_DIR=/mnt/x86-test/src/tr069_client
DEST_DIR=/usr/lib/tr-069

cp $SRC_DIR/dimclient4/dimclient /usr/bin

cp -r $SRC_DIR/host_bridge/src/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/X86/ntc_test/core/* $DEST_DIR
cp $SRC_DIR/host_bridge/src/platform/X86/ntc_test/handlers/* $DEST_DIR/handlers
cp $SRC_DIR/host_bridge/src/platform/X86/ntc_test/scripts/* $DEST_DIR/scripts

cp $SRC_DIR/host_bridge/src/platform/X86/ntc_test/tr-069.conf /etc

rm -f /tmp/DEBUG.log
rm -f /tmp/TEST.log
rm -f /tmp/dimclient.out
rm -f /tmp/download.tr069

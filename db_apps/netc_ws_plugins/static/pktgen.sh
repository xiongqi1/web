#!/bin/bash

for fd in $(ls /proc/$$/fd); do
  case "$fd" in
    0|1|2)
      ;;
    *)
      eval "exec $fd>&-"
      ;;
  esac
done

RVAL=`bc <<< "$1 / (1400*8)"`

# REMOTE_MAC="6c:4b:90:0e:03:79"
REMOTE_MAC="6c:4b:90:0e:03:33"

HAVEKO=`lsmod | grep netmap`
if [ -z "$HAVEKO" ]; then
 sudo insmod /home/demo/Dev/netmap*/netmap.ko
fi
while [ true ]; do sudo pkt-gen -i eno4 -f tx -d 192.168.10.50 -s 192.168.10.2 -D $REMOTE_MAC -S ac:1f:6b:1b:fc:53 -l 1400 -R ${RVAL}; done

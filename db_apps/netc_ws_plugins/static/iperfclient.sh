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

while [ true ]; do iperf -c 192.168.10.1 -u -t 100000 -b$1; done

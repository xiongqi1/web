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

iperf -s -u

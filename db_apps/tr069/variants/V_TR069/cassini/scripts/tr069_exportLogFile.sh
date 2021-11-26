#!/bin/sh
#
# Copyright (C) 2020 Casa Systems
#

o_fullname="$1"

if [ -z "$o_fullname" ]; then
	exit 1;
fi

logcat.sh -a > "$o_fullname" || exit 1

exit 0

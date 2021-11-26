#!/bin/sh

o_fullname=$1
if [ "${o_fullname}x" = "x" ]; then
	exit 1
fi

case "$o_fullname" in
	*core*)
		logdir='/smartcity/ems_ic_adapter/core/log'
		;;
	*)
		logdir='/smartcity/ems_ic_adapter/app/log'
		;;
esac

tar -zcf $o_fullname --directory=$logdir .
exit 0

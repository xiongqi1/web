#!/bin/sh


print_usage() {
	cat << EOF

ctrl_dispd.sh - control dispd LED control

usage>
	ctrl_dispd.sh (router_mode|user_mode|stat)

	router_mode	: set to normal operation mode
	user_mode	: set to user LED overriding mode
	stat		: shows current operation mode

EOF
}



case $1 in
	'router_mode')
		rdb_set "dispd.disable" 0
		;;

	'user_mode')
		rdb_set "dispd.disable" 1
		;;
	

	'stat')
		stat=$(rdb_get "dispd.disable")
		if [ "$stat" = "1" ]; then
			echo "user_mode"
		else
			echo "router_mode"
		fi
		;;

	*)
		print_usage
		exit 1
		;;
esac

exit 0

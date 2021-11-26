#!/bin/sh

# TODO: delete the following default when the buildsystem gives the path
if ! which "rdb_get" 2> /dev/null > /dev/null; then
	PATH=../../staging_test:$PATH
fi

# get directory info.
app="$0"
app_fpath=$(readlink -f "$app")
prj_dir=$(dirname "$app_fpath")

# switch to project dir
cd "$prj_dir"
PATH=./:./obj-Host:$PATH

log() {
	echo "$*"
}

test_section() {
	echo "* $*"
}

test_session_end() {
	echo ""
}

test_desc() {
	echo -n "$* : "
}

test_result() {
	rc=$?

	msg=""
	if [ "$rc" -eq 0 ]; then
		msg="OK"
	else
		msg="FAIL"
	fi

	if [ $# -gt 0 -a -n "$1" ]; then
		echo "$msg - $*"
	else
		echo "$msg"
	fi

	return $rc
}

err() {
	echo "$*" >&2
}

start_io_mgr() {
	io_mgr=$(which io_mgr 2> /dev/null)
	# launch io mgr
	if [ -z "$io_mgr" ]; then
		return 1
	fi

	if [ ! -x "$io_mgr" ]; then
		return 1
	fi

	io_mgr -i &
	pid_io_mgr=$!
}

stop_io_mgr() {
	if [ -z "$pid_io_mgr" ]; then
		return 0
	fi

	kill $pid_io_mgr 2> /dev/null

	for i in 1 2 3 4 5; do
		if [ ! -e /proc/$pid_io_mgr ]; then
			break;
		fi
		sleep 1
	done

	kill -9 $pid_io_mgr 2> /dev/null
}

bail() {
	if [ $# -gt 0 ]; then
		echo "$*" >&2
	fi

	stop_io_mgr
	exit 1
}

test_section "framework environment test"
####################################################################################
	test_desc "check location of IO manager"
		which io_mgr 2> /dev/null > /dev/null && test -e $(which io_mgr)
		test_result || bail

	test_desc "check location of rdb tools"
		which rdb_get 2> /dev/null > /dev/null && test -e $(which rdb_get)
		test_result || bail

	test_session_end

test_section "IO manager launch test"
####################################################################################

	test_desc "clear IO manager ready flag"
		rdb_set "sys.sensors.iocfg.mgr.ready" "0"
		test_result || bail

	test_desc "wait for IO manage ready flag"
		rdb_wait "sys.sensors.iocfg.mgr.ready" 15 &
		pid_rdb=$!
		test_result "pid=$pid_rdb" || bail

	test_desc "launch IO manager"
		start_io_mgr 2> /dev/null > /dev/null
		test_result || bail

	test_desc "check process running status of IO manager"
		test -e "/proc/$pid_io_mgr"
		test_result "pid='$pid_io_mgr'" || bail

	test_desc "wait for IO manager ready flag up to 15 seconds"
		wait $pid_rdb
		rdb_ready=$(rdb_get "sys.sensors.iocfg.mgr.ready")
		test_result "ready=$rdb_ready" || bail

	test_desc "check RDB interface reaction of IO manager up to 2 times"
		for i in 1 2 3 4 5; do
			if io_ctl stat 2> /dev/null > /dev/null; then
				break
			fi
			sleep 1
		done
		test_result || bail

	test_desc "check RDB communication of IO manager"
		stat=$(io_ctl stat 2> /dev/null)
		test "$stat" = "running"
		test_result "stat='$stat'" || bail
test_session_end

test_section "sensor input test"
####################################################################################
	test_desc "list up IO facilities"
		ios=$(io_ctl list 2> /dev/null | tr '\n' ' ')
		lio=$(io_ctl list 2> /dev/null | tail -n 1)
		test -n "$ios" -a -n "$lio"
		test_result || bail

	test_desc "build analogue ios"
		aios=""
		for i in $ios; do
			if ! io_ctl get_mode $i 2> /dev/null | grep -q '\banalogue_in\b' 2> /dev/null; then
				continue
			fi
			aios="$aios $i"
		done
		test_result "$aios" || bail

	test_desc "enable socket input mode"
		io_ctl hfi_enable_input
		test_result "$ios" || bail

	test_desc "wait for port up to 5 seconds"
		for i in 1 2 3 4 5; do
			if netstat -n -l | grep -q ':30001\b'; then
				break;
			fi
			sleep 1;
		done
		netstat -n -l | grep -q ':30001\b'
		test_result || bail

	#test_desc "reset analogue values ($i)"
		for i in $aios; do
			test_desc "reset analogue values ($i)"
			rdb_set "sys.sensors.io.$i.adc" "0.0"
			test_result || bail
		done

	for k in 1 2 3; do
		test_desc "build a 10 second test analogue stream $k/3"
			tm=0
			while [ $tm -lt 10000 ]; do

				j=0
				for i in $aios; do
					j=$(($j+1))

					if [ $k = 1 -o $k = 3 ]; then
						j=0;
					fi
					echo "$i,ain,$tm:$j,$j,$j,$j,$j,$j,$j,$j,$j,$j"
				done 
				tm=$((tm+500))
			done > "stream-seq.txt"
			flen=$(stat -c %s "stream-seq.txt" 2> /dev/null)
			test -n "$flen" -a $flen -gt 0
			test_result || bail

		test_desc "inject the analogue stream"
			io_ctl hfi_netcat_input "stream-seq.txt"
			test_result || bail

		sleep 2
		test_desc "check running status of input stream mode"
			input_stream_mode=$(rdb_get "sys.sensors.iocfg.mgr.input_stream_mode")
			test "$input_stream_mode" = "1"
			test_result "mode='$input_stream_mode'"|| bail

		test_desc "wait for input up to 15 seconds"
			# wait for the last
			rdb_wait "sys.sensors.iocfg.mgr.input_stream_mode" 15 &
			pid_rdb=$!
			wait $pid_rdb
			test_result || bail

		#test_desc "check analogue inputs ($i)"
			j=0
			for i in $aios; do
				j=$(($j+1))
				test_desc "check analogue inputs ($i)"

				if [ $k = 1 -o $k = 3 ]; then
					j=0;
				fi

				v=$(io_ctl read_analogue $i)
				test "${v%%.*}" = "${j}"
				test_result "$v" || bail
			done
	done
	test_session_end

test_section "IO manager termination test"
####################################################################################
	test_desc "terminate IO manager"
		kill $pid_io_mgr 2> /dev/null
		stop_io_mgr 
		test ! -e "/proc/$pid_io_mgr"
		test_result "pid=$pid_io_mgr" || bail
	test_session_end


####################################################################################
sed_pin_with_func() {
	sed -n "
		/$1/ {
			s/.* param(pin=\([0-9]\+\).*/\1/p
		}
	" "$2" 2> /dev/null
}

test_section "GPIO log test"
####################################################################################
	test_desc "check GPIO log existing"
		test -r "./gpio.log"
		test_result || bail

	count_merge=$(sed_pin_with_func ':gpio_request_pin>\|:gpio_free_pin>' "gpio.log" | sort -u | wc -l)
	count_req=$(sed_pin_with_func ':gpio_request_pin>' "gpio.log" | sort -u | wc -l)
	count_free=$(sed_pin_with_func ':gpio_free_pin>' "gpio.log" | sort -u | wc -l)

	test_desc "check GPIO req/free"
		test "$count_merge" -eq "$count_req" -a "$count_merge" -eq "$count_free"
		test_result "merge=$count_merge,req=$count_req,free=$count_free" || bail

	test_session_end

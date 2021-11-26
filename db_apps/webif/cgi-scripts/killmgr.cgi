#!/bin/sh
kill_appl() {
	while true;do
		killall -9 $1
		test `pidof $1` || break
	done
}

kill_appl connection_mgr 2>/dev/null
kill_appl simple_at_manager 2>/dev/null
kill_appl cnsmgr 2>/dev/null
echo -e 'Content-type: text/html\n'
exit 0
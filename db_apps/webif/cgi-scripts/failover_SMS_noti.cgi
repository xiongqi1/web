#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "failover_SMS_noti.cgi" -- $@
}


log "starting..."

# parse parameter
eval $(sed "s/&/\n/g;s/=/='/g" | sed "s/$/'/g")

set | grep "^SMS_" | log

echo -e "Status: 200\nContent-type: text/plain\nPragma: no-cache\nCache-Control: no-cache\n\n"

log "launching - failover_SMS_noti testsms '$SMS_to' '$deviceName'"
if failover_SMS_noti testsms "$SMS_to" "$deviceName" 2>/dev/null >/dev/null; then
	echo "result=0;"
else
	echo "result=1;"
fi

rdb_set "wwan.0.FailoverNoti.TestSMS_to" ""

log "done"

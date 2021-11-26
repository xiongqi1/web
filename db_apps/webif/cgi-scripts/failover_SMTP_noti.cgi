#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "failover_SMTP_noti.cgi" -- $@
}


log "starting..."

# parse parameter
eval $(sed "s/&/\n/g;s/=/='/g" | sed "s/$/'/g")

# log
set | grep "^Email_" | log


echo -e "Status: 200\nContent-type: text/plain\nPragma: no-cache\nCache-Control: no-cache\n\n"

log "launching - failover_SMTP_noti send testmail '$Email_username' '****' '$Email_serveraddr' '$Email_serverport' '$Email_from' '$Email_to' '$Email_cc' '$deviceName'"
if failover_SMTP_noti "send" "testmail" "$Email_username" "$Email_passwd" "$Email_serveraddr" "$Email_serverport" "$Email_from" "$Email_to" "$Email_cc" "$deviceName" 2>/dev/null >/dev/null; then
	echo "result=0;"
else
	echo "result=1;"
fi

log "done"

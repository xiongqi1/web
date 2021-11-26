#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
  exit 0
fi

source /lib/linkProfileLib.sh

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache
Connection: keep-alive

EOF
}

# Get next profile and send JSON response
sendHttpOk
result="ok"
num=$(lplGetLinkProfile)
[ -z "$num" ] && result="Out of space"
cat <<EOF
{
  "result":"$result",
  "newProfileNum":"$num"
}
EOF

exit 0

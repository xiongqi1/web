#!/bin/sh
#
# Http post handler to upload a CBQ file.
#
# We are expecting stdin to look something like this:
#   -----------------------------38026035515432338451676012903^M
#   Content-Disposition: form-data; name="cbqInitFile"; filename="cbq-nnnn.nnn"^M
#   Content-Type: application/octet-stream^M
#   ^M
#   Line 1
#   ...
#   line N
#   ^M
#   -----------------------------38026035515432338451676012903--^M

nof=`basename $0`           # program name
msgFilename=""              # Name of downloaded file from message header.
tmpFile="/tmp/$nof.tmp"     # Temporary file

source /lib/utils.sh

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache

EOF
}

sendBadHdrAndExit() {
  sendHttpOk
  cat <<EOF
{
  "result":"bad message header"
}
EOF
  exit 0
}

# Ignore if session not current
if [ -z "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ] ;then
  sendHttpOk
  cat <<EOF
{
  "result":"session timed out"
}
EOF
  exit 0
fi

readUntilBlank() {
	while read line; do
    if ! echo "$line" | tr -d '\r' | grep -q "^$" ;then
			continue
		fi
    return 0
  done
  return 1
}

discardHdr() {
  while read line; do
    if echo "$line" | grep -q "Content-Disposition: .* filename=" ;then
      msgFilename=$(echo $line | sed -r "s/^.+filename=\"(.+)\".*/\1/")
      continue
    elif echo "$line" | grep -q "Content-Type: " ;then
      readUntilBlank || return 1
      return 0
    else
      continue
    fi
  done
  return 1
}

downloadFile() {
  local boundary
  local rdbVar

  # Get the content boundary.
  # We expect $CONTENT_TYPE to look something like this:
  #   multipart/form-data; boundary=---------------------------3802603551543233845167601290
  boundary=${CONTENT_TYPE#*boundary=}

  # Filter and write the data to the RDB variable, and tmp file.
  # Notes:
  #   - sed filter:
  #     - it removes the end boundary line
  #     - it allows for extra '-' before the boundary and excess chars after it
  #   - Use "rdb set" instead of "rdb_set" to handle leading hyphens better.
  discardHdr || sendBadHdrAndExit
  checkFilename $msgFilename
  rdbVar="qos.cbqInit.file.$msgFilename"
  rdb set $rdbVar "$(cat 2> /dev/null | sed -r "/^-*${boundary}.*/d" | tee $tmpFile)"

  # Trigger the template
  rdb_set "qos.cbqInit.trigger" "1"

  /bin/rm -f $tmpFile
  sendHttpOk
  cat <<EOF
{
  "result":"ok"
}
EOF
}

sendBadFilename()
{
  local name=$1
  local msg=$2
  sendHttpOk
  cat <<EOF
{
  "result":"$msg",
  "name":"$name"
}
EOF
  exit 0
}

# Check name, exit if bad.
# Rules:
# - name: cbq-NNNN.<name>
# - NNNN: 0002-FFFF (cbq class ID)
# - name: alpha-numeric
checkFilename()
{
  local name=$1

  expr $name : "cbq-" > /dev/null \
    || sendBadFilename $name "Invalid prefix, should be: 'cbq-'"
  [ "${name%.*}" = "$name" ] \
    && sendBadFilename $name "Missing file extension"
  local classId=${name#cbq-}
  classId=${classId%.*}
  [ "$classId" = "0001" ] \
    && sendBadFilename $name "Invalid class ID, 0001 is not supported"
  echo $classId | egrep "[[:xdigit:]]{4}" > /dev/null 2>&1 \
    || sendBadFilename $name "Invalid class ID, should be: 0002-FFFF"
}

# Expect query: cmd=upload
arg=${QUERY_STRING#*=}
case "$arg" in
"upload") downloadFile ;;
*)
  logErr "Unknown cmd: $QUERY_STRING"
  sendHttpOk
  cat <<EOF
{
  "result":"unknown command",
  "query":"$QUERY_STRING"
}
EOF
  exit 0
  ;;
esac

exit 0

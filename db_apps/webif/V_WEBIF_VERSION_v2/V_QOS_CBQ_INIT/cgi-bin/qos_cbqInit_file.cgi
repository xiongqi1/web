#!/bin/sh
#
# Http get handler to show/delete a CBQ file.
#

nof=`basename $0`           # program name

source /lib/utils.sh

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache

EOF
}

# Ignore if session not current
if [ -z "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ] ;then
  exit 0
fi

doDelete()
{
  local filename=$1

  rdb unset "qos.cbqInit.file."$filename

  # Trigger the template
  rdb_set "qos.cbqInit.trigger" "1"

  sendHttpOk
}

doShow()
{
  local filename=$1

  sendHttpOk
  echo "<pre>"
  rdb_get "qos.cbqInit.file."$filename | sed -r "s/*$//"
  echo "</pre>"
}

# Expect query: delete=<file>
cmd=${QUERY_STRING%=*}
arg=${QUERY_STRING#*=}
case "$cmd" in
"delete") doDelete $arg ;;
"show")   doShow   $arg ;;
*)        logErr "Unknown cmd: $QUERY_STRING" ;;
esac

exit 0

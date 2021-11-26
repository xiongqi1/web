#!/bin/sh
#
# Shell script to generate a list of available APs via the Linux WiFi driver.
#
. /etc/variant.sh
nof=`basename $0`           # Program name
radioInstance=0             # Default client radio instance to use.

test -z $(echo $PATH | grep local) && PATH="/usr/local/bin:/usr/local/sbin:"$PATH

# To log messages to syslog, prefixed by script name.
log() {
  logger -t $nof -- $@
}

if [ "$V_WIFI_CLIENT" = "qca_soc_lsdk" ]; then
  source /lib/coproUtils.sh
  getCoproDetails ipAddr user passwd
fi

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache
Connection: keep-alive

EOF
}

sendHttpHtmlHdr() {
  local msg="$1"
  cat <<EOF
Status: $msg
Content-Type: text/html; charset=iso-8859-1
Connection: keep-alive

EOF
}

sendHttpHtmlDoc() {
  local msg="$1"
  local text="$2"
  cat <<EOF
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<HTML><HEAD>
<TITLE>$msg</TITLE>
</HEAD><BODY>
<H2>$msg</H2>
$text <P>
</BODY></HTML>

EOF
}

sendHttpNoSession() {
  sendHttpHtmlHdr "408"
  sendHttpHtmlDoc "The session timed out"
}

sendHttpBadArg() {
  local argValue="$1"
  sendHttpHtmlHdr "400"
  sendHttpHtmlDoc "The query argument is unknown: $argValue"
}

# Ignore if session not current
if [ -z "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ] ;then
  sendHttpNoSession
  exit 0
fi

# Process query command and options
for i in $(echo $QUERY_STRING | sed "s/&/ /g")
do
  case $i in
  "cmd=getApScanList") ;;
  radio=*) radioInstance=${i#radio=} ;;
  *)
    log "Unknown cmd: $QUERY_STRING"
    sendHttpBadArg "$QUERY_STRING"
    exit 0
    ;;
  esac
done

# Scan using "wpa_cli" command.
# Note this scan is unreliable.
# Sometimes the results are stale.
doWpaCliScan()
{
  # Initiate a scan
  interface="wlan_sta${radioInstance}"
  if ! wpa_cli -i $interface scan > /dev/null 2>&1 ;then
    sendHttpOk
    cat <<EOF
{
"result":"1"
}
EOF
    exit 0
  fi

  # Get connected AP
  # Note: must be done after scan started, otherwise it will hang.
  bssid=$(wpa_cli -i $interface status | grep "bssid=")
  bssid=${bssid#"bssid="}

  # Send result
  sendHttpOk
  echo "{"
  wpa_cli scan_results | ./wlan_sta_linux_ap_scan.awk -v callerpid=$$

  cat <<EOF
,
  "connectedBssid":"$bssid"
EOF
}

# Scan using "iw" command.
# Note the scan can take around 5 seconds.
doIwScan()
{
  interface="wlan_sta${radioInstance}"

  # Do scan and process result
  sendHttpOk
  echo "{"
  local ret=0
  if [ "$V_WIFI_HW" = "rtl8723" ]; then
    # use lockfile to avoid any race conditions
    lockfile-create /tmp/lockscanap
    # timeout 10 seconds: if iw doesn't complete within 10 seconds, send signal TERM to terminate it
    timeout -t 10 iw dev $interface scan 2>/dev/null > /tmp/scanresult
    ret=$?
    if [ "$ret" = "0" ]; then
      wlan_sta_linux_ap_scan.awk -v callerpid=$$ < /tmp/scanresult
    fi
    rm /tmp/scanresult
    lockfile-remove /tmp/lockscanap
  else
    iw dev $interface scan | wlan_sta_linux_ap_scan.awk -v callerpid=$$
  fi
  return $ret
}

doQcaScan()
{
  sendHttpOk
  echo "{"
  local returnVal=""
  local bssid=""

  # Due to poor performance of WIFI system and interfaceing method, it takes much more time to get information.
  local timeout="60"

  returnVal=$(coproCmd $ipAddr $user $passwd "wifi_client_ap_scan.sh" "$timeout") 
  echo "$returnVal" | wlan_sta_linux_ap_scan.awk -v coproc=qca callerpid=$$

  local wpa_status=$(rdb_get wlan_sta.0.ap.0.status.wpa_status)
  local sta_bssid=$(rdb_get wlan_sta.0.ap.0.status.bssid)
  returnVal=$(coproCmd $ipAddr $user $passwd "wpa_cli status 2>/dev/null") 

  if [ "$wpa_status" =  "COMPLETED" ]; then
        bssid="$sta_bssid"
  else
    bssid=""
  fi

  cat <<EOF
,
  "connectedBssid":"$bssid"
EOF
}

if [ "$V_WIFI_CLIENT" = "qca_soc_lsdk" ]; then
  doQcaScan
  res=$?
else
  # The "iw" scan is more reliable (but takes longer) so use it now.
  #doWpaCliScan
  doIwScan
  res=$?
fi
# in case of error or timeout, doIwScan does not generate any JSON output so "," is not printed here
if [ "$res" = "0" ]; then
  echo ","
fi
# Send result
cat <<EOF
  "result":"$res"
}
EOF

exit 0

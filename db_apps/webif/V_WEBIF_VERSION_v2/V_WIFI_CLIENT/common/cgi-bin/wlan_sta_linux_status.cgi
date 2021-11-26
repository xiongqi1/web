#!/bin/sh

. /etc/variant.sh

radioInstance="0"     # Default client radio instance to use.
needOnlyExtraStatus="0"

# HTTP Error code 440 is not official,
# but some service,such as IIS, use it as "Login Timeout"(The client's session has expired and must log in again.)
sendHttpSessionTimeout() {
  cat <<EOF
Status: 440
Content-Type: text/plain
Cache-Control: no-cache
Connection: keep-alive

EOF
}

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
  sendHttpSessionTimeout
  exit 0
fi

test -z $(echo $PATH | grep local) && PATH="/usr/local/bin:/usr/local/sbin:"$PATH

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache
Connection: keep-alive

EOF
}

getChan() {
  local frequency

  if [ "$V_WIFI_HW" = "rtl8723" ]; then
    # Frequency is retrieved from output of "iw dev $dev survey dump" as below.
    # However, "iw dev $dev survey dump" is not implemeneted in rtl8723bu driver.
    # Hence frequency is parsed from "iw dev $dev link" with this driver.
    # However "iw dev $dev link" only reports frequency when connection is established.
    # If channel is needed before that, the channel can be read from a /proc file of rtl8723
    # that is "/proc/net/rtl8723bu/wlan_sta0/rf_info".
    #frequency="$(iw dev $dev link | awk '/freq:/ { print $2; }')"
    #
    # Note: "iw dev $dev link" sometimes reports outdated channel
    # even when channel has changed.
    # /proc/net/rtl8723bu/wlan_sta0/rf_info is more reliable.
    #
    # "cat /proc/net/rtl8723bu/wlan_sta0/rf_info" produces something like this:
    # cur_ch=1, cur_bw=0, cur_ch_offset=0
    # oper_ch=1, oper_bw=0, oper_ch_offset=0
    # where cur_xx means lastest parameters used for connection,
    # while oper_xx means current operating parameters.
    # e.g. if STA was connected to channel 1 but now gets disconnected and is
    # scanning on channel 6, then cur_ch=1, oper_ch=6. Once it gets connected
    # to channel 6, cur_ch=oper_ch=6.
    RF_INFO="/proc/net/rtl8723bu/$dev/rf_info"
    [ -r "$RF_INFO" ] && cat "$RF_INFO" |
      sed -n \
        's/^[[:space:]]*cur_ch[[:space:]]*=[[:space:]]*\([[:digit:]]*\).*/\1/p'
    return
  elif [ "$V_WIFI_CLIENT" = "qca_soc_lsdk" ]; then
    frequency=$(rdb_get wlan_sta.0.ap.0.status.freq)
  else
    frequency="$(iw dev $dev survey dump | awk '/frequency/ { print $2; }')"
  fi

  case "$frequency" in
  "2412"|"2.412") echo "1"  ;;
  "2417"|"2.417") echo "2"  ;;
  "2422"|"2.422") echo "3"  ;;
  "2427"|"2.427") echo "4"  ;;
  "2432"|"2.432") echo "5"  ;;
  "2437"|"2.437") echo "6"  ;;
  "2442"|"2.442") echo "7"  ;;
  "2447"|"2.447") echo "8"  ;;
  "2452"|"2.452") echo "9"  ;;
  "2457"|"2.457") echo "10" ;;
  "2462"|"2.462") echo "11" ;;
  "2467"|"2.467") echo "12" ;;
  "2472"|"2.472") echo "13" ;;
  "2484"|"2.484") echo "14" ;;
  *)      echo ""   ;;
  esac
}

getState() {
  if [ "$V_WIFI_CLIENT" = "qca_soc_lsdk" ]; then
    echo "$(rdb_get wlan_sta.0.ap.0.status.wpa_status)"
  else
    wpa_cli -i $dev status |
    awk '/wpa_state=/ {
      match($0, "wpa_state=");
      print substr($0, RSTART+RLENGTH);
    }'
  fi
}

getExtraStatus() {
  if [ "$radioInstance" -eq "$radioInstance" ] 2>/dev/null
  then
    rdb get "wlan_sta.${radioInstance}.extra_status"
  fi
}

# Process query
for i in $(echo $QUERY_STRING | sed "s/&/ /g")
do
  case $i in
  radio=*) radioInstance=${i#radio=} ;;
  onlyextra=*) needOnlyExtraStatus=${i#onlyextra=} ;;
  *)
    log "Unknown cmd: $QUERY_STRING"
    exit 0
    ;;
  esac
done
dev="wlan_sta${radioInstance}"

# Get status and send JSON response
if [ "$V_WIFI_CLIENT" = "backports" ]; then
  extra_status="$(getExtraStatus)"
fi
sendHttpOk
if [ "$needOnlyExtraStatus" = "0" ]; then
  state=$(getState)
  if [ "$state" = "COMPLETED" ]; then
    state="Connected"
    chan=$(getChan)
  else # do not show the channel unless connected
    chan=""
  fi
  cat <<EOF
{
  "chan":"$chan",
  "state":"$state",
  "extra_status":"$extra_status"
}
EOF

else
  cat <<EOF
{
  "extra_status":"$extra_status"
}
EOF

fi
exit 0

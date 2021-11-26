#!/bin/sh
#
# Add static route for SAS SERVER to go through CBRS APN interface
#
# Copyright (C) 2019 Casa Systems Inc
#

nof=${0##*/}                        # Name of file/script.
source /lib/utils.sh

SCRIPT="/usr/bin/$nof"

# don't support mulitple instances of sas_route.template
TIMER_PID=$(ps w|awk '/[o]ne_shot/ && /sas_route/ {print $1}')
if [ ! -z "$TIMER_PID" ]; then
  logNotice "Stop $nof timer PID:$TIMER_PID"
  kill -9 $TIMER_PID
fi

# check if SAS server defined
SAS_URL=$(rdb_get sas.config.url)
if [ -z "$SAS_URL" ]; then exit; fi

SAS_PDP_PROFILE=$(rdb_get sas.config.pdp_profile)
PFNO=$(getPrfNoOfPdpFunction CBRSSAS $SAS_PDP_PROFILE)
NETIF=$(rdb_get link.profile.${PFNO}.interface)

# lookup IP address for SAS server, can be multiple addresses
SAS_SERVER=$(rdb_get sas.config.url|awk -F[/:] '{print $4}')
SAS_IP=$(nslookup $SAS_SERVER|awk "x==1 {print \$3}/$SAS_SERVER/{x=1}"|grep -v ":" | sort)
if [ -z "$SAS_IP" ]; then
  logErr "Failed to nslookup SAS server[$SAS_SERVER], re-check in 30s"
  /usr/bin/one_shot_timer -d -a 3000 -x "$SCRIPT"
  exit
fi

# include static route to TR069 server via CBRSSAS apn if requires
TR069_ON_SAS_APN=$(rdb_get sas.config.tr069_on_sas_apn)
tr069_server=$(rdb_get tr069.server.url|awk -F[/:] '{print $4}')
if [ -n "$tr069_server" -a "$TR069_ON_SAS_APN" = "1" ]; then
  TR069_IP=$(nslookup $tr069_server|awk "x==1 {print \$3}/$tr069_server/{x=1}"|grep -v ":" | sort)
fi

# get first index for sas
index=0
while true; do
  value=$(rdb_get service.router.static.route.${index})
  if [ -z "$value" -o -n "$(echo $value|grep "sas-")" ]; then break; fi
  index=$((index+1))
done

# update static route list
WHITELIST="$SAS_IP $TR069_IP"
for ip in $WHITELIST ; do
  now=$(rdb_get service.router.static.route.$index)
  new="sas-$index,$ip,255.255.255.255,,0,$NETIF"
  if [ "$now" != "$new" ]; then
    rdb_set service.router.static.route.$index "sas-$index,$ip,255.255.255.255,,0,$NETIF"
  fi
  if [ -z "$(ip route|grep $ip)" ]; then trigger=1; fi
  index=$((index+1))
done

if [ "$trigger" = "1" ]; then rdb_set service.router.static.route.trigger ""; fi

# update remote white list
now="$(rdb_get admin.remote.whitelist)"
new="$(echo $now $WHITELIST|tr ' ' '\n'|sort|uniq|tr '\n' ' ')"
if [ "$now" != "$new" ]; then
  rdb_set admin.remote.whitelist "$new"
fi

# to inform SAS client
CBRS_CONFIG_STAGE=2
if [ "$(rdb_get sas.cbrs_up_config_stage)" != "$CBRS_CONFIG_STAGE" ]; then
  rdb_set sas.cbrs_up_config_stage $CBRS_CONFIG_STAGE
fi

# set the timer to re-check after 2 min (or 12000 deci-sec)
/usr/bin/one_shot_timer -d -a 12000 -x "$SCRIPT"

# supervising sas client: restart if any error in the monitor list perists more than the threshold limit
SAS_ERROR=$(rdb_get "sas.error")
if [ -n "$SAS_ERROR" ]; then
  TS=$(rdb_get "sas.error.ts")
  NOW=$(date +%s)
  DURATION=$(( $NOW - $TS ))
  THRESHOLD=$(rdb_get sas.error.threshold)
  if [ -n "$THRESHOLD" ] && [ $DURATION -gt $THRESHOLD ]; then
    for e in $(rdb_get sas.error.monitor) ; do
      if [ "$e" == "$SAS_ERROR" ]; then
        rdb_set "sas.error"
        logNotice "restart SAS client due to error:$e since:$TS now:$NOW duration:$DURATION THRESHOLD:$THRESHOLD"
        /etc/init.d/rc.d/sas_client restart
        break
      fi
    done
  fi
fi


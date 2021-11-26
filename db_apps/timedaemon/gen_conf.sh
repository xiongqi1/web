#!/bin/sh

# Copyright NetComm Wireless Limited, Oct 2012.
# Written by Matthew Forgie.
#
# This script generates a config for timedaemon, taking into account V_Variable 
# settings.
#

cat << EOF > timedaemon.conf
# time daemon configuration

#
# date    : YYYY/MM/DD HH:MM:SS ex) 2010/05/10 23:59:59 / local time
# mobile  : different from the current time but time-zone may be incorrect ex) -226396sec 
# gps     : DD.MM.YY HH:MM:SS ex) 01/01/12 23:59:59 / UTC time
# network : YYYY-MM-DD HH:MM:SS ex) 2015-11-16 17:50:39 / UTC time
# browser : YYYY.MM.DD-HH.MM ex) 2012.05.17-05:33 / UTC time

# * accuracy and priority
#  The better accurate time source overrides the less accurace time sources. Upper located sources have high priority when the time sources 
# offer the exact same accuracy. All sources are checked by their higher priority time sources. If any time source moves out of
# their accuracies, the time source will be completedly ignored until it comes back into the accuracy range.
#
# * system clock error
# timedaemon assumes the system time error grows up 1 sec a day. One day after syncing with gps(1sec), the system clock becomes
# available to sync any time source that has better than 2sec accuracy (ntp and gps)
#

# source name	/ rdb variable name						/ accuracy(ms)	/ type	/ min. read

EOF

# Only include ntp & mobile if V_NTP_SERVER is enabled
if [ "$1" = "V_NTP_SERVER" ]; then
cat << EOF >> timedaemon.conf
ntp		system.ntp.time							1000		ntp	1	# ntp
EOF
	exit 0
fi

# Only include GPS time sources if V_GPS is enabled
if [ "$1" = "V_GPS" ]
then
cat << EOF >> timedaemon.conf
gps/standalone	sensors.gps.0.standalone.date:sensors.gps.0.standalone.time	1000		gps	60	# stand alone ( every 1 sec)
ntp		system.ntp.time							1000		ntp	1	# ntp
gps/assisted	sensors.gps.0.assisted.date:sensors.gps.0.assisted.time		30000		gps	1	# assist ( every 600 sec by default)
EOF
else
echo "ntp		system.ntp.time							1000		ntp	1	# ntp" >> timedaemon.conf
fi

cat << EOF >> timedaemon.conf
mobile		wwan.0.modem_date_and_time					30000		mobile	5	# phone network (every 10 second)
network         wwan.0.networktime.datetime                                     50000           network 1       # network time (every attach to network)
browser		system.browser.time						60000		browser	1	# browser time (every login)
saved		system.saved.timeDate					2147483646	saved		# saved time (every month=2592000000ms=30*24*60*60*1000) (too big for a 32 bit int, 2147483646ms=~25 days will be used)
EOF

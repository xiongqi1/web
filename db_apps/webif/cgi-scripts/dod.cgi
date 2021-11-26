#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "dod.cgi" -- "$@"
}

# splits CGI query into var="value" strings
cgi_split() {
	echo "$1" | awk 'BEGIN{
		hex["0"] =  0; hex["1"] =  1; hex["2"] =  2; hex["3"] =  3;
		hex["4"] =  4; hex["5"] =  5; hex["6"] =  6; hex["7"] =  7;
		hex["8"] =  8; hex["9"] =  9; hex["A"] = 10; hex["B"] = 11;
		hex["C"] = 12; hex["D"] = 13; hex["E"] = 14; hex["F"] = 15;
	}
	{
		n=split ($0,EnvString,"&");
		for (i = n; i>0; i--) {
			z = EnvString[i];
			x=gsub(/\=/,"=\"",z);
			x=gsub(/\+/," ",z);
			while(match(z, /%../)){
				if(RSTART > 1)
					printf "%s", substr(z, 1, RSTART-1)
				printf "%c", hex[substr(z, RSTART+1, 1)] * 16 + hex[substr(z, RSTART+2, 1)]
				z = substr(z, RSTART+RLENGTH)
			}
			x=gsub(/$/,"\"",z);
			print z;
		}
	}'
}

qlist=`cgi_split "$QUERY_STRING"`

split() {
	shift $1
	echo "$1"
}

# Get the device node and name (but don't get any other params yet!)
while read V; do
	VAR="CGI_PARAM_$V"
	SEP="$(echo "$VAR" | tr '=' ' ')"
	NAME="$(split 1 $SEP)"
	VAL="'$(split 2 $SEP)'"

	if [ "$NAME" = "CGI_PARAM_cmd" ]; then
		eval $VAR
	fi

	if [ "$NAME" = "CGI_PARAM_opt1" ]; then
		eval $VAR
	fi

	if [ "$NAME" = "CGI_PARAM_opt2" ]; then
		eval $VAR
	fi

	if [ "$NAME" = "CGI_PARAM_opt3" ]; then
		eval $VAR
	fi

done << EOF
$qlist
EOF

encodeJSON() {
	arg="$1"
	i=0
	while [ $i -lt ${#arg} ]; do c=${arg:$i:1}; printf '\u%04X' "'$c'"; i=$((i+1)); done
}

uri_decode() {
	arg="$1"
	i="0"
	while [ "$i" -lt ${#arg} ]; do
		c0=${arg:$i:1}
		if [ "x$c0" = "x%" ]; then
			c1=${arg:$((i+1)):1}
			c2=${arg:$((i+2)):1}
			printf "\x$c1$c2"
			i=$((i+3))
		else
			echo -n "$c0"
			i=$((i+1))
		fi
	done
}

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteReply() {
	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: text/plain\n"
	htmlWrite "Cache-Control: no-cache\n"
	htmlWrite "Connection: keep-alive\n\n"
}


list_activated_wwan_profile() {
	rdb_get -L "link.profile." | sed -n 's/^link\.profile\.\([0-9]\+\).dev[[:space:]]\+wwan.[0-9]\+$/\1/p' | sort | while read pf; do
		profile_en=$(rdb_get "link.profile.${pf}.enable")
		def_profile=$(rdb_get "link.profile.${pf}.defaultroute")

		if [ "$profile_en" = "1" -a "$def_profile" = "1" ]; then
			echo -n "$pf,"
		fi
	done
}

rdb_get_def() {
	r=$(rdb_get "$1")
	if [ -n "$r" ]; then
		echo "$r"
	else
		echo "$2"
	fi
}

dod_get() {

	eval $(rdb_get -L "dialondemand." | sed -n 's/^dialondemand\.\([^\.]*\)[[:space:]]\+\(.*\)/\1="\2"/p')
	
	test -z "$deactivation_timer" && deactivation_timer=0
	test -z "$dial_delay" && dial_delay=0
	test -z "$dod_verbose_logging" && dod_verbose_logging=0
	test -z "$enable" && enable=0
	test -z "$ignore_dns" && ignore_dns=0
	test -z "$ignore_icmp" && ignore_icmp=0
	test -z "$ignore_ntp" && ignore_ntp=0
	test -z "$ignore_tcp" && ignore_tcp=0
	test -z "$ignore_udp" && ignore_udp=0
	test -z "$ignore_win7" && ignore_win7=0
	test -z "$min_online" && min_online=5
	test -z "$periodic_online" && periodic_online=0
	test -z "$periodic_online_random" && periodic_online_random=30
	test -z "$ports_en" && ports_en=0
	test -z "$ports_list" && ports_list=""
	test -z "$poweron" && poweron=0
	test -z "$traffic_online" && traffic_online=20

	if [ -z "$profile" ]; then
		profile="1"
		enable="0"
	fi

	# print start
	cat << EOF
{
	"cgiresult":0,
	"dod_profile":{
		"profile":$profile,
		
		"deactivation_timer":$deactivation_timer,
		"dial_delay":$dial_delay,
		"dod_verbose_logging":$dod_verbose_logging,
		"enable":$enable,
		"ignore_dns":$ignore_dns,
		"ignore_icmp":$ignore_icmp,
		"ignore_ntp":$ignore_ntp,
		"ignore_tcp":$ignore_tcp,
		"ignore_udp":$ignore_udp,
		"ignore_win7":$ignore_win7,
		"min_online":$min_online,
		"periodic_online":$periodic_online,
		"periodic_online_random":$periodic_online_random,
		"ports_en":$ports_en,
		"ports_list":"$ports_list",
		"poweron":$poweron,
		"traffic_online":$traffic_online
	},
	"profiles":[$(list_activated_wwan_profile | sed 's/,$//g')]
}
EOF
}

case "$CGI_PARAM_cmd" in
	'dod_get')
		htmlWriteReply
		dod_get
		;;

	*)
		log "cmd='$CGI_PARAM_cmd', opt1='$CGI_PARAM_opt1', opt2='$CGI_PARAM_opt2', opt3='$CGI_PARAM_opt3'"
		exit 1
		;;
esac

exit 0

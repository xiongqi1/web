#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "roaminglist.cgi" -- "$@"
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

rdb_get_def() {
	r=$(rdb_get "$1")
	if [ -n "$r" ]; then
		echo "$r"
	else
		echo "$2"
	fi
}

customerList="/usr/local/cdcs/conf/plmn-*.csv"
factoryList="/etc/cdcs/conf/plmn-*.csv.factory"


sed_imsi_range() {
	sed -n 's/.*plmn-\([0-9]\+\)\.csv.*$/\1/p'
}

get_roaminglist()
{
found_inuse="0"
imsi_range=$(rdb_get "manualroam.current_PRL_imsi_range")
plmn_version=$(rdb_get "manualroam.current_PRL_version")

cat << EOF
{
	"roaminglist": [
EOF

for filename in $(ls $customerList 2> /dev/null)
do
	range="$(echo "$filename" | sed_imsi_range)"
	version="$(grep -m1 Version: "$filename"| awk -F'[][]' '{print $2}' | sed -n 's/Version:[ /t]*//p')"

	if [ "$found_inuse" = "0" -a "$imsi_range" = "$range" -a "$plmn_version" = "$version" ];
	then
		found_inuse="1"
		echo "{\"inuse\":\"1\", \"range\":\"$range\", \"version\":\"$version\", \"filename\":\"$filename\", \"isfactory\":\"0\"},"
	else
		echo "{\"inuse\":\"0\", \"range\":\"$range\", \"version\":\"$version\", \"filename\":\"$filename\", \"isfactory\":\"0\"},"
	fi
done

for filename in $(ls $factoryList 2> /dev/null)
do
	range="$(echo "$filename" | sed_imsi_range)"
	version="$(grep -m1 Version: "$filename"| awk -F'[][]' '{print $2}' | sed -n 's/Version:[ /t]*//p')"

	if [ "$found_inuse" = "0" -a "$imsi_range" = "$range" -a "$plmn_version" = "$version" ];
	then
		found_inuse="1"
		echo "{\"inuse\":\"1\", \"range\":\"$range\", \"version\":\"$version\", \"filename\":\"$filename\", \"isfactory\":\"1\"},"
	else
		echo "{\"inuse\":\"0\", \"range\":\"$range\", \"version\":\"$version\", \"filename\":\"$filename\", \"isfactory\":\"1\"},"
	fi
done

cat << EOF
		{}
	]
}
EOF

}

case "$CGI_PARAM_cmd" in
	'getlist')
		htmlWriteReply
		get_roaminglist
		;;

	'deletelist')
		htmlWriteReply
		if [ -n "$CGI_PARAM_opt1" -a -f "$CGI_PARAM_opt1" ]
		then
			rm -f "$CGI_PARAM_opt1"
		fi

		cgiresult=$?

		cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
		;;

	*)
		log "cmd='$CGI_PARAM_cmd', opt1='$CGI_PARAM_opt1', opt2='$CGI_PARAM_opt2', opt3='$CGI_PARAM_opt3'"
		exit 1
		;;
esac

exit 0

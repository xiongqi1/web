#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "rdb_tool.cgi" -- "$@"
}

# bypass if it is from no logged session
if [ "$SESSION_ID" !=  "$sessionid" ]; then
	exit 1
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
fi
QUERY_STRING=$(echo $QUERY_STRING | sed "s/^csrfTokenGet=[a-zA-z0-9]\+&//")

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

	# take all params starting with CGI_PARAM_opt
	if [ -n "$NAME" -a "${NAME/CGI_PARAM_*/}" = "" ]; then
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

rdb_tool_get() {

	rdb_var=$(uri_decode "$CGI_PARAM_opt1")

#log << EOF
#cmd='rdb_get'
#rdb_var='$rdb_var'
#EOF

	rdb_val=$(rdb_get "$rdb_var")
	cgiresult=$?
	
	rdb_val_json=$(encodeJSON "$rdb_val")

	cat << EOF
{
	"val":"$rdb_val_json",
	"cgiresult":$cgiresult
}
EOF
	return 0
}

rdb_tool_mget() {

	i=0

	# print begining
	cat << EOF
{
EOF
	
	while true; do
	
		# get rdb variable from cgi opt
		i=$((i+1))
		rdb_var=$(eval uri_decode "\$CGI_PARAM_opt${i}")
		
		# break if no more option
		if [ -z "$rdb_var" ]; then
			break;
		fi
		
		# get rdb val
		rdb_val=$(rdb_get "$rdb_var")
		# json encode
		rdb_val_json=$(encodeJSON "$rdb_val")

#log << EOF
#cmd='rdb_mget'
#rdb_var='$rdb_var'
#rdb_val='$rdb_val'
#EOF
		
		# print var and val
		cat << EOF
	"$rdb_var":"$rdb_val_json",
EOF

	done
		# print ending
		cat << EOF
	"cgiresult":0
}
EOF
}		

rdb_tool_mset() {

	i=0

	# get rdb flag
	rdb_flag="$CGI_PARAM_flag"
	
	while true; do
	
# get rdb variable from cgi opt
		i=$((i+1))
		rdb_var=$(eval uri_decode "\$CGI_PARAM_opt${i}")
		i=$((i+1))
		rdb_val=$(eval uri_decode "\$CGI_PARAM_opt${i}")
		
# break if no more option
		if [ -z "$rdb_var" ]; then
			break;
		fi
		

#log << EOF
#cmd='rdb_mget'
#rdb_var='$rdb_var'
#rdb_val='$rdb_val'
#EOF
		# set rdb
		rdb_set $rdb_flag "$rdb_var" "$rdb_val"
	done
	
	# print ending
	cat << EOF
{	
	"cgiresult":0
}
EOF
}		

rdb_tool_set () {
	rdb_var=$(uri_decode "$CGI_PARAM_opt1")
	rdb_val=$(uri_decode "$CGI_PARAM_opt2")
	rdb_opt=$(uri_decode "$CGI_PARAM_opt3")

#log << EOF
#cmd='rdb_set'
#rdb_var='$rdb_var'
#rdb_val='$rdb_val'
#rdb_opt='$rdb_opt'
#EOF

	if [ -n "$rdb_opt" ]; then
		rdb_set "$rdb_opt" "$rdb_var" "$rdb_val"
	else
		rdb_set "$rdb_var" "$rdb_val"
	fi
	cgiresult=$?

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
	return 0
}


case "$CGI_PARAM_cmd" in
	'rdb_get')
		htmlWriteReply
		rdb_tool_get
		;;
		
	'rdb_mget')
		htmlWriteReply
		rdb_tool_mget
		;;

	'rdb_set')
		htmlWriteReply
		rdb_tool_set
		;;

	'rdb_mset')
		htmlWriteReply
		rdb_tool_mset
		;;

	*)
		log "cmd='$CGI_PARAM_cmd', opt1='$CGI_PARAM_opt1', opt2='$CGI_PARAM_opt2', opt3='$CGI_PARAM_opt3'"
		exit 1
		;;
esac

exit 0

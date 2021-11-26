#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

. /etc/variant.sh 2>/dev/null
if [ "$V_WEBIF_VERSION" = "v2" ]; then
	# CSRF token must be valid
	if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
		exit 254
	fi
fi
QUERY_STRING=$(echo $QUERY_STRING | sed "s/^csrfTokenGet=[a-zA-z0-9]\+&//")

script=$(basename "$0")
log() {
	logger -t "$script" -- "$@"
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
for V in $qlist; do
	VAR="CGI_PARAM_$V"
	SEP="$(echo "$VAR" | tr '=' ' ')"
	NAME="$(split 1 $SEP)"
	VAL="'$(split 2 $SEP)'"

	if [ "$NAME" = "CGI_PARAM_reqtype" ]; then
		eval $VAR
	fi
	

done

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteReply() {
	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: text/plain\n"
	htmlWrite "Cache-Control: no-cache\n"
	htmlWrite "Connection: keep-alive\n\n"
}


cgi_disconnect() {
	log "force to disconnect [dod]"

	diald-ctrl.sh "down" 2> /dev/null > /dev/null
	cgiresult=$?

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF

}

cgi_connect() {
	log "force to connect wwan [dod]"

	diald-ctrl.sh "up" 2> /dev/null > /dev/null
	cgiresult=$?

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF

}

cgi_get_status() {
	# get pdp session status
	dod_status=$(rdb_get "dialondemand.status")
	if [ "$dod_status" != "1" ]; then
		dod_status="0"
	fi
	
	# get dod enable status
	dod_en=$(rdb_get "dialondemand.enable")
	if [ "$dod_en" != "1" ]; then
		dod_en="0"
	fi
	

	cgiresult=0
	

	cat << EOF
{
	"status":$dod_status,
	"dod_enable":$dod_en,
	"cgiresult":$cgiresult
}
EOF
	
}

case "$CGI_PARAM_reqtype" in
	'get_status'|'connect'|'disconnect')
		htmlWriteReply
		cgi_${CGI_PARAM_reqtype}
		;;

	*)
		exit 1
		;;
esac

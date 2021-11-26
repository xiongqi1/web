#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

#
# reqtype 
# reqparam
#

log() {
	logger -t 'ltph.cgi' -- "$@"
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

	if [ "$NAME" = "CGI_PARAM_reqparam" ]; then
		eval $VAR
	fi

done

log "reqtype='$CGI_PARAM_reqtype', reqparam='$CGI_PARAM_reqparam'"

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteReply() {
	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: text/plain\n"
	htmlWrite "Cache-Control: no-cache\n"
	htmlWrite "Connection: keep-alive\n\n"
}

cgi_ping() {

	ping -c 1 "$1" 2> /dev/null > /dev/null
	cgiresult=$?
	
	log "cgiresult : $cgiresult"

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
}

htmlWriteReply

case "$CGI_PARAM_reqtype" in

	'ping')
		cgi_ping "$CGI_PARAM_reqparam"
		;;

	*)
		log "unknown reqtype - '$CGI_PARAM_reqtype'"
		return 1
		;;
	
esac

return 0

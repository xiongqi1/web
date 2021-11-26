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
			#x=gsub(/$/,"\"",z);
			print z;
		}
	}'
}

QUERY_STRING=$(echo $QUERY_STRING | sed "s/^&csrfTokenGet=[a-zA-z0-9]\+&//")
qlist=`cgi_split "$QUERY_STRING"`

# Get the device node and name (but don't get any other params yet!)
echo -e "Status: 200\nContent-type: text/plain\nPragma: no-cache\nCache-Control: no-cache\n\n"

for V in $qlist; do
	val=$(rdb_get $V)
	var=$(echo $V | sed "s/\./_/g")
	echo "$var=$val"
	#log "echo $var=\"$val\";"
done

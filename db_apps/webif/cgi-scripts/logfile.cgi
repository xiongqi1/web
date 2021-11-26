#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	# TODO: enforce on all html files that use this cgi
	if [ "$action" = "downloadDiagnosticlog" -o "$csrfTokenGet" != "" ]; then
		exit 254
	fi
fi

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
			x=gsub(/$/,"\"\n",z);
			print z;
		}
	}'
}

log() {
	echo -e "$@" | logger -t "logfile.cgi"
}

split() {
	shift $1
	echo "$1"
}



while read v; do
	if [ -z "$v" ]; then
		continue
	fi

	VAR="logfile_cgi_$v"

	# do not accept anything else
	if echo "$VAR" | grep "^logfile_cgi_action=" || echo "$VAR" | grep "^logfile_cgi_param="; then
		eval $VAR
	fi
done << EOF
$(cgi_split "$QUERY_STRING")
EOF

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteLog() {
	htmlWrite "$@"
	log "$@"
}

htmlCatFrom() {
	source="$1"
	filename="$2"
	gzip=0

	if [ -z "$filename" ]; then
		filename=$(basename "$source")
	fi

	if [ ! -r "$source" ]; then
		htmlWriteLog "cannot access file \'$source\': Permission denied"
		return 1;
	fi

	realfn=$(readlink -f "$1")
	file_size=$(stat -c %s "$realfn")

	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: application/download\n";
	htmlWrite "Content-transfer-encodig: binary\n";
	if [ "${ACCEPT_ENCODING/gzip/}" != "$ACCEPT_ENCODING" ]; then # gzip encoding supported
		htmlWrite "Content-Encoding: gzip\n";
		gzip=1;
	fi
	htmlWrite "Content-disposition: attachment; filename=\"$filename\"\n";
	htmlWrite "Connection: close\n\n"

	if [ $gzip -eq 1 ]; then
		cat $source | gzip;
		ret=$?;
	else
		cat $source;
		ret=$?
	fi
	if [ $ret -ne 0 ]; then
		htmlWriteLog "cannot cat file \'$source\': Return code $ret"
		return 1;
	fi

	return 0
}

. /etc/variant.sh

prefixHeaders() {
	echo -n -e "Status: 200\n"
	echo -n -e "Content-type: text/plain\n"
	echo -n -e "Cache-Control: no-cache\n"
	echo -n -e "Connection: keep-alive\n\n"
}

# merge either 1 or 2 log files to a single common file
generateFullLog() {
	FULLLOGFILE="$1"
	LOGFILE_PART="$2"

	if [ -f "$LOGFILE_PART".0 ]; then
		cat "$LOGFILE_PART".0 "$LOGFILE_PART" > "$FULLLOGFILE"
	else
		cat "$LOGFILE_PART" > "$FULLLOGFILE"
	fi
}

case "$logfile_cgi_action" in
	'downloadlog')
		logToFile=$(rdb_get service.syslog.option.logtofile)
		if [ "$logToFile" = "1" ]; then
			# we need to generate the full log on-the-fly for download since merge_logfile.sh might only produces a truncated version
			if [ "$V_SYSLOG_STYLE" = "generic" ]; then
				LOGFILE="/var/log/messages.full"
				LOGFILEBASE="/var/log/messages"
			else
				LOGFILE="/opt/messages.full"
				LOGFILEBASE="/opt/messages"
			fi
			generateFullLog "$LOGFILE" "$LOGFILEBASE"
			htmlCatFrom "$LOGFILE" "logfile.log"
			rm -f "$LOGFILE" > /dev/null 2>&1
		else
			if [ "$V_WEBIF_VERSION" = "v1" ]; then
				SOFTLINK="/www/logfile.txt"
			else
				SOFTLINK="/etc/logfile.txt"
			fi
			LOGFILE="/tmp/logfile.log"
			cp -f $SOFTLINK $LOGFILE
			htmlCatFrom "$LOGFILE";
			rm $LOGFILE
		fi
		;;
	'downloadDiagnosticlog')
		LOGFILE="/tmp/diagnosticlog_$(date +%d%m%y_%H%M%S)_$$.zip"
		LOGDIR=$(rdb_get system.diagnostic.log.dir)
		if [ -d $LOGDIR ]; then
			(cd $LOGDIR; zip -r $LOGFILE .)
		fi
		htmlCatFrom $LOGFILE
		rm $LOGFILE
		;;
	'downloadIPseclog')
		if [ "$V_WEBIF_VERSION" = "v1" ]; then
			SOFTLINK="/www/ipseclog.txt"
		else
			SOFTLINK="/etc/ipseclog.txt"
		fi
		LOGFILE="/tmp/ipseclog.log"
		cp -f $SOFTLINK $LOGFILE
		htmlCatFrom "$LOGFILE";
		rm $LOGFILE
		;;
	'getIPseclog')
		if [ "$V_WEBIF_VERSION" = "v1" ]; then
			SOFTLINK="/www/ipseclog.txt"
		else
			SOFTLINK="/etc/ipseclog.txt"
		fi
		LOGFILE="/tmp/ipseclog.log"
		cp -f $SOFTLINK $LOGFILE
		prefixHeaders;
		cat $LOGFILE
		rm $LOGFILE
		;;
	'downloadMib')
		LOGFILE="/www/snmp.mib"
		htmlCatFrom "$LOGFILE";
		;;
	'downloadEvtLog')
		LOGFILE=`rdb_get service.eventnoti.conf.log_file`
		htmlCatFrom "$LOGFILE";
		;;
	'getEvtLog')
		LOGFILE=`rdb_get service.eventnoti.conf.log_file`
		prefixHeaders;
		cat "$LOGFILE" 2>/dev/null;
		;;
	*)
		exit 1
		;;
esac

exit 0

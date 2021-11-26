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
QUERY_STRING=$(echo $QUERY_STRING | sed "s/^&csrfTokenGet=[a-zA-z0-9]\+&//")

OPENVPN_KEY_DEFAULT_DIR=/usr/local/cdcs/openvpn-keys

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
			x=gsub(/\+/," ",z);
			while(match(z, /%../)){
				if(RSTART > 1)
					printf "%s", substr(z, 1, RSTART-1)
				printf "%c", hex[substr(z, RSTART+1, 1)] * 16 + hex[substr(z, RSTART+2, 1)]
				z = substr(z, RSTART+RLENGTH)
			}
			print z;
		}
	}'
}

log() {
	echo -e "$@" | logger -t "vpn_action.cgi" 
}

logpipe() {
	logger -t "vpn_action.cgi" 
}


split() {
	shift $1
	echo "$1"
}

while read v; do
	if [ -z "$v" ]; then
		continue
	fi
	
	VAR="vpn_cgi_$v"
	
	# do not accept anything else
	if echo "$VAR" | grep "^vpn_cgi_action=" || echo "$VAR" | grep "^vpn_cgi_param="; then
		key="${VAR%=*}"
		val="${VAR#*=}"
		export "${key}"="${val}"
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

htmlReadUntilBlank() {
	while read line; do
		if ! echo "$line" | tr -d '\r' | grep -q "^$"; then
			continue
		fi
		
		return 0
	done
	
	return 1
}

htmlParsePostData() {
	while read line; do
		if echo "$line" | grep -q "Content-Disposition: .* filename="; then
			continue
		elif echo "$line" | grep -q "Content-Type: "; then
			htmlReadUntilBlank
			return 0
		elif echo "$line" | grep -q "Content-Disposition: form-data; name="; then
			variable=$(echo "$line" | sed -n 's/.* name="\([a-zA-Z_]*\)".*/\1/p')
			
			htmlReadUntilBlank

			read line
			value=$(echo "$line" | tr -d '\r')
			
			log "Disposition: $variable=$value detected"
			
			case "$variable" in
				'subaction')
					vpn_cgi_subaction="$value"
					;;
					
				'param')
					vpn_cgi_param="$value"
					;;
					
				*)
					;;
			esac
		fi
			
	done
	
	return 1
}

htmlCatTo() {
	target="$1"
	
	# download - max 64kb
	head -c $((64*1024*1024)) > "$target"
	
	return $?
}

htmlCatFrom() {
	source="$1"
	filename="$2"
	
	if [ -z "$filename" ]; then
		filename=$(basename "$source")
	fi
	
	if [ ! -r "$source" ]; then
		htmlWriteLog "cannot access file \'$source\': Permission denied"
		return 1;
	fi
	
	file_size=$(stat -c %s "$1")
	
	log "filename=$filename , file_size=$file_size"

	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: application/download\n";
	htmlWrite "Content-length: $file_size\n";
	htmlWrite "Content-transfer-encodig: binary\n";
	htmlWrite "Content-disposition: attachment; filename=\"$filename\"\n";
	htmlWrite "Connection: close\n\n"

	if ! cat "$source"; then
		htmlWriteLog "cannot cat file \'$source\': Return code $?"
		return 1;
	fi
	
	return 0
}


#log "launching - action=$vpn_cgi_action,param=$vpn_cgi_param"

case "$vpn_cgi_action" in
	'init')
		htmlWrite "Content-Type: text/html\n\n"
		keySize=$(echo "$vpn_cgi_param" | cut -d , -f 1)
		test -n "$keySize" && rdb_set service.openvpn.keysize "$keySize"
		log "init DH key..."
		if ! openvpn_keygen.sh "init"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		log "DH success"

		htmlWrite "var result='ok';\n"
		;;
		
	'delsecret')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "deleting secret key..."
		if ! openvpn_keygen.sh "delsecret" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi

		htmlWrite "var result='ok';\n"
		;;

	'deltlsauth')
		htmlWrite "Content-Type: text/html\n\n"

		log "deleting tls auth key..."
		if ! openvpn_keygen.sh "deltlsauth" $vpn_cgi_param 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi

		htmlWrite "var result='ok';\n"
		;;

	'dnsecret')
		SECRETKEY_FILENAME="$OPENVPN_KEY_DEFAULT_DIR/server/secret.key"
		log "downloading... $SECRETKEY_FILENAME"
		if ! htmlCatFrom "$SECRETKEY_FILENAME"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;

	'dntlsauth')
		SECRETKEY_FILENAME="$OPENVPN_KEY_DEFAULT_DIR/server/ta.key"
		log "downloading... $SECRETKEY_FILENAME"
		if ! htmlCatFrom "$SECRETKEY_FILENAME"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;

	'gensecret')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "genereate secret key..."
		if ! openvpn_keygen.sh "gensecret" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
	
		htmlWrite "var result='ok';\n"
		;;

	'gentlsauth')
		htmlWrite "Content-Type: text/html\n\n"

		log "genereate tls auth key..."
		if ! openvpn_keygen.sh "gentlsauth" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi

		htmlWrite "var result='ok';\n"
		;;

	'ca')
		htmlWrite "Content-Type: text/html\n\n"
		
		COMMON_NAME=$(echo "$vpn_cgi_param" | cut -d , -f 1)
		KEY_COUNTRY=$(echo "$vpn_cgi_param" | cut -d , -f 2)
		KEY_PROVINCE=$(echo "$vpn_cgi_param" | cut -d , -f 3)
		KEY_CITY=$(echo "$vpn_cgi_param" | cut -d , -f 4)
		KEY_ORG=$(echo "$vpn_cgi_param" | cut -d , -f 5)
		KEY_EMAIL=$(echo "$vpn_cgi_param" | cut -d , -f 6)

		export KEY_COUNTRY KEY_PROVINCE KEY_CITY KEY_ORG KEY_EMAIL
		log "param = $COMMON_NAME / $KEY_COUNTRY / $KEY_PROVINCE / $KEY_CITY / $KEY_ORG $KEY_EMAIL"
		
		log "cleaning keys..."
		if ! openvpn_keygen.sh "clean" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed to clean - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		log "generating ca key..."
		if ! openvpn_keygen.sh "ca" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		log "generating server key..."
		if ! openvpn_keygen.sh "server" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
	
		htmlWrite "var result='ok';\n"
		
		log "ca done."
		;;
		
	'genclient')
		htmlWrite "Content-Type: text/html\n\n"
		
		COMMON_NAME=$(echo "$vpn_cgi_param" | cut -d , -f 1)
		KEY_COUNTRY=$(echo "$vpn_cgi_param" | cut -d , -f 2)
		KEY_PROVINCE=$(echo "$vpn_cgi_param" | cut -d , -f 3)
		KEY_CITY=$(echo "$vpn_cgi_param" | cut -d , -f 4)
		KEY_ORG=$(echo "$vpn_cgi_param" | cut -d , -f 5)
		KEY_EMAIL=$(echo "$vpn_cgi_param" | cut -d , -f 6)
		
		export KEY_COUNTRY KEY_PROVINCE KEY_CITY KEY_ORG KEY_EMAIL
		log "param = $COMMON_NAME / $KEY_COUNTRY / $KEY_PROVINCE / $KEY_CITY / $KEY_ORG / $KEY_EMAIL"
		
		if ! openvpn_keygen.sh "genclient" "$COMMON_NAME" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		htmlWrite "var result='ok';\n"
		;;

	'install')
		htmlWrite "Content-Type: text/html\n\n"

		# Remount rootfs as RW
		remount_partition.sh / rw

		if cp "$OPENVPN_KEY_DEFAULT_DIR/server/server.crt" /etc/appweb/ && cp "$OPENVPN_KEY_DEFAULT_DIR/server/server.key" /etc/appweb/ ; then
			log "appweb server key installed"
		else
			log "appweb server key installation has failed"
		fi

		# Remount rootfs as RO
		remount_partition.sh / ro

		htmlWrite "var result='ok';\n"
		;;

	'restart')
		htmlWrite "Content-Type: text/html\n\n"
		if [ -x /etc/init.d/rc.d/appweb.sh ]; then
			/etc/init.d/rc.d/appweb.sh restart
		else
			# assume appweb will be restarted by inittab
			killall -9 appweb
		fi
		exit 0
		;;

	'info')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "extracting information...."

		OPENVPN_TMP_FILE="/tmp/openvpn_action_tmp_$$"
		rm -f "${OPENVPN_TMP_FILE}_fifo"
		mkfifo "${OPENVPN_TMP_FILE}_fifo" 2>/dev/null >/dev/null
		(cat "${OPENVPN_TMP_FILE}_fifo" | logpipe; rm -f "${OPENVPN_TMP_FILE}_fifo")&
		
		if ! openvpn_keygen.sh "info" 2> "${OPENVPN_TMP_FILE}_fifo"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		htmlWrite "result='ok';\n"
		
		log "information successfully extracted."
		;;
		
	'info_dh')
		htmlWrite "Content-Type: text/html\n\n"
		
		OPENVPN_TMP_FILE="/tmp/openvpn_action_tmp_$$"
		rm -f "${OPENVPN_TMP_FILE}_fifo"
		mkfifo "${OPENVPN_TMP_FILE}_fifo" 2>/dev/null >/dev/null
		(cat "${OPENVPN_TMP_FILE}_fifo" | logpipe; rm -f "${OPENVPN_TMP_FILE}_fifo")&
		
		if ! openvpn_keygen.sh "info_dh" 2> "${OPENVPN_TMP_FILE}_fifo"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		
		htmlWrite "result='ok';\n"
		;;
		
	'dnclient')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.tgz"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.tgz"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;

	'dnclient2')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.p12"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.p12"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;

	'setclientnw')
		htmlWrite "Content-Type: text/html\n\n"
		
		COMMON_NAME=$(echo "$vpn_cgi_param" | cut -d , -f 1)
		NWADDR=$(echo "$vpn_cgi_param" | cut -d , -f 2)
		NWMASK=$(echo "$vpn_cgi_param" | cut -d , -f 3)
		
		if [ -n "$NWADDR" -a -n "$NWMASK" ]; then
			PUSHINFO="$NWADDR $NWMASK"
		else
			PUSHINFO=""
		fi

		log "cn = $COMMON_NAME / pushinfo = \"$PUSHINFO\""
		
		if ! openvpn_keygen.sh "setclientnw" "$COMMON_NAME" "$PUSHINFO" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		htmlWrite "result='ok';\n"
		;;
	
	'rmclient')
		htmlWrite "Content-Type: text/html\n\n"
		if ! openvpn_keygen.sh "rmclient" "$vpn_cgi_param" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		htmlWrite "result='ok';\n"
		;;
	
	'delclient')
		htmlWrite "Content-Type: text/html\n\n"
		
		if ! openvpn_keygen.sh "delclient" "$vpn_cgi_param" 2>&1 | logpipe; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		htmlWrite "result='ok';\n"
		;;
	

	'dnca2')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/ca.crt"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/ca.crt"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;
		
	'dnca')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/ca.tgz"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/ca.tgz"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 0
		fi
		;;
		
	'upload')
		htmlParsePostData
		
		log "POST PARAM: vpn_cgi_subaction=$vpn_cgi_subaction, vpn_cgi_param=$vpn_cgi_param"
		
		OPENVPN_TMP_FILE="/tmp/openvpn_action_tmp_$$"
		
		case $vpn_cgi_subaction in
			'upsecret')
				log "uploading secret... $OPENVPN_TMP_FILE"
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				
				log "installing secret... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instsecret "$OPENVPN_TMP_FILE" 2>&1 | logpipe; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				;;

			'uptlsauth')
				log "uploading tls auth secret... $OPENVPN_TMP_FILE"
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi

				log "installing tls auth secret... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh insttlsauth "$OPENVPN_TMP_FILE" "$vpn_cgi_param" 2>&1 | logpipe; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				;;
			'upca')
				log "uploading ca... $OPENVPN_TMP_FILE"
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				
				log "installing ca... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instca "$OPENVPN_TMP_FILE" "$vpn_cgi_param" 2>&1 | logpipe; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				;;
				
			'upclient')
				OPENVPN_TMP_FILE="/tmp/openvpn_action_tmp_$$"
				log "uploading clientkey... "
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi
				
				log "installing clientkey... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instclient "$OPENVPN_TMP_FILE" 2>&1 | logpipe; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 0
				fi

				log "clientkey installed successfully... $OPENVPN_TMP_FILE"

				;;
		esac
		
		rm -f "$OPENVPN_TMP_FILE"
		
		htmlWrite "Content-Type: text/html\n\n"
		htmlWrite "var result='ok'\n"
		
		#htmlWrite "<html><head><script language=\"JavaScript\">\n"
		#htmlWrite "var result='ok'\n"
		#htmlWrite "</script></head></html>\n"
		;;
		
		
	*)
		exit 1
		;;
esac

exit 0

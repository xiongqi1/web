#!/bin/sh

QUERY_STRING="$@"

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
	echo -e "$@" | logger -t "TR069 vpn_action.sh" 
}

logpipe() {
	logger -t "TR069 vpn_action.sh" 
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

	htmlWrite "HTTP/1.0 200 OK\n"
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

		log "init DH key..."
		if ! openvpn_keygen.sh "init"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		log "DH success"

		htmlWrite "var result='ok';\n"
		;;
		
	'delsecret')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "deleting secret key..."
		if ! openvpn_keygen.sh "delsecret" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
	
		htmlWrite "var result='ok';\n"
		;;
		
	'dnsecret')
		SECRETKEY_FILENAME="$OPENVPN_KEY_DEFAULT_DIR/server/secret.key"
		log "downloading... $SECRETKEY_FILENAME"
		if ! htmlCatFrom "$SECRETKEY_FILENAME"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		;;
	
	'gensecret')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "genereate secret key..."
		if ! openvpn_keygen.sh "gensecret" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
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
		if ! openvpn_keygen.sh "clean" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed to clean - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		log "generating ca key..."
		if ! openvpn_keygen.sh "ca" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		log "generating server key..."
		if ! openvpn_keygen.sh "server" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
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
		
		if ! openvpn_keygen.sh "genclient" "$COMMON_NAME" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		htmlWrite "var result='ok';\n"
		;;
		
	'info')
		htmlWrite "Content-Type: text/html\n\n"
		
		log "extracting information...."

		rm -f "${OPENVPN_TMP_FILE_fifo}_fifo"
		mkfifo "${OPENVPN_TMP_FILE_fifo}_fifo" 2>/dev/null >/dev/null
		(cat "${OPENVPN_TMP_FILE_fifo}_fifo" | logpipe; rm -f "${OPENVPN_TMP_FILE_fifo}_fifo")&
		
		if ! openvpn_keygen.sh "info" 2> "${OPENVPN_TMP_FILE_fifo}_fifo"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		htmlWrite "result='ok';\n"
		
		log "information successfully extracted."
		;;
		
	'info_dh')
		htmlWrite "Content-Type: text/html\n\n"
		
		rm -f "${OPENVPN_TMP_FILE_fifo}_fifo"
		mkfifo "${OPENVPN_TMP_FILE_fifo}_fifo" 2>/dev/null >/dev/null
		(cat "${OPENVPN_TMP_FILE_fifo}_fifo" | logpipe; rm -f "${OPENVPN_TMP_FILE_fifo}_fifo")&
		
		if ! openvpn_keygen.sh "info_dh" 2> "${OPENVPN_TMP_FILE_fifo}_fifo"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		
		htmlWrite "result='ok';\n"
		;;
		
	'dnclient')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.tgz"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/$vpn_cgi_param.tgz"; then
			log "download failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
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
		
		if ! openvpn_keygen.sh "setclientnw" "$COMMON_NAME" "$PUSHINFO" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		htmlWrite "result='ok';\n"
		;;
	
	'rmclient')
		htmlWrite "Content-Type: text/html\n\n"
		if ! openvpn_keygen.sh "rmclient" "$vpn_cgi_param" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		htmlWrite "result='ok';\n"
		;;
	
	'delclient')
		htmlWrite "Content-Type: text/html\n\n"
		
		if ! openvpn_keygen.sh "delclient" "$vpn_cgi_param" > /dev/null 2>&1; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
		fi
		htmlWrite "result='ok';\n"
		;;
	

	'dnca')
		log "downloading... $OPENVPN_KEY_DEFAULT_DIR/server/ca.tgz"
		if ! htmlCatFrom "$OPENVPN_KEY_DEFAULT_DIR/server/ca.tgz"; then
			log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
			exit 1
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
					exit 1
				fi
				
				log "installing secret... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instsecret "$OPENVPN_TMP_FILE" > /dev/null 2>&1; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 1
				fi
				;;
				
			'upca')
				log "uploading ca... $OPENVPN_TMP_FILE"
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 1
				fi
				
				if [ -z "$vpn_cgi_param" ]; then
					log "upca requres param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 1
				fi
				
				log "installing ca... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instca "$OPENVPN_TMP_FILE" "$vpn_cgi_param" > /dev/null 2>&1; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 1
				fi
				;;
				
			'upclient')
				OPENVPN_TMP_FILE="/tmp/openvpn_action_tmp_$$"
				log "uploading clientkey... "
				if ! htmlCatTo "$OPENVPN_TMP_FILE"; then
					log "htmlCatTo failed - action=$vpn_cgi_action,param=$vpn_cgi_param"

					rm -f "$OPENVPN_TMP_FILE"
					exit 1
				fi
				
				log "installing clientkey... $OPENVPN_TMP_FILE"
				if ! openvpn_keygen.sh instclient "$OPENVPN_TMP_FILE" > /dev/null 2>&1; then
					log "openvpn_keygen.sh failed - action=$vpn_cgi_action,param=$vpn_cgi_param"
					
					rm -f "$OPENVPN_TMP_FILE"
					exit 1
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

#!/bin/sh
IPSEC_KEY_DIR="/etc/ipsec.d"
IPSEC_CERTS_DIR="$IPSEC_KEY_DIR/certs"
IPSEC_CACERTS_DIR="$IPSEC_KEY_DIR/cacerts"
IPSEC_CRLCERTS_DIR="$IPSEC_KEY_DIR/crls"
IPSEC_PRIVATEKEY_DIR="$IPSEC_KEY_DIR/private"
IPSEC_RSAKEY_DIR="$IPSEC_KEY_DIR/rsakey"

QUERY_STRING="$1"

if [ -z "$QUERY_STRING" ]; then
	echo "Need argument"
	exit 1
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
	echo -e "$@" | logger -t "ipsec_action.cgi" 
}

logpipe() {
	logger -t "ipsec_action.cgi" 
}


split() {
	shift $1
	echo "$1"
}

while read v; do
	if [ -z "$v" ]; then
		continue
	fi
	
	VAR="ipsec_cgi_$v"
	
	# do not accept anything else
	if echo "$VAR" | grep "^ipsec_cgi_action=" || echo "$VAR" | grep "^ipsec_cgi_param="; then
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
					ipsec_cgi_subaction="$value"
					;;
					
				'param')
					ipsec_cgi_param="$value"
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

htmlCatIpsecLog() {
	
	source="/var/log/ipseclog"
	filename=$(basename "$source")

	if [ ! -r "$source" ]; then
		htmlWriteLog "cannot access file \'$source\': Permission denied"
		return 1;
	fi
	
	file_size=$(stat -c %s "$source")
	
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


htmlCatPubKeyFrom() {
	source="$1"
	filename="$2"
	IPSEC_DN_PUBLIC_KEY_FILE="/tmp/leftpub.key"

	if [ -z "$filename" ]; then
		filename=$(basename "$source")
	fi
	
	if [ ! -r "$source" ]; then
		htmlWriteLog "cannot access file \'$source\': Permission denied"
		return 1;
	fi
	
	PUBLIC_KEY_SED_OPTION="-n /#pubkey=/,/PublicExponent:/p"
	sed -n /#pubkey=/,/PublicExponent:/p $source > $IPSEC_DN_PUBLIC_KEY_FILE
	
	file_size=$(stat -c %s "$IPSEC_DN_PUBLIC_KEY_FILE")
	
	log "filename=$filename , file_size=$file_size"

	htmlWrite "HTTP/1.0 200 OK\n"
	htmlWrite "Content-type: application/download\n";
	htmlWrite "Content-length: $file_size\n";
	htmlWrite "Content-transfer-encodig: binary\n";
	htmlWrite "Content-disposition: attachment; filename=\"$filename\"\n";
	htmlWrite "Connection: close\n\n"

	if ! cat "$IPSEC_DN_PUBLIC_KEY_FILE"; then
		htmlWriteLog "cannot cat file \'$source\': Return code $?"
		return 1;
	fi
	rm -f $IPSEC_DN_PUBLIC_KEY_FILE
	return 0
}


log "launching - action=$ipsec_cgi_action,param=$ipsec_cgi_param"

case "$ipsec_cgi_action" in

	'certinfo')
		htmlWrite "Content-Type: text/html\n\n"
		log "Checking certificates info..."
		
 		profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
		LOCAL_KEY_NAME="$IPSEC_PRIVATEKEY_DIR/local$profileNum.key"
		LOCAL_CERT_PEM_NAME="$IPSEC_CERTS_DIR/local$profileNum.pem"
		LOCAL_CERT_CRT_NAME="$IPSEC_CERTS_DIR/local$profileNum.crt"
		REMOTE_CERT_PEM_NAME="$IPSEC_CERTS_DIR/remote$profileNum.pem"
		REMOTE_CERT_CRT_NAME="$IPSEC_CERTS_DIR/remote$profileNum.crt"
		CA_CERT_PEM_NAME="$IPSEC_CACERTS_DIR/ca$profileNum.pem"
		CA_CERT_CRT_NAME="$IPSEC_CACERTS_DIR/ca$profileNum.crt"
		CRL_CERT_PEM_NAME="$IPSEC_CRLCERTS_DIR/crl$profileNum.pem"
		CRL_CERT_CRT_NAME="$IPSEC_CRLCERTS_DIR/crl$profileNum.crt"
		# Check local keys
		if [ -e $LOCAL_KEY_NAME ]; then
			if cat $LOCAL_KEY_NAME | grep -q "BEGIN RSA PRIVATE KEY"; then
				htmlWrite "IPSRsaKeyValid='ok';\n"
			else
				htmlWrite "IPSRsaKeyValid='wrongformat';\n"
			fi
		else
			htmlWrite "IPSRsaKeyValid='nofile';\n"
		fi
		
		# Check local certs
		if [ -e $LOCAL_CERT_PEM_NAME ]; then
			LOCAL_CERT_NAME=$LOCAL_CERT_PEM_NAME
		else
			LOCAL_CERT_NAME=$LOCAL_CERT_CRT_NAME
		fi

		if [ -e $LOCAL_CERT_NAME ]; then
			if cat $LOCAL_CERT_NAME | grep -q "BEGIN CERTIFICATE" ; then
				echo "IPSRsaLocalCertValid=\"ok\";"
			else
				echo "IPSRsaLocalCertValid=\"wrongformat\";"
			fi
		else
			echo "IPSRsaLocalCertValid=\"nofile\";"
		fi

		# Check remote certs
		if [ -e $REMOTE_CERT_PEM_NAME ]; then
			REMOTE_CERT_NAME=$REMOTE_CERT_PEM_NAME
		else
			REMOTE_CERT_NAME=$REMOTE_CERT_CRT_NAME
		fi

		if [ -e $REMOTE_CERT_NAME ]; then
			if cat $REMOTE_CERT_NAME | grep -q "BEGIN CERTIFICATE"; then
				echo "IPSRsaRemoteCertValid=\"ok\";"
			else
				echo "IPSRsaRemoteCertValid=\"wrongformat\";"
			fi
		else
			echo "IPSRsaRemoteCertValid=\"nofile\";"
		fi

		# Check CA certs
		if [ -e $CA_CERT_PEM_NAME ]; then
			CA_CERT_NAME=$CA_CERT_PEM_NAME
		else
			CA_CERT_NAME=$CA_CERT_CRT_NAME
		fi

		if [ -e $CA_CERT_NAME ]; then
			if cat $CA_CERT_NAME | grep -q "BEGIN CERTIFICATE" ; then
				echo "IPSRsaCACertValid=\"ok\";"
			else
				echo "IPSRsaCACertValid=\"wrongformat\";"
			fi
		else
			echo "IPSRsaCACertValid=\"nofile\";"
		fi

		# Check CRL certs
		if [ -e $CRL_CERT_PEM_NAME ]; then
			CRL_CERT_NAME=$CRL_CERT_PEM_NAME
		else
			CRL_CERT_NAME=$CRL_CERT_CRT_NAME
		fi

		if [ -e $CRL_CERT_NAME ]; then
			if cat $CRL_CERT_NAME | grep -q "BEGIN CERTIFICATE" ; then
				echo "IPSRsaCRLCertValid=\"ok\";"
			else
				echo "IPSRsaCRLCertValid=\"wrongformat\";"
			fi
		else
			echo "IPSRsaCRLCertValid=\"optional\";"
		fi
		SERVERDATE=$(date +%Y-%m-%d)
		SERVERTIME=$(date +%H:%M:%S)
		echo "curDate=\"$SERVERDATE\";"
		echo "curTime=\"$SERVERTIME\";"
		htmlWrite "var result='ok';\n"
		
		;;

	'delprofile')
		htmlWrite "Content-Type: text/html\n\n"
		log "deleting ipsec profile..."
		profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
		rdb_get -L "profile.$profileNum" | while read line; do
			rdb_del $line		
		done
		htmlWrite "var result='ok';\n"
		;;

	'ipsecLogApply')
		htmlWrite "Content-Type: text/html\n\n"
		log "Restart openswan to apply log change..."
		
		killall _pluto_adns 1>/dev/null 2>&1
		killall pluto 1>/dev/null 2>&1
			
		htmlWrite "var result='ok';\n"
		;;

	'ipsecLogDn')
		log "downloading ipsec log..."
		if ! htmlCatIpsecLog; then
			log "download failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
			exit 0
		fi
		;;
	
	'dnrsa')
		profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
		RSAKEY_FILENAME="$IPSEC_RSAKEY_DIR/leftrsa$profileNum.key"
		log "downloading RSA key..."
		if ! htmlCatPubKeyFrom "$RSAKEY_FILENAME"; then
			log "download failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
			exit 0
		fi
		;;
	
	'genrsa')
		htmlWrite "Content-Type: text/html\n\n"
		log "genereate RSA key..."
		profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
		mv /dev/random /dev/random_tmp
		ln -s /dev/urandom /dev/random
		install -d -m 0700 "$IPSEC_RSAKEY_DIR"
		if pgrep "_pluto_adns" 1>/dev/null 2>&1 || pgrep "pluto" 1>/dev/null 2>&1; then
			if ! (eval "/usr/netipsec/sbin/ipsec newhostkey --output $IPSEC_RSAKEY_DIR/leftrsa$profileNum.key") then
				log "ipsec rsa key generating failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
				rdb_set "result.${profileNum}.rsa.keygen" "Failure"
				exit 0
			fi
			rm -f /dev/random
			mv /dev/random_tmp /dev/random

			rdb_set "result.${profileNum}.rsa.keygen" "Success"
			htmlWrite "var result='ok';\n"
		else
			log "ipsec is not running"
			exit 0
		fi
		;;
	'upload')
		htmlParsePostData
		
		log "POST PARAM: ipsec_cgi_subaction=$ipsec_cgi_subaction, ipsec_cgi_param=$ipsec_cgi_param"
		
		IPSEC_TMP_FILE="/tmp/ipsec_action_tmp_$$"
		IPSEC_TMP_FILE1="/tmp/ipsec_action_tmp_1"

		case $ipsec_cgi_subaction in
			
			#remote rsa keys upload
			'upIpsecRsa')
				log "uploading remote secret... $IPSEC_TMP_FILE"
				if ! htmlCatTo "$IPSEC_TMP_FILE"; then
					log "htmlCatTo failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
					
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi

				if [ -z "$ipsec_cgi_param" ]; then
					log "upIpsecRsa requires param"
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi
				profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
	
				# install rsa keys
				RSA_KEY_NAME="rightrsa$profileNum.key" 
				install -d -m 0700 "$IPSEC_RSAKEY_DIR"
				#delete the last two lines which are added by html
				sed 'N;$!P;$!D;$d'  "$IPSEC_TMP_FILE" > "$IPSEC_TMP_FILE1"
				install -m 0600 "$IPSEC_TMP_FILE1" "$IPSEC_RSAKEY_DIR/$RSA_KEY_NAME"
				;;

			#Local rsa keys upload
			'upIpsecLocalRsa')
				log "uploading left secret... $IPSEC_TMP_FILE"
				if ! htmlCatTo "$IPSEC_TMP_FILE"; then
					log "htmlCatTo failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
					
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi
				
				if [ -z "$ipsec_cgi_param" ]; then
					log "upIpsecLocalRsa requires param"
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi
				
				profileNum=$(echo "$ipsec_cgi_param" | cut -d , -f 1)
				
				# install Local rsa keys
				RSA_KEY_NAME="leftrsa$profileNum.key" 
				install -d -m 0700 "$IPSEC_RSAKEY_DIR"
				#delete the last two lines which are added by html
				sed 'N;$!P;$!D;$d'  "$IPSEC_TMP_FILE" > "$IPSEC_TMP_FILE1"
				install -m 0600 "$IPSEC_TMP_FILE1" "$IPSEC_RSAKEY_DIR/$RSA_KEY_NAME"
				;;

			'upIpsecCert')
				log "uploading certificate..."

				if [ -z "$ipsec_cgi_param" ]; then
					log "upcert requires param"
					
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi

				profileNum=$(echo "$ipsec_cgi_param" | cut -d "&" -f 1)
				UPLOAD_CERT_NAME=$(echo "$ipsec_cgi_param" | cut -d "&" -f 2)
				FILE_TYPE=$(echo "$ipsec_cgi_param" | cut -d "&" -f 3)

				if echo "$UPLOAD_CERT_NAME" | grep -q ".pem"; then
					EXT="pem"
			
				elif echo "$UPLOAD_CERT_NAME" | grep -q ".crt"; then
					EXT="crt"
				else
					EXT="key"					
				fi
				
				case $FILE_TYPE in
					'key')
						INSTALl_PATH="/etc/ipsec.d/private/"
						CERT_NAME="local$profileNum.$EXT"
						;;
					'local')
						INSTALl_PATH="/etc/ipsec.d/certs/"
						CERT_NAME="local$profileNum.$EXT"
						;;
					'remote')
						INSTALl_PATH="/etc/ipsec.d/certs/"
						CERT_NAME="remote$profileNum.$EXT"
						;;
					'ca')
						INSTALl_PATH="/etc/ipsec.d/cacerts/"
						CERT_NAME="ca$profileNum.$EXT"
						;;
					'crls')
						INSTALl_PATH="/etc/ipsec.d/crls/"
						CERT_NAME="crl$profileNum.$EXT"
						;;
					*)
						;;
				esac
					
				if ! htmlCatTo "$IPSEC_TMP_FILE"; then
					log "htmlCatTo failed - action=$ipsec_cgi_action,param=$ipsec_cgi_param"
					
					rm -f "$IPSEC_TMP_FILE"
					exit 0
				fi
				

				log "installing certificate..."
				install -d -m 0700 "$INSTALl_PATH"
				#delete the last two lines which are added by html
				sed 'N;$!P;$!D;$d'  "$IPSEC_TMP_FILE" > "$IPSEC_TMP_FILE1"
				install -m 0600 "$IPSEC_TMP_FILE1" "$INSTALl_PATH$CERT_NAME"
				;;
		esac
		
		rm -f "$IPSEC_TMP_FILE"
		rm -f "$IPSEC_TMP_FILE1"

		htmlWrite "Content-Type: text/html\n\n"
		htmlWrite "var result='ok'\n"
			
		;;
		
	*)
		exit 1
		;;
esac

exit 0

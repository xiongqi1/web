#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

log() {
	logger -t "ssh.cgi" -- "$@"
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

print_commands() {
	cat << EOF
info
del_hostkeys
gen
dn_hostkeys
dn_pubhostkeys
delall_clientkeys
del_clientkey
up_clientkey
up_hostkeys
EOF
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

htmlWriteJSONB() {
	# print json output
	echo "{"
}

htmlWriteJSONE() {
	echo "\"cgiresult\":$cgiresult"
	echo "}"

}

ssh_cgi_del_hostkeys() {
	# html header
	htmlWriteReply
	
	htmlWriteJSONB
	
	sshd.sh delete_host_keys
	cgiresult=$?
	
	htmlWriteJSONE
}

ssh_cgi_del_clientkey() {
	# html header
	htmlWriteReply
	
	htmlWriteJSONB
	
	if [ -z "$CGI_PARAM_opt1" -o -z "$CGI_PARAM_opt2" ]; then
		log "user name or index not specified - user='$CGI_PARAM_opt1',idx='$CGI_PARAM_opt2'"
		cgiresult=1
	else 
		# delete rdb
		rdb_del "service.ssh.clientkey.$CGI_PARAM_opt1.$CGI_PARAM_opt2" > /dev/null 2> /dev/null
	fi
	
	# trigger template
	rdb_set "service.ssh.clientkey" "1"
	
	cgiresult=$?
	
	htmlWriteJSONE
}

ssh_cgi_up_hostkeys() {
	htmlParsePostData
	
	cat 2> /dev/null | sshd.sh "add_host_keys" "$CGI_PARAM_opt1"
	cgiresult=$?
	
	# html header
	htmlWriteReply
	htmlWriteJSONB
	htmlWriteJSONE
}

ssh_cgi_up_clientkey() {

	htmlParsePostData
	
	# add public key
	cat 2> /dev/null | sshd.sh "add_client_key"
	cgiresult=$?
	
	# update client keys
	sshd.sh update_client_keys
	
	# html header
	htmlWriteReply
	htmlWriteJSONB
	htmlWriteJSONE
}

ssh_cgi_delall_clientkeys() {
	
	# html header
	htmlWriteReply
	
	htmlWriteJSONB
	
	sshd.sh delete_client_keys
	
	# trigger template
	sshd.sh update_client_keys
	
	cgiresult=$?
	
	htmlWriteJSONE
}

ssh_cgi_gen() {
	htmlWriteReply
	
	htmlWriteJSONB
	
	sshd.sh delete_host_keys
	sshd.sh gen_host_keys
	cgiresult=$?

	# assume it is success if no key is generated
	if [ $cgiresult -eq 99 ]; then
		cgiresult=0
	fi
	
	htmlWriteJSONE
}

ssh_cgi_info() {
	htmlWriteReply
	
	htmlWriteJSONB
	
	sshd.sh list_host_keys && sshd.sh list_client_keys
	cgiresult=$?
	
	htmlWriteJSONE
}

ssh_cgi_dn_hostkeys() {
	tmp=$(mktemp "/tmp/ssh_cgi_$$_XXXXXX")
	
	sshd.sh extract_host_keys "$CGI_PARAM_opt1" > "$tmp"
	cgiresult=$?
	htmlCatFrom "$tmp" "server_private_public_keys.zip" 2> /dev/null
	
	rm -f "$tmp" 2> /dev/null
}

ssh_cgi_dn_pubhostkeys() {
	tmp=$(mktemp "/tmp/ssh_cgi_$$_XXXXXX")
	
	sshd.sh extract_public_host_keys > "$tmp"
	cgiresult=$?
	htmlCatFrom "$tmp" "server_public_keys.zip" 2> /dev/null
	
	rm -f "$tmp" 2> /dev/null
}


# bypass if it is an unknown command 
if [ -z "$CGI_PARAM_cmd" ] || ! print_commands | grep -q "^$CGI_PARAM_cmd$"; then

	log "unknown command specified - cmd='$CGI_PARAM_cmd', opt1='$CGI_PARAM_opt1', opt2='$CGI_PARAM_opt2', opt3='$CGI_PARAM_opt3'"

	# return error
	cat << EOF
{
	"cgiresult":255
}
EOF

	exit 0
fi

log "starting command... [cmd='${CGI_PARAM_cmd}',opt1='${CGI_PARAM_opt1}']"

# start command
eval ssh_cgi_${CGI_PARAM_cmd} 2> /dev/null

if [ $cgiresult -eq 0 ]; then
	log "finishing command... succ ['${CGI_PARAM_cmd}']"
else
	log "finishing command... fail ['${CGI_PARAM_cmd}',cgiresult:$cgiresult]"
fi	

exit 0

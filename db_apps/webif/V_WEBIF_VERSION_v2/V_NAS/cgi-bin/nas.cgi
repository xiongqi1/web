#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# add local path for ipk packages
export PATH="/usr/local/sbin:/usr/local/bin:$PATH"

log() {
	logger -t "nas.cgi" -- "$@"
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
mount
umount
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

nas_cgi_umount() {
	htmlWriteReply

	htmlWriteJSONB

	idx=$(uri_decode "$CGI_PARAM_opt1")

	storage.sh umount "$idx"
	cgiresult=$?

	htmlWriteJSONE
}

nas_cgi_mount() {
	htmlWriteReply

	htmlWriteJSONB

	idx=$(uri_decode "$CGI_PARAM_opt1")

	storage.sh mount "$idx"
	cgiresult=$?

	htmlWriteJSONE
}

nas_cgi_info() {
	htmlWriteReply

	htmlWriteJSONB

	echo '"storages":['

	# TODO: use JSON encode for strings
	storage.sh info | egrep '^[0-9]+' | awk -F $'\t' '{
		if (NR>=2)
		print "	,"
		print "	{"
			print "		\"idx\":"$1","
			print "		\"stat\":\""$2"\","
			print "		\"dev\":\""$3"\","
			print "		\"loc\":\""$4"\","
			print "		\"mp\":\""$5"\","
			print "		\"fs\":\""$6"\","
			print "		\"size\":\""$7"\","
			print "		\"used\":\""$8"\","
			print "		\"avail\":\""$9"\","
			print "		\"up\":\""$10"\","
			print "		\"desc\":\""$11"\""
		print "	}"


	}'

	echo '],'

	cgiresult=$?

	htmlWriteJSONE
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

if [ "${CGI_PARAM_cmd/_*/}" = "info" ]; then
	info_cmd=1
else
	info_cmd=0
fi

if [ "$info_cmd" = "0" ]; then
	if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
		exit 0
	fi

fi

if [ "$info_cmd" = "0" ]; then
	log "starting command... [cmd='${CGI_PARAM_cmd}',opt1='${CGI_PARAM_opt1}']"
fi

# start command
eval nas_cgi_${CGI_PARAM_cmd} 2> /dev/null

if [ "$info_cmd" = "0" ]; then
	if [ $cgiresult -eq 0 ]; then
		log "finishing command... succ ['${CGI_PARAM_cmd}']"
	else
		log "finishing command... fail ['${CGI_PARAM_cmd}',cgiresult:$cgiresult]"
	fi
fi

exit 0

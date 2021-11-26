#!/bin/sh
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi
#
# reqtype 
#

# termtype
# atcmd

log() {
	logger -t 'routerservice.cgi' -- "$@"
}

PIPEID_DIRECT=1000
PIPEID_ATMGR=1001

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

	if [ "$NAME" = "CGI_PARAM_atcmd" ]; then
		eval $VAR
	fi

	if [ "$NAME" = "CGI_PARAM_termtype" ]; then
		eval $VAR
	fi

done


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

cgi_atterm_recv() {
	if [ -n "$PIPEID" ]; then
		atresp=$(cgipipe_read "$PIPEID" 2> /dev/null)
		cgiresult=$?
		atresp_encodeJSONd=$(encodeJSON "$atresp")
	else
		atresp_encodeJSONd=""
		cgiresult="0"
	fi

	if cgipipe_check "$PIPEID_DIRECT" >/dev/null 2>/dev/null; then
		termtype="direct"
	elif cgipipe_check "$PIPEID_ATMGR" >/dev/null 2>/dev/null; then
		termtype="atmgr"
	else
		termtype="none"
	fi

	# print JSON
	cat << EOF
{
	"atresp":"$atresp_encodeJSONd",
	"termtype":"$termtype",
	"cgiresult":$cgiresult
}
EOF
}

cgi_atterm_send() {
	log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"

	atcmd=$(uri_decode "$CGI_PARAM_atcmd")

	echo "$atcmd" | cgipipe_write "$PIPEID" 2> /dev/null > /dev/null
	cgiresult=$?

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
}

cgi_atterm_reboot() {
	log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"

	reboot_module.sh 2> /dev/null > /dev/null
	cgiresult=$?
	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
}

cgi_atterm_start() {
	log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"

	if [ -z "$PIPEID" ]; then
		log "cgi_atterm_start: incorrect termtype detected"
		return 1
	fi

	cgipipe_server "$PIPEID" /usr/bin/atcmd_term.sh "$CGI_PARAM_termtype" 2> /dev/null > /dev/null &

	cgiresult=$?
	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
}

lsof() {
	lsof_pids=$(ls -l $(find /proc/[0-9]*/fd -type l 2> /dev/null ) 2> /dev/null | grep "$1" | sed -n 's/.* \/proc\/\([0-9]\+\)\/fd\/.*/\1/p')
	echo "$lsof_pids"

	if [ -z "$lsof_pids" ]; then
		return 1
	else
		return 0
	fi
}

cgi_atterm_stop() {
	log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"

	if [ -z "$PIPEID" ]; then
		log "cgi_atterm_stop: incorrect termtype detected"
		return 1
	fi

	log "echo '*term' | cgipipe_ctrl "$PIPEID" 2>/dev/null >/dev/null"
	echo '*term' | cgipipe_ctrl "$PIPEID" 2>/dev/null >/dev/null

	cgiresult=$?

	cat << EOF
{
	"cgiresult":$cgiresult
}
EOF
	return 0
}

#log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"

htmlWrite() {
	echo -n -e "$@"
}

htmlWriteReply() {
	htmlWrite "Status: 200\n"
	htmlWrite "Content-type: text/plain\n"
	htmlWrite "Cache-Control: no-cache\n"
	htmlWrite "Connection: keep-alive\n\n"
}

htmlCatFrom() {
	source="$1"
	filename="$2"
	
	if [ -z "$filename" ]; then
		filename=$(basename "$source")
	fi
	
	if [ ! -r "$source" ]; then
		log "cannot access file \'$source\': Permission denied"
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
		log "cannot cat file \'$source\': Return code $?"
		return 1;
	fi
	
	return 0
}

sys_info_cmds() {
	cat << EOF
rdb_get -L . | sort
cat /proc/bus/usb/devices
lsusb
logread
ps
uptime
date
ifconfig
mount
change_module_mode.sh
netstat -an
netstat -aner
route -n
iptables -t nat -L -n -v
iptables -t filter -L -n -v
cat /etc/resolv.conf
ip route show cache
lsmod
cat /etc/version.txt
cat /proc/net/arp
free
cat /proc/meminfo 
top -n 1
cat /proc/net/dev
EOF
}

sys_info_run_cmds() {
	sys_info_cmds | while read cmd; do
		echo "############################"
		echo "$cmd"
		echo "############################"

		$cmd 2>&1
	done
}

cgi_download_sys_info() {
	log "reqtype='$CGI_PARAM_reqtype', atcmd='$CGI_PARAM_atcmd', termtype='$CGI_PARAM_termtype'"
	sys_info_fname="/tmp/sys-info-$$"

	# this is replaced with the real platform name in Makefile
	platform="<<<PLATFORM>>>"

	case "$platform" in
		'Platypus2')
			password=$(rdb_get "admin.user.password")
			;;

		'Bovine')
			password=$(rdb_get "admin.user.root")
			;;

		*)
			password=$(rdb_get "admin.user.admin")
			;;
	esac

	sys_info_run_cmds | gzip -f | openssl des3 -salt -k "$password" > $sys_info_fname

	htmlCatFrom "$sys_info_fname" "sys-info.gzip.des3"

	rm -f "$sys_info_fname"
}

# get peipe id
case $CGI_PARAM_termtype in
	'direct')
		PIPEID="$PIPEID_DIRECT"
		;;

	'atmgr')
		PIPEID="$PIPEID_ATMGR"
		;;

	*)
		PIPEID=""
		;;
esac

case "$CGI_PARAM_reqtype" in
	atterm_*)
		htmlWriteReply
		cgi_${CGI_PARAM_reqtype}
		;;

	'download_sys_info')
		cgi_download_sys_info
		;;

	*)
		exit 1
		;;
esac

exit 0

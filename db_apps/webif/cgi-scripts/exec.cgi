#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# Regular expression to match legal commands
ALLOWED='^change_module_mode.sh.*$'

# Forcing to root path
cd /

# splits CGI query into var="value" strings
cgi_split() {
	echo "$1" | awk 'BEGIN {
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

invalid() {
	echo "Error"
	echo '</body>'
	echo '</html>'
	exit 1
}

echo -e 'Content-type: text/html\n'
echo '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">'
echo '<html xmlns="http://www.w3.org/1999/xhtml">'
echo '<head>'
echo '  <meta http-equiv="Pragma" content="no-cache"/>'
echo '  <meta http-equiv="Expires" content="-1"/>'
echo '  <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"/>'
echo "  <title>Running command</title>"
echo '</head>'
echo '<body>'

qlist=`cgi_split "$QUERY_STRING"`

split() {
	shift $1
	echo "$1"
}

# Format check
if ! echo "$qlist" | grep -q 'CMD=".*"'; then
	invalid
fi

# Extract command line and strip quotes
CMD=`echo "$qlist" | sed 's/CMD=//' | tr -d '"'`

# Check for legal command
if ! echo "$CMD" | grep -q -- "$ALLOWED"; then
	invalid
fi

echo "Running: \"$CMD\""
echo '<pre>'

#echo "$QUERY_STRING"
#echo "$qlist"
#echo "$CMD"

/bin/sh -c "$CMD"

echo '</pre>'
echo '</body>'
echo '</html>'

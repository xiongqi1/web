#!/bin/sh

WGET_CONNECTION_TIME=10

WGET_HOST="services.netcomm.com.au"
WGET_URL="http://services.netcomm.com.au/c/time.pl"
WGET_STR='2[0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9].*<br/>[0-9]\+'


nslookup_by_ping() {
	host="$1"
	if ping -c 1 "$host" 2>&1 >/dev/null | grep -q "bad address"; then
		return 1
	fi
		
	return 0
}

wget_by_nc() {
	host="$1"
	url="$2"
	
	logger -t "score" "echo -e \"GET $url HTTP/1.0\n\n\" | nc -w $WGET_CONNECTION_TIME \"$host\" 80 2>&1"
	echo -e "GET $url HTTP/1.0\n\n" | nc -w $WGET_CONNECTION_TIME "$host" 80 2>&1
	RESULT_WGET=$?
	
	return $RESULT_WGET
}

# score
score=0

logger -t "score" "scoring..."

TEMPFILE="/tmp/score-$$.tmp"
wget_by_nc "$WGET_HOST" "$WGET_URL" > "$TEMPFILE"

if grep -q "$WGET_STR" "$TEMPFILE"; then
	score=9
else
	test -z "$RESULT_WGET" && RESULT_WGET=0
	
	if [ $RESULT_WGET = 0 ]; then
		score=2
	else
		RESULT_NSLOOKUP=$(nslookup_by_ping "$WGET_HOST"; echo $?)
		
		if [ $RESULT_NSLOOKUP = 0 ]; then
			score=3
		fi
	fi
fi

logger -t "score" "score : $score"

echo "score : $score"


rm -f "$TEMPFILE"

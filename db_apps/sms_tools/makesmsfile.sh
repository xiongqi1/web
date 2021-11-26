#!/bin/sh
# $1 : number of sms files
# assume this script run on sms directory
# create sms files in inbox & outbox folder

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo ""
	echo "This shell script is for internal system use only."
	echo "It is used for managing SMS inbox/outbox files & folders."
	echo "Please do not run this script manually."
	echo ""
	exit 0
fi

SMS_NO=10
test $1 -gt 99999 && echo "too large number '$1', give up" && exit 255
test "$1" != "" && SMS_NO=$1

#------- inbox
if [ ! -e "./inbox" ]; then
	mkdir "./inbox"
fi

cd "./inbox"
#rm -f *
let "idx=1"
while [ $idx -le  $SMS_NO ]; do
	FILE="rxmsg_"`printf %05d ${idx}`"_unread"
	touch $FILE
	echo "From: 61438404841" > $FILE
	echo "From_SMSC: 61418706700" >> $FILE
	echo "Sent: "`date` >> $FILE
	#echo "Received: "`date` >> $FILE
	echo "Subject: GSM1" >> $FILE
	echo "Alphabet:  ISO" >> $FILE
	echo "UDH: false" >> $FILE
	echo "" >> $FILE
	echo "GSM7:sms test #00$idx" >> $FILE
	echo `date` >> $FILE
	let "idx+=1"
	sleep 1
done

cd ../

#------- outbox
if [ ! -e "./outbox" ]; then
	mkdir "./outbox"
fi
cd ./outbox
#rm -f *

let "idx=1"
while [ "$idx" -le  "$SMS_NO" ]; do
	FILE="txmsg_"`printf %05d ${idx}`
	touch $FILE
	echo "To: 0438404841" > $FILE
	echo "" >> $FILE
	echo "GSM7:SMS TX test #00$idx" >> $FILE
	echo `date` >> $FILE
	let "idx+=1"
	sleep 1
done

cd ../

exit 0


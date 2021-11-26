#!/bin/sh
# extract message strings from sms.js to include into sms.html file
# in order to create sms.xml file during compilation.
HTMLSOURCE="sms.html"
JSSOURCE="sms.js"
TARGET="sms_js_strings.inc"

#---------------------------------------------------------------------------
# For help text
#---------------------------------------------------------------------------
if [ "$1" = "--help" -o "$1" = "-h" ]; then
	echo ""
	echo "This shell script is for internal system use only."
	echo "It is used for extracting strings from sms.js to include in sms.xml."
	echo "Please do not run this script manually."
	echo ""
	exit 0
fi

test ! -e $JSSOURCE && echo "$JSSOURCE file does not exist!" && exit 0
test ! -e $HTMLSOURCE && echo "$HTMLSOURCE file does not exist!" && exit 0
test -e $TARGET && rm $TARGET
grep "_(\"" $JSSOURCE > $TARGET
cat $TARGET | sed -e 's/^[ \t]*//' -e 's/^/\/\//' > $TARGET.new && mv $TARGET.new $TARGET

exit 0

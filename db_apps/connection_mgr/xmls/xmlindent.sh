#!/bin/sh

#
# This indent is required for Platypus and Bovine UI that have a lame xml parser
#

ident=-2;
sed "s/\&amp\;/&/g" | sed "s/ </\n</g" | sed "s/><\//>\n<\//g" | sed "s/<\(.*\) \/>/<\1><\/\1>/g" | while read a; do
	if echo "$a" | egrep -q '<[^/]'; then 
		ident=$((ident + 1))
	fi
	
	i=0
	while [ $i -lt $ident ]; do
		i=$((i+1))
		echo -n "\t"
	done
	
	echo "$a"
	
	if echo "$a" | egrep -q '</|/>'; then
		ident=$((ident - 1))
	fi
	
done

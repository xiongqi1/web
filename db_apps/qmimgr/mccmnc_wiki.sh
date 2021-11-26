#!/bin/sh

#
# this script extract mcc mnc list from wiki page
#

print_mccmnc() {
	i=0
	mcc=""
	mnc=""
	
	wget -q http://en.wikipedia.org/wiki/Mobile_Network_Code -O - | egrep '<tr>|<td>[0-9][0-9][0-9]*</td>|<td></td>' | while read line; do

		if echo "$line" | grep -q "<tr>"; then
			if [ -n "$mcc" -o -n "$mnc" ]; then
				test -z "$mcc" && mcc="?" 
				test -z "$mnc" && mnc="?" 
				
				echo "$mcc $mnc"
			fi
		
			i=0
			mcc=""
			mnc=""
		else
			val=$(echo "$line" | sed -n 's/<td>\([0-9]\+\)<\/td>/\1/p')
			
			case $i in
				1) mcc="$val";;
				2) mnc="$val";;
			esac
			
		fi

		i=$((i+1))
	done
}


j=1
print_mccmnc | sort -u | while read line; do
	echo "{ \"$line\", $j },"
	j=$((j+1))
done

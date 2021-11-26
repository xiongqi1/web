#!/bin/sh
echo -e 'Content-type: text/html\n'

cd /www/usermenu

V=""
for F in *; do
	if [ "$F" = "*" ]; then
		exit;
	fi
	V=${V}"<a href='`cat "$F"`'>$F</a>" #>>$USER_MENU_FILENAME
done

echo $V

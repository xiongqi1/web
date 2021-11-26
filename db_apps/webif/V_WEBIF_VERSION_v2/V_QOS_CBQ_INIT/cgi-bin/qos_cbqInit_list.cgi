#!/bin/sh
#
# Generate a html javascript segment to set an array to the installed
# CBQ file names
#

echo "<script language=\"JavaScript\">"
echo -n "var cbqFiles=["
list=$(rdb list qos.cbqInit.file | tr "\n" " ")
if [ -n "$list" ] ;then
  for file in $list
  do
    file=${file#qos.cbqInit.file.}
    echo -n "\"$file\","
  done
fi
echo "];"
echo "</script>"

exit 0


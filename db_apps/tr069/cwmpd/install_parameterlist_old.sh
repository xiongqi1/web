#!/usr/bin/env sh

variantFile="$1"
releaseVer="$2"
InputFile="$3"
OutputFile="$4"

tempFile="./tempParameterList"
echo "variantFile=$variantFile"
echo "releaseVer=$releaseVer"
echo "InputFile=$InputFile"
echo "OutputFile=$OutputFile"
usage() {
	cat <<EOM

Usage: `basename $0` InputFile OutputFile

	Generate TR069 parameter list

	InputFile = TR069 Configuration File
	OutputFile = Parameter List File

EOM
	exit 1
}

if [ ! "$#" = "4" -o ! -f "$InputFile" ]; then
	usage
fi

if [ -e "$OutputFile" -a ! -f "$OutputFile" ]; then
	usage
fi

if [ ! -f "$variantFile" ]; then
	usage
fi

#To replace leading spaces to tab character
cmd1=':start;s/^\(\t*\)\( \)/\1\t/;t start;'

HandlerTypes="const transient rdb rdbobj rdbmem dynamic"

#To take out handler field
cmd2=''

for hdlerType in $HandlerTypes; do
	cmd2="${cmd2}s/${hdlerType}[ ]*\([^#]*\)[ ;]//;"
done
# To put some colour on comments.
cmd3='s/\(#[^0-9A-Fa-f].*\)/<font style="color:#FF0000">\1<\/font>/;'

# To take out "param" indicator from each parameter and put quotation mark for the name field
cmd4='s/\(^[\t]*\)[ ]*\(param\)[ ]*\([^ ]*\)\(.*\)/<li>\1"\3"\4<\/li>/;'

# To swap the position of "object" and "name" field and put quotation mark for the name field
cmd5='s/\(^[\t]*\)[ ]*\(object\)[ ]*\([^ \{]*\)/<li style="color:#0000FF">\1"\3" \2<\/li>/;'

# To swap the position of "collection" and "name" field and put quotation mark for the name field
cmd6='s/\(^[\t]*\)[ ]*\(collection\)[ ]*\([^ \{]*\)/<li style="color:#0000FF">\1"\3" \2<\/li>/;'

# To replace default instance of "collection" to "1" object
cmd7='s/\(^[\t]*\)default/<li style="color:#0000FF">\1"1" object<\/li>/;'






Body=`sed "${cmd1}${cmd2}${cmd3}${cmd4}${cmd5}${cmd6}${cmd7}" $InputFile`

echo "$Body" > $tempFile

. $variantFile

htmlresult=$(cat << EOF
<html>
<head>
<title>TR069 Parameter List</title>
</head>
<body>
<h2 style='text-align:center'>TR069 Parameter List</h2>
<table border='1'>
<tr><th>Product:</th><th>&nbsp;&nbsp;${V_PRODUCT}</th></tr>
<tr><th>Version:</th><th>&nbsp;&nbsp;${releaseVer}</th></tr>
<tr><th>ModelName:</th><th>&nbsp;&nbsp;${V_PRODUCT}</th></tr>
<tr><th>Description:</th><th>&nbsp;&nbsp;${V_IDENTITY}</th></tr>
<tr><th>ProductClass:</th><th>&nbsp;&nbsp;${V_CLASS}</th></tr>
</table>
<dir>


`sed 's/{/<dir style="color:#000000">/; s/}\;/<\/dir>/' $tempFile`


</dir>
</body>
</html>
EOF
)

echo "$htmlresult" > $OutputFile
rm -f $tempFile 2> /dev/null

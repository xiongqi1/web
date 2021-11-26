#! /usr/bin/awk -f

BEGIN {
	print ("Content-type: text/html\n");
	print ("var st=[");
	el=split( "usage.html http:www.netcommwireless.com", exceptList, " " );
	cmd = "ls /www |grep .html";
	allList="";
	while (cmd | getline) {
		fileName=$1;
		find=0;
		for(i=1; i<=el; i++) {
			if(fileName==exceptList[i]) {
				find=1;
				continue;
			}
		}
		#print fileName;
		if( !find && !index(allList, fileName) ) {
			if(allList=="")
				allList="\""fileName"\"";
			else
				allList=allList",\""fileName"\"";
		}
	}
	close (cmd);
#	gsub(/[\/()]/, "", allList)
	print allList "]"
}

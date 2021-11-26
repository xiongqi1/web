#! /usr/bin/awk -f

BEGIN {
	print ("Content-type: text/html\n");	
	print ("var st=[");
	el=split( "usage.html snmp.html http:www.netcommwireless.com", exceptList, " " );
	cmd = "cat /www/menu.html";
	allList="";
	while (cmd | getline) {
		if(index($0, "<!--"))
			continue;
		pos=index($0, "href=\"")
		if(!pos)
			continue;
		s1=substr($0, pos+6);
		
		pos=index(s1, "\"");
		if(!pos)
			continue;
		fileName=substr(s1, 1, pos-1);
		gsub(/\//, "", fileName)
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

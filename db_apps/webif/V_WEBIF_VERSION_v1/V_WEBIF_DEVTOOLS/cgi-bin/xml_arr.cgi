#! /usr/bin/awk -f
#######################################################
#qry[1]=file name / qry[2]=Language / qry[3]=arrar name
#######################################################
BEGIN {
	print ("Content-type: text/html\n");
	len=split (ENVIRON["QUERY_STRING"], qry, "&");
	if ( len != 3 )
		exit (0);
	FS="=";
	
#	print "var "qry[3]"=new Array();"
	cmd = "cat /www/lang/"qry[2]"/"qry[1];
	while (cmd | getline) {
		if(NF != 3)
			continue;
		id=$2;
		str=$3;
		gsub(/ msgstr/, "", id)
		gsub(/\/>/, "", str)
		if(id!="") {
			print qry[3]"["id"]="str;
		}
	}
	close (cmd);
}

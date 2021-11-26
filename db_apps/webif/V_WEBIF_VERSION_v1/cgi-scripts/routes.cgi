#! /usr/bin/awk -f

function escape(i_Str) {
	retVal="";
	srcStr=i_Str;
	while (match(srcStr, /[\\\"\']/)) {
		retVal = retVal substr(srcStr, 1, RSTART-1) "\\" substr(srcStr, RSTART, RLENGTH);
		srcStr = substr(srcStr, RSTART+RLENGTH);
	}
	retVal = retVal srcStr;

	return retVal;
}

BEGIN
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
########################## Static Routes ###############################
	print ("var st_routes=[");
	i=0;
	while( (cmd = sprintf("rdb_get service.router.static.route.%s",i) | getline) > 0 ) {
		if( split($0,myarr,",")<5 ){
			i++;
			continue;
		}

		if(i>0) print(",");
		printf("{\n");
		escapedStr = escape(myarr[1]);
		printf("\"routeName\":\"%s\",\n", escapedStr);
		printf("\"dstIP\":\"%s\",\n", myarr[2]);
		printf("\"subMask\":\"%s\",\n", myarr[3]);
		printf("\"gatewayIP\":\"%s\",\n", myarr[4]);
		printf("\"metric\":\"%s\",\n", myarr[5]);
		if(myarr[6]=="") {
			printf("\"networkInf\":\"%s\"\n", "auto");
		}
		else {
			printf("\"networkInf\":\"%s\"\n", myarr[6]);
		}
		printf("}");
		i++;
		close( cmd );
	}
	print ("\n];\n");

######################### Active Routing Table ########################	
	print ("var routingtable=[");
	cmd="route -n | grep '^[0-9]'"
	#"route -n" | getline; #1st and 2nd line is the header
	#"route -n" | getline;
	i=1;
	while ((cmd | getline) > 0) {
#printf("\"NF=%s\",\n",NF);
#for(j=0; j<=NF; j++) printf("\"j=%s --- %s\",\n",j, $j);
		if( NF==8 ) {
			if(i>1) print(",");
			printf("{\n");
			printf("\"Destination\":\"%s\",\n", $1);
			printf("\"Gateway\":\"%s\",\n", $2);
			printf("\"Genmask\":\"%s\",\n", $3);
			printf("\"Flags\":\"%s\",\n", $4);
			printf("\"Metric\":\"%s\",\n", $5);	
			printf("\"Ref\":\"%s\",\n", $6);
			printf("\"Use\":\"%s\",\n", $7);
			printf("\"Iface\":\"%s\"\n", $8);
			printf("}");
			i++;
		}
	}
	print ("\n];\n");

	close(cmd);
}
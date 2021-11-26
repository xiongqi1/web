#! /usr/bin/awk -f 
BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	print ("var st_nat=[");
	i=0;
	j=1;
	while( (cmd = sprintf("rdb_get service.firewall.dnat.%s",i) | getline) > 0 )
	{		
		#printf("\"NF=%s\",\n",NF);
		#for(j=0; j<=NF; j++)
		#{
		#	printf("\"j=%s --- %s\",\n",j, $j);
		#}
		#return;

		if( NF == 13)
		{
			if(j>1) print(",");
			printf("{\n");
			printf("\"protocol\":\"%s\",\n", toupper($2));
			printf("\"sourceIP\":\"%s\",\n", $4);
			idx = index($6,":");
			printf("\"incomingPortStart\":\"%s\",\n", substr($6,0,idx-1) );
			printf("\"incomingPortEnd\":\"%s\",\n", substr($6,idx+1, 5) );
			idx = index($12,":");
			printf("\"destinationIP\":\"%s\",\n", substr($12, 0, idx-1));
			printf("\"destinationPortStart\":\"%s\",\n", substr($12, idx+1,(index($12,"-")-idx-1)) );
			printf("\"destinationPortEnd\":\"%s\"\n", substr($12, index($12,"-")+1, 5) );	
			printf("}");
			i++;	
			j++;	
		}
		else if( NF == 11 && $3 == "--dport" )
		{
			if(j>1) print(",");
			printf("{\n");
			printf("\"protocol\":\"%s\",\n", toupper($2));
			printf("\"sourceIP\":\"0.0.0.0\",\n");
			idx = index($4,":");
			printf("\"incomingPortStart\":\"%s\",\n", substr($4,0,idx-1) );
			printf("\"incomingPortEnd\":\"%s\",\n", substr($4,idx+1, 5) );
			idx = index($10,":");
			printf("\"destinationIP\":\"%s\",\n", substr($10, 0, idx-1));
			printf("\"destinationPortStart\":\"%s\",\n", substr($10, idx+1,(index($10,"-")-idx-1)) );
			printf("\"destinationPortEnd\":\"%s\"\n", substr($10, index($10,"-")+1, 5) );	
			printf("}");
			i++;	
			j++;	
		}
		else {i++; continue;}	
	}	
	print ("\n];\n");
	#printf( "var itemnum=%s;\n",i-1);
}

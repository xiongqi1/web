#! /usr/bin/awk -f 
BEGIN 
{
	V_BOARD = "FINCH";
	cmd = "cat /etc/variant.sh"	
	while( cmd | getline )
	{
		if ($0 == "V_BOARD='BOVINE'")
		{
			V_BOARD = "BOVINE";
			WWAN_INTERFACE = "ppp0"
			break;
		}
	}
	close( cmd );
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	
	print ("Content-type: text/html\n");
	printf ("var st=[");
	if(V_BOARD != "FINCH")
	{
		while(( "hostapd_cli all_sta" | getline) > 0)
		{
			for(i=1; i <= NF; i++)
			{
				if(length($i)==17 && (split($i, tempa, ":" )==6) )
					printf("{\"mac\":\"%s\"},\n", $i);
				#if( index( $i, "dot11RSNAStatsSTAAddress=") )
				#	printf("{\"mac\":\"%s\"},\n", substr($i,26,17));
				
			}
		}
	}
	else //FINCH
	{
		counter = 0;
		while(( "wmiconfig -i athwlan0 --getsta" | getline) > 0)
		{
			if(length($1)==17 && (split($1, tempa, ":" )==6) )
			{
				if( counter )
					printf(",\n{\"mac\":\"%s\",\"aid\":\"%s\",",$1,$2);
				else
					printf("\n{\"mac\":\"%s\",\"aid\":\"%s\",",$1,$2);
				cmd = "cat /tmp/var/lib/misc/dnsmasq.leases";
				thisip=tolower($1);
				myip = "";
				myhost = "";
				while(( cmd | getline) > 0)
				{
					if( NF==5 && (split($i, tempb, ":" )>=6) )
					{				
						if ( $2 == thisip )
						{
							myip=$3;
							myhost=$4;	
							break;
						}
					}
				}
				printf("\"ip\":\"%s\",", myip);
				printf("\"host\":\"%s\"", myhost);	
				printf "}"
				counter++;
				close( cmd );
			}	
		}
		close("wmiconfig -i athwlan0 --getsta");
	}
	print ("\n];\n");
}

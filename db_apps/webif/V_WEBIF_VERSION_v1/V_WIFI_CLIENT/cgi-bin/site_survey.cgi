#! /usr/bin/awk -f 
function write_java_variable(javascript_var,javascript_val,suffix) {
	printf("\'%s\':\'%s\'%s",javascript_var,javascript_val,suffix);
}

BEGIN 
{
	print ("Content-type: text/html\n");	
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	cmd = sprintf("iwpriv ra0 get_site_survey 2>/dev/null | grep '^[0-9]' ");
	j=0;
	k=0;
	print ("ssidArray=[");
	while((cmd | getline) > 0)
	{
		ssid= substr($0,5,33); 
		gsub(/ *$/,"",ssid);
		#print ssid
		if(NF > 6)
		{
			BSSID_num=NF-6;
			sec_num=NF-5;
			sig_num=NF-4;
			wlmode_num=NF-3;
		}else{
			BSSID_num=1;
			sec_num=1;
			sig_num=1;
			wlmode_num=1;
		}
	
	# put seperator if any previous exists
		if(k++>0) 
			print(",");
		printf ("{")
		write_java_variable("channel",$1, ",");
		write_java_variable("ssid",ssid, ",");
		write_java_variable("BSSID",$BSSID_num, ",");
		write_java_variable("security",$sec_num, ",");
		write_java_variable("SignalStrength",$sig_num, ",");
		write_java_variable("WirelessMode",$wlmode_num, "");
		printf ("}\n");
	}
	cmd_result = close(cmd);
	print ("];\n");	

	if(cmd_result != 0)
	{
		print ("result='not ready';\n");
		exit 0;
	}

	cmd=sprintf("iwpriv ra0 connStatus 2>/dev/null | cut -d':' -f2 | cut -d'(' -f1");	
	if((cmd | getline) > 0)
	{
		printf ("connStatus='%s';\n",$0);
		
	}
	if($0 == "Connected"){
		cmd=sprintf("iwpriv ra0 connStatus 2>/dev/null |cut -d'[' -f2 | cut -d']' -f1 |tr '[:upper:]' '[:lower:]'");
		if((cmd | getline) > 0){
			printf ("connectedBSSID='%s';\n", $0);	
		}
		else	
			printf ("connectedBSSID='%s';\n", "00:00:00:00:00:00");
	}else
		printf ("connectedBSSID='%s';\n", "00:00:00:00:00:00");
	print ("result='ok';\n");
}


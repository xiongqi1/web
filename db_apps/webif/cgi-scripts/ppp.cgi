#! /usr/bin/awk -f
BEGIN
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	prefix="rdb_get link.profile.";
	print ("var stpf=[");
	i=0;
	profilenum = 0;
	for(i=1; i<=6; ++i) {
		cmd = prefix i ".name";
		if((( cmd | getline) <= 0)||($0=="")) {
			if(profilenum==0) profilenum = i;
			close(cmd);
			continue;
		}
		close(cmd);
		if( i>1 ) print(",");
		printf("{\n\"name\":\"%s\",\n",$0);
		printf("\"enable\":");
		cmd = prefix i ".enable";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"profilenum\":\"%s\",\n",i);
		printf("\"dialnum\":");
		cmd = prefix i ".dialstr";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"\",");
		close(cmd);
		printf("\"user\":");
		cmd = prefix i ".user";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"\",");
		close(cmd);
		printf("\"pass\":");
		cmd = prefix i ".pass";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"\",");
		close(cmd);
		printf("\"snat\":");
		cmd = prefix i ".snat";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"readonly\":");
		cmd = prefix i ".readonly";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"reconnect_delay\":");
		cmd = prefix i ".reconnect_delay";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"30\",");
		close(cmd);
		printf("\"reconnect_retries\":");
		cmd = prefix i ".reconnect_retries";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"metric\":");
		cmd = prefix i ".defaultroutemetric";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"1\",");
		close(cmd);
		printf("\"authtype\":");
		cmd = prefix i ".auth_type";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"chap\",");
		close(cmd);
		##### for PAD mode #####
		printf("\"padmode\":");
		cmd = prefix i ".pad_mode";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"tcp\",");
		close(cmd);
		printf("\"port\":");
		cmd = prefix i ".pad_encode";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"host\":");
		cmd = prefix i ".pad_host";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0.0.0.0\",");
		close(cmd);
		printf("\"pad_o\":");
		cmd = prefix i ".pad_o";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		printf("\"connection_op\":");
		cmd = prefix i ".pad_connection_op";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"1\",");
		close(cmd);
		printf("\"tcp_nodelay\":");
		cmd = prefix i ".tcp_nodelay";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		########################		
		printf("\"pppdebug\":");
		cmd = prefix i ".verbose_logging";
		if(( cmd | getline) > 0)
			printf("\"%s\",\n",$0);
		else
			print("\"0\",");
		close(cmd);
		cmd = prefix i ".apn";
		printf("\"APNName\":");
		if(( cmd | getline) > 0) {
			printf("\"%s\"\n}",$0);
		}
		else {
			printf("\"\"\n}");
			break;
		}
		close(cmd);
	}
	print ("];");
}
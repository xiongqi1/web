#! /usr/bin/awk -f

function exec(mycmd) {
value="";
	#system( "logger \"&&&&&&&&&------cmd="mycmd "\"");
	while( mycmd | getline ) {
		value = sprintf("%s%s", value, $0);
	}
	close( mycmd );
	return value;
}

BEGIN {
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	# CSRF token
	csrfTokenValid=0;
	if (ENVIRON["csrfToken"] != "" && ENVIRON["csrfTokenGet"] != "" && ENVIRON["csrfToken"] == ENVIRON["csrfTokenGet"]) {
		csrfTokenValid=1;
	}
	sub(/^\&csrfTokenGet=[a-zA-z0-9]+\&/, "", ENVIRON["QUERY_STRING"]);

	split (ENVIRON["QUERY_STRING"], qry, "&");
	if( qry[1] == "getList") {
	########################## Data Stream List ###############################
		print ("var st_streams=[");
		i=0;
		while( (en = sprintf("rdb_get service.dsm.stream.conf.%s.enabled", i) | getline) > 0  && $0!="") {
			if(i>0) print(",");
			printf("{\n");
			printf("\"enable\":\"%s\",\n", $1);

			cmd = sprintf("rdb_get service.dsm.stream.conf.%s.name", i);
			cmd | getline;
			printf("\"name\":\"%s\",\n", $0);
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.conf.%s.epa_name", i);
			cmd | getline;
			printf("\"epa_name\":\"%s\",\n", $0);
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.conf.%s.epb_name", i);
			cmd | getline;
			printf("\"epb_name\":\"%s\",\n", $0);
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.conf.%s.epa_mode", i);
			cmd | getline;
			printf("\"epa_mode\":\"%s\",\n", $0);
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.conf.%s.epb_mode", i);
			cmd | getline;
			printf("\"epb_mode\":\"%s\",\n", $0);
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.%s.pid", i);
			if (cmd | getline)
				printf("\"pid\":\"%s\",\n", $0);
			else
				print("\"pid\":\"\",\n");
			close(cmd);

			cmd = sprintf("rdb_get service.dsm.stream.%s.validated", i);
			cmd | getline;
			printf("\"validated\":\"%s\"\n", $0);
			close(cmd);

			printf("}");
			i++;
			close(en);
		}
		print ("\n];\n");
	}
	else if( qry[1] == "setup") {
	########################## Setup Data Stream List ###############################
		# CSRF token must be valid
		if (csrfTokenValid != 1) {
			exit 254;
		}
		# remove stream list first
		system("dsm_tool -d -C -r service.dsm.stream.conf" );
		l=split (qry[2], stream_list, ":");
		for( i=0; i<l; i++ ) {
			ll=split (stream_list[i+1], n, ",");
			if( ll!=6 ) {
				system( "logger \"stream list format error");
				break;
			}
			system( "rdb_set service.dsm.stream.conf."i".enabled \"" n[1] "\" -p" );
			system( "rdb_set service.dsm.stream.conf."i".name \"" n[2] "\" -p" );
			system( "rdb_set service.dsm.stream.conf."i".epa_name \"" n[3] "\" -p" );
			system( "rdb_set service.dsm.stream.conf."i".epb_name \"" n[4] "\" -p" );
			system( "rdb_set service.dsm.stream.conf."i".epa_mode \"" n[5] "\" -p" );
			system( "rdb_set service.dsm.stream.conf."i".epb_mode \"" n[6] "\" -p" );
		}
		system("rdb_set service.dsm.trigger 1");
	}
}

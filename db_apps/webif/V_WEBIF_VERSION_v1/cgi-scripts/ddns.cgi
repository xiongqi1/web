#! /usr/bin/awk -f 
BEGIN {
	print ( "Content-type: text/html\n" );
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	cmd = "cat /tmp/ddns.log 2>/dev/null";
	while( cmd | getline );
	close( cmd );
	if( $0 != "" && $0 != "0" ) {
		print ( "var service_ddns_status='"$0"'" );
		system ("rdb_set service.ddns.status '"$0"'");
	}
	else {
		cmd = "cat /tmp/ddns.err 2>/dev/null";	
		while( cmd | getline );
		if( $0 != "" && $0 != "0" ) {
			print ( "var service_ddns_status='"$0"'" );
			system ("rdb_set service.ddns.status '"$0"'");
		}
		else {
			print ( "var service_ddns_status=''" );
		}
		close( cmd );
	}
}
#! /usr/bin/awk -f
BEGIN {
	print ( "Content-type: text/html\n" );
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	cmd = "cat /tmp/wirelesshotspot.log 2>/dev/null";
	while( cmd | getline );
	close( cmd );
	if( $0 != "" && $0 != "0" ) {
		print ( "var service_wirelesshotspot_status='"$0"'" );
		system ("rdb_set service.wirelesshotspot.status '"$0"'");
	}
	else {
		cmd = "cat /tmp/wirelesshotspot.err 2>/dev/null";
		while( cmd | getline );
		if( $0 != "" && $0 != "0" ) {
			print ( "var service_wirelesshotspot_status='"$0"'" );
			system ("rdb_set service.wirelesshotspot.status '"$0"'");
		}
		else {
			print ( "var service_wirelesshotspot_status=''" );
		}
		close( cmd );
	}
}

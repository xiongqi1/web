#! /usr/bin/awk -f 

BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	#system ( "rdb_set service.system.reset 1" );
	#system ( "rdb_set service.systemmonitor.forcereset 1" );
	system ( "reboot" );
}
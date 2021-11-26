#! /usr/bin/awk -f 

# TODO: There is no grantee that the rules are sequential. The script has to match each indivisual rule to the count properly

BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	"rdb_get service.firewall.DefaultFirewallPolicy" | getline default_policy;	
	find_chains = 0;
	while( ("iptables -t filter -L -v -n" | getline) > 0 )
	{		
		if ( find_chains == 1 )
		{
			if ( index( $0, "pkts" )>0 )
				continue;
			if ( index( $0, "/* embedded_rules */" )>0 )
				continue;
			if ( $1=="Chain" )	
			{
				print "";
				exit
			}
			printf $1" "
		}
		if ( $1=="Chain" && $2=="macipport_filter" )
		{
			find_chains = 1;
		}
	}
	print "";
}
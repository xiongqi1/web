#! /usr/bin/awk -f 
BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	"rdb_get statistics.total_hours" | getline total_hours
	if (total_hours == "" || total_hours == "N/A" ) total_hours = 24;
	if(( ("rdb_get statistics.recv" | getline) > 0 ) && ($0!="" ))
		if( $0 != "" )
			printf("recv_total=%s;\n", $0);
		else
			printf("recv_total=0;\n");
	else
		printf("recv_total=0\n");	
	if(( ("rdb_get statistics.sent" | getline) > 0 ) && ($0!="" ))
		printf("sent_total=%s;\n", $0);	
	else
		printf("sent_total=0;\n");	
	"rdb_get statistics.sent_detail" | getline
	for(i=total_hours; i>0; i--)
	{
		if($i!="")
			printf("sent_detail[%s]=%s;\n", total_hours-i, $i);
		else
			printf("sent_detail[%s]=0;\n", total_hours-i);
	} 
	"rdb_get statistics.recv_detail" | getline
	for(i=total_hours; i>0; i--)
	{
		if($i!="")
			printf("recv_detail[%s]=%s;\n", total_hours-i, $i);
		else
			printf("recv_detail[%s]=0;\n", total_hours-i);
	} 
}


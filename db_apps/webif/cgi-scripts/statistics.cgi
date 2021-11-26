#! /usr/bin/awk -f 
function write_java_variable(javascript_var,javascript_val,suffix) {
	printf("\'%s\':\'%s\'%s",javascript_var,javascript_val,suffix);
}
BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	print ("var memtotal;")
	print ("var memfree;")
	cmd = sprintf("cat /proc/meminfo");
	while(( cmd | getline) > 0)
	{
		if($1 == "MemTotal:")
			printf("memtotal=\"%s %s\";\n",$2,$3);
		if($1 == "MemFree:")
			printf("memfree=\"%s %s\";\n",$2,$3);
	}
	close(cmd);
	
	printf ("var wan_array=[");
	cmd = sprintf("cat /proc/net/dev");
	while((cmd | getline) > 0)
	{
		if($1~"eth2.2")
		{
			# put name
			printf ("{")
			write_java_variable("recv_bytes",$2, ",");
			write_java_variable("recv_pkts",$3, ",");
			write_java_variable("tx_bytes",$10, ",");
			write_java_variable("tx_pkts",$11, "");
			printf ("}");
			printf ("];\n");
			break;
		}
	}
	close(cmd);
	printf ("var lan_array=[");
	cmd = sprintf("cat /proc/net/dev");
	while((cmd | getline) > 0)
	{
		if($1~"br0")
		{
			# put name
			printf ("{");
			write_java_variable("recv_bytes",$2, ",");
			write_java_variable("recv_pkts",$3, ",");
			write_java_variable("tx_bytes",$10, ",");
			write_java_variable("tx_pkts",$11, "");
			printf ("}");
			printf ("];\n");
			break;
		}
	}
	close(cmd);

	j =0;
	inf_num = 0;
	printf ("var allinfs_array=[");
	cmd = sprintf("cat /proc/net/dev");
	while((cmd | getline) > 0)
	{
		j ++ ;
		# Bypass the first two lines		
		if(j < 3)
			continue;
		inf_num ++;
		# The first element should not start with ","
		if(j > 3) 
			printf(",");
		printf ("{");
		split($1, inf, ":");
		write_java_variable("inf_name",inf[1], ",");
		if(inf[1]~"ra"){
			write_java_variable("recv_bytes",inf[2], ",");
			write_java_variable("recv_pkts",$2, ",");
			write_java_variable("tx_bytes",$9, ",");
			write_java_variable("tx_pkts",$10, "");
		}else{
			write_java_variable("recv_bytes",$2, ",");
			write_java_variable("recv_pkts",$3, ",");
			write_java_variable("tx_bytes",$10, ",");
			write_java_variable("tx_pkts",$11, "");
		}
		printf ("}\n");
	}
	printf ("];\n");
	printf ("var inf_num=%d;\n", inf_num);
}


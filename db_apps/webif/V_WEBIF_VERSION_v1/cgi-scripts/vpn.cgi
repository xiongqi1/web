#! /usr/bin/awk -f 

# Note this is only used by the V1 Web UI
#
# $1 = type of dev (openvpn.0 , pptp.0 , gre.0)
# $2 = 
#

function read_rdb_profile(profile_no,profile_var,default_value) {
	
	cmd = sprintf("rdb_get link.profile.%s.%s",profile_no,profile_var)
	if( (cmd | getline) == 1 ) {
		var=$0
	}
	else {
		var=default_value
	}
	close(cmd);
	
	return var
}

function write_java_variable(javascript_var,javascript_val,suffix) {
	printf("\"%s\":\"%s\"%s\n",javascript_var,javascript_val,suffix);
}

function write_rdb_profile_to_java_variable(profile_no,profile_var,default_value,suffix) {
	javascript_val=read_rdb_profile(profile_no,profile_var,default_value)
	write_java_variable(profile_var,javascript_val,suffix);
}

BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	print ("var st=[");
	i=0;
	j=0;
	profilenum = 0;
	while(++i)
	{		
		cmd = sprintf("rdb_get link.profile.%s.dev",i);
		if(( cmd | getline) > 0)
		{
			if( ($1=="pptp.0") || ($1=="gre.0") || ($1=="ipsec.0") || ($1=="openvpn.0") ) {
				# keep type
				type = $1;
				
				rdb_var = read_rdb_profile(i,"delflag","")
				if (rdb_var == "1")
					continue;
				# put seperator if any previous exists
				if(j++>0) 
					print(",");
				# put name
				printf("{\n")

				# read name
				rdb_var = read_rdb_profile(i,"name","")
				# write name
				write_java_variable("name",rdb_var,",");
			
				# put enable
				write_rdb_profile_to_java_variable(i,"enable","0",",");

				# put profile number
				write_java_variable("profilenum",i,",");
				# put profile type
				write_java_variable("type",type,",");

				# openvpn type
				write_rdb_profile_to_java_variable(i,"vpn_type","",",");
				
				# put network address
				write_rdb_profile_to_java_variable(i,"network_addr","",",");
				# put network mask
				write_rdb_profile_to_java_variable(i,"network_mask","",",");
				# put authentication type
				write_rdb_profile_to_java_variable(i,"vpn_authtype","",",");
				
				# TODO: print generated certificates - array
				
				# put user name and password
				write_rdb_profile_to_java_variable(i,"user","",",");
				write_rdb_profile_to_java_variable(i,"pass","",",");
				# put server address and port
				write_rdb_profile_to_java_variable(i,"serveraddress","",",");
				write_rdb_profile_to_java_variable(i,"serverport","",",");
				write_rdb_profile_to_java_variable(i,"serverporttype","",",");
				write_rdb_profile_to_java_variable(i,"defaultgw","0",",");

				write_rdb_profile_to_java_variable(i,"certi","",",");

				# TODO: print installed certificates - array
				
				# put network address
				write_rdb_profile_to_java_variable(i,"local_ipaddr","",",");
				write_rdb_profile_to_java_variable(i,"remote_ipaddr","",",");
				write_rdb_profile_to_java_variable(i,"remote_nwaddr","",",");
				write_rdb_profile_to_java_variable(i,"remote_nwmask","",",");
				#ipsec
				write_rdb_profile_to_java_variable(i,"remote_gateway","",",");
				write_rdb_profile_to_java_variable(i,"remote_lan","",",");
				write_rdb_profile_to_java_variable(i,"remote_lan2","",",");
				write_rdb_profile_to_java_variable(i,"remote_lan3","",",");
				write_rdb_profile_to_java_variable(i,"remote_lan4","",",");
				write_rdb_profile_to_java_variable(i,"remote_mask","",",");
				write_rdb_profile_to_java_variable(i,"local_gateway","",",");
				write_rdb_profile_to_java_variable(i,"local_lan","",",");
				write_rdb_profile_to_java_variable(i,"local_mask","",",");
				write_rdb_profile_to_java_variable(i,"enccap_protocol","",",");
				write_rdb_profile_to_java_variable(i,"ike_mode","",",");
				write_rdb_profile_to_java_variable(i,"pfs","",",");
				write_rdb_profile_to_java_variable(i,"ike_enc","",",");
				write_rdb_profile_to_java_variable(i,"ike_hash","",",");
				write_rdb_profile_to_java_variable(i,"ipsec_enc","",",");
				write_rdb_profile_to_java_variable(i,"ipsec_hash","",",");
				write_rdb_profile_to_java_variable(i,"ipsec_dhg","",",");
				write_rdb_profile_to_java_variable(i,"ipsec_method","none",",");
				write_rdb_profile_to_java_variable(i,"ipsec_dpd","",",");
				write_rdb_profile_to_java_variable(i,"psk_value","",",");
				write_rdb_profile_to_java_variable(i,"psk_remoteid","",",");
				write_rdb_profile_to_java_variable(i,"psk_localid","",",");
				write_rdb_profile_to_java_variable(i,"rsa_remoteid","",",");
				write_rdb_profile_to_java_variable(i,"rsa_localid","",",");
				write_rdb_profile_to_java_variable(i,"key_password","",",");
				write_rdb_profile_to_java_variable(i,"life_time","",",");
				write_rdb_profile_to_java_variable(i,"ike_time","",",");
				write_rdb_profile_to_java_variable(i,"dpd_time","",",");
				write_rdb_profile_to_java_variable(i,"dpd_timeout","",",");

				printf("\"metric\":");
				cmd = sprintf("rdb_get link.profile.%s.defaultroutemetric",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"30\",");
				}
				printf("\"authtype\":");
				cmd = sprintf("rdb_get link.profile.%s.authtype",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"any\",");
				}
				printf("\"snat\":");
				cmd = sprintf("rdb_get link.profile.%s.snat",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"0\",");
				}
				printf("\"sdefgw\":");
				cmd = sprintf("rdb_get link.profile.%s.default.defaultroutemetric",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"0\",");
				}
				printf("\"reconnect_delay\":");
				cmd = sprintf("rdb_get link.profile.%s.reconnect_delay",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"30\",");
				}
				printf("\"reconnect_retries\":");
				cmd = sprintf("rdb_get link.profile.%s.reconnect_retries",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"0\",");
				}
				printf("\"dnstopptp\":");
				cmd = sprintf("rdb_get link.profile.%s.default.dnstopptp",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"0\",");
				}
				printf("\"pppdebug\":");
				cmd = sprintf("rdb_get link.profile.%s.verbose_logging",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\",\n",$0);
				}
				else
				{
					print("\"0\",");
				}
				printf("\"ttl\":");
				cmd = sprintf("rdb_get link.profile.%s.ttl",i);
				if(( cmd | getline) > 0)
				{
					printf("\"%s\"\n",$0);
				}
				else
				{
					print("\"255\"");
				}

				printf("}\n")
			}
		}
		else break;
	}
	print ("];");
	print ("newprofilenum =" ((profilenum==0)?i:profilenum) ";");
}

#link.profile.n.dev;0;0;0;pptp
#link.profile.n.enable;0;0;0;
#link.profile.n.serveraddress;0;0;0;
#link.profile.n.default.defaultroutemetric;0;0;0;
#link.profile.n.default.dnstopptp;0;0;0;
#link.profile.n.username;0;0;0;
#link.profile.n.password;0;0;0;
#link.profile.n.reconnect_delay;0;0;0;30
#link.profile.n.reconnect_retries;0;0;0;1

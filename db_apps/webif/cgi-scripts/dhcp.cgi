#! /usr/bin/awk -f

function exec(mycmd) {
value="";
	while(mycmd | getline) {
		value = sprintf("%s%s", value, $0);
	}
	close(mycmd);
	return value;
}

# invoke base64 to encode given input field
# return encoded field
function base64Encode(field,   tempCmd,   tempFile,   cmd,   encodedField) {
	# prepare a temporary file
	tempCmd = "mktemp"
	tempCmd | getline tempFile
	close(tempCmd)
	if (tempFile == "") {
		# it should not happen, return empty string in this case
		return ""
	}
	# encode
	encodedField = ""
	cmd = "base64 | tr -d '\n' > " tempFile
	printf "%s", field | cmd
	close(cmd)
	getline encodedField < tempFile

	# delete temporary file
	tempCmd = "rm " tempFile
	system(tempCmd)

	return encodedField
}

BEGIN {
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	print ("var reservationdata=[");
	i=0;
	# If no query has been passed, then this command is getting the 
	# standard, static dhcp settings, otherwise, it is 
	# getting the settings for a vlan-bound dhcp server.
	profilenum = ENVIRON["QUERY_STRING"];

	hasprofile=0;

	if (length(profilenum) == 0) {
		hasprofile=0;
	}
	else {
		hasprofile=1;
	}
	profilenum = profilenum + 0;

	if (!hasprofile) {
		cmd = sprintf("rdb_get service.dhcp.static.%s", i);
	# interface.0.dhcp.static.x;0;0;0;computer name,mac address,ip address;enabled or disabled
	}
	else {
		cmd = sprintf("rdb_get vlan.%s.dhcp.static.%s", profilenum, i);
		# vlan.[0|1].dhcp.static.x;0;0;0;computer name,mac address,ip address;enabled or disabled
	}
	while( (cmd | getline) > 0 ) {
		n=split($0,myarr,",")
		if(n!=4) {
			continue;
		}
		if(i>0) print(",");
		printf("{\n");
		printf("\"computerName\":\"%s\",\n", base64Encode(myarr[1]));
		printf("\"mac\":\"%s\",\n", myarr[2]);
		printf("\"ip\":\"%s\",\n", myarr[3]);
		printf("\"enable\":\"%s\"\n", myarr[4]);
		printf("}");
		i++;
		if (!hasprofile) {
			cmd = sprintf("rdb_get service.dhcp.static.%s", i);
		}
		else {
			cmd = sprintf("rdb_get vlan.%s.dhcp.static.%s", profilenum, i);
		}
	}
	close( cmd );
	print ("\n];\n");
	print ("var leasesdata=[");
	i=1;

	# check with Bovine/Platypus2 file
	if (!hasprofile) {
		catcmd = "cat /tmp/dnsmasq.leases 2>/dev/null";
		lscmd = "ls /tmp/dnsmasq.leases 2>/dev/null";
		exist = exec(lscmd);
		if (exist == 0) {
			catcmd = "cat /tmp/var/lib/misc/dnsmasq.leases 2>/dev/null";
			}
	}
	else {	
		catcmd = sprintf("cat /tmp/dnsmasq-%s.leases 2>/dev/null", profilenum);
		lscmd = sprintf("ls /tmp/dnsmasq-%s.leases 2>/dev/null", profilenum);
		exist = exec(lscmd);
		if (exist == 0) {
			catcmd = sprintf("cat /tmp/var/lib/misc/dnsmasq-%s.leases 2>/dev/null", profilenum);
		}
	}

	while((catcmd | getline) > 0 ) {
		if(i>1) print(",");
		printf("{\n");
		mac=$2;
		ends=$1;
		ip=$3;
		hostname=$4;
		###checking the change of the system time
		if( ends<1355266923 ) {
			### the endtime was setup before the system time updated, adjust the leasetime by time offset.
			cmd = "cat /proc/uptime";
			cmd |getline;
			uptime = int($1);
			close(cmd);
			timeoffset = exec("date +%s")-uptime;
			ends += timeoffset;
		}
		printf("\"ends\":\"%s\",\n", ends);
		printf("\"mac\":\"%s\",\n", mac);
		printf("\"ip\":\"%s\",\n", ip);
		printf("\"hostname\":\"%s\"\n", base64Encode(hostname));
		printf("}");
		i++;
	}
	print ("\n];\n");
	close(catcmd);
	close(lscmd);
}

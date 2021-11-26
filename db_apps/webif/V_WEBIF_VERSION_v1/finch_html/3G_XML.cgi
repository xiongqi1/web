#! /usr/bin/awk -f 
BEGIN 
{
	print ("Content-type: text/xml\n");
	print ("<NetcommN3G009W>");

	print (" <Device>");
		xml_rdb( "SoftwareVersion", "version.Software");
		xml_rdb( "Bootloader", "version.bootloader");
		xml_rdb( "WirelessDriver", "version.wirelessdriver");
		xml_rdb( "SerialNumber", "wwan.0.imei");
	print (" </Device>");
	print (" <ThreeG>");
		xml_rdb( "Network", "wwan.0.system_network_status.network");
		xml_rdb( "Link", "link.if.ppp0.status");
		xml_rdb( "Mode", "wwan.0.contype");
		
		printf("  <SignalStrength>");
		cmd = sprintf("rdb_get wwan.0.radio.information.signal_strength");
		if(( cmd | getline) > 0)
		{
			csq = substr($0,0,index($0,"dBm")-1)+0.0;	
			if(csq == 0)
				imageidx = 0;
			else if(csq >= -77)
				imageidx = 5;
			else if(csq >= -86 )
				imageidx = 4;
			else if(csq >= -92 )
				imageidx = 3;
			else if(csq >= -101 )
				imageidx = 2;
			else if(csq >= -108)
				imageidx = 1;
			else if(csq >= -110 )
				imageidx = 1;
			else
				imageidx = 0;
			printf("%s",imageidx);
		}
		printf("</SignalStrength>\n");
#		xml_rdb( "SignalStrength", "wwan.0.radio.information.signal_strength");
		
		xml_rdb( "SimInfo", "wwan.0.sim.status.status");
		xml_rdb( "WANIP", "wlan.1.radius_server_ip");
		xml_rdb( "Downloaded", "");
		xml_rdb( "Uploaded", "");
		xml_rdb( "IMSI", "");
		xml_rdb( "IMEI", "");
		xml_rdb( "APN ", "link.profile.1.apn");
		xml_rdb( "USERNAME", "link.profile.1.user");
		xml_rdb( "Band", "wwan.0.system_network_status.current_band");
	print (" </ThreeG>");
	print (" <LAN>");
		xml_rdb( "LANIP", "interface.0.address");
		xml_rdb( "DefaultGateway", "interface.0.address");
		xml_rdb( "PrimaryDNS", "interface.0.dhcp.dns1.0");
		xml_rdb( "SecondaryDNS", "interface.0.dhcp.dns2.0");
	print (" </LAN>");
	print (" <Wifi>");
		xml_rdb( "SSID", "wlan.1.ssid");
		xml_rdb( "Authentication", "wlan.1.network_auth");
		if( $0 == WPA || $0 == WPA-PSK )
			xml_rdb( "Encryption", "wlan.1.wpa_encryption");
		else
			xml_rdb( "Encryption", "wlan.1.wep_encryption");
	print (" </Wifi>");
	print (" </NetcommN3G009W>");	
}

function xml_rdb(xml_name, rdb_val)
{
	printf("  <%s>", xml_name);
	cmd = sprintf("rdb_get %s", rdb_val);
	if(( cmd | getline) > 0)	printf("%s",$0);
	printf("</%s>\n", xml_name);
}
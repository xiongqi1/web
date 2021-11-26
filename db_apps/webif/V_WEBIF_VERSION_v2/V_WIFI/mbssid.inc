<%
mbssidST='';

function writeSsidArray() {
	var rdbKeyBase;
	function getRdb(key,defaultVal) {
		var val=get_single(rdbKeyBase+key);
		if (val=='N/A')
			val=defaultVal;
		return val;
	}
	function addSsidVar(key, defaultVal, suffix, base64) {
		mbssidST = mbssidST + '"'+key+'":';
		if (base64 == 1) {
			mbssidST = mbssidST + 'Base64.decode("' + base64_encode(getRdb( key ,defaultVal)) + '")';
		}
		else {
			mbssidST = mbssidST + '"' + getRdb( key ,defaultVal) + '"';
		}
		mbssidST = mbssidST + suffix+'\n';
	}

	function writeSsidEntry(i) {
		rdbKeyBase = 'wlan.'+i+'.';
		mbssidST = mbssidST + "{";
		addSsidVar("enable","0",",");
		addSsidVar("ssid","",",", 1);
		addSsidVar("hide_accesspoint","0",",");
		addSsidVar("network_key_id","0",",");
		addSsidVar("network_key1","",",", 1);
		addSsidVar("network_key2","",",", 1);
		addSsidVar("network_key3","",",", 1);
		addSsidVar("network_key4","",",", 1);
		addSsidVar("wep8021x","",",");
		addSsidVar("encryption_type","AES",",");
		addSsidVar("wpa_pre_shared_key","a1b2c3d4e5",",", 1);
		addSsidVar("wpa_group_rekey_interval","600",",");
		addSsidVar("radius_port","1812",",");
		addSsidVar("radius_key","NetComm",",", 1);
		addSsidVar("net_re_auth_interval","0",",");
		addSsidVar("radius_server_ip","0.0.0.0",",");
		addSsidVar("network_auth","WPA2PSK",",");
		addSsidVar("wpa2_preauthentication","off","");
		mbssidST = mbssidST + "}";
	}

	writeSsidEntry(0);
	for( var i=1; i<5; i++) {
		mbssidST = mbssidST +",";
		writeSsidEntry(i);
	}
}
writeSsidArray();
%>

<script>
var mbssidST=[@@mbssidST];
</script>

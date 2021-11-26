/* ================= port_status.js ================= */
#if (defined V_ETH_PORT_4pw_llll) || (defined V_ETH_PORT_4plllw_l)
function wanatp6OnSwitch() {
var objWanatp6=document.form1.selWanatp6;
var selIdx=objWanatp6.options.selectedIndex;
var selVal=objWanatp6.options[selIdx].value.toUpperCase();
var newWanatp6;
var wan_mode;
var lastWanatp6;
var f=document.form1;
var msg;

	if( selVal == "WAN" ) {
		newWanatp6=1;
		msg=_("wan lan help 6");
	}
	else if (selVal == "LAN" ) {
		newWanatp6=0;
		msg=_("wan lan help 5");
	}
	else
		return;

	wan_mode=<%temp_val=get_single("link.profile.0.wan_mode");%>"@@temp_val";
	if( wan_mode == "wan")
		lastWanatp6 = 1;
	else
		lastWanatp6 = 0;

	if( lastWanatp6 == newWanatp6) {
		document.getElementById("selWanatp6C").selected="yes";
		return false;
	}

	if(!confirm(msg)) {
		document.getElementById("selWanatp6C").selected="yes";
		return false;
	}
	if(selVal == "WAN")
		f.rdbCmd.value = "link.profile.0.wan_mode=wan";
	else
		f.rdbCmd.value = "link.profile.0.wan_mode=lan";
	f.submit();
}
#endif

function showPortStatus( str ) {
#if (defined V_ETH_PORT_8plllllllw_l)
	var doc = "";
	var all = new Array();
	all = str.split(",");
	for(var i=0; i < all.length; i++) {
		if(all[i].indexOf("Up")!= -1) {
			if(all[i].indexOf("1000Mb")!= -1)
				doc = "<img src='/images/1000.gif' width='18' height='12'>";
			else if(all[i].indexOf("100Mb")!= -1)
				doc = "<img src='/images/100.gif' width='18' height='12'>";
			else
				doc = "<img src='/images/10.gif' width='18' height='12'>";
		}
		else {
			doc = "<img src='/images/empty.gif' width='18' height='12'>";
		}
		document.getElementById( "port"+(i+1) ).innerHTML = doc+"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;LAN "+(i+1);
		document.getElementById( "port"+(i+1)+"_text" ).innerHTML = all[i];
	}
#elif (defined V_ETH_PORT_1pl || defined V_ETH_PORT_1)

	var doc = "";
	var all = new Array();
	all = str.split(",");
	if( all.length>0 && all[0].indexOf("Up")!= -1)
		document.getElementById( "port1" ).innerHTML = "LAN: &nbsp;&nbsp;<img src='/images/up.gif' width='15' height='15' alt='LAN'/>"
	else
		document.getElementById( "port1" ).innerHTML = "LAN: &nbsp;&nbsp;<img src='/images/down.gif' width='15' height='15' alt='LAN'/>"
	document.getElementById( "port1_text" ).innerHTML = all[0];

#elif (defined V_ETH_PORT_4pw_llll) || (defined V_ETH_PORT_4plllw_l)
	var doc = "";
	var e=document.getElementById( "port1" );
	var all = new Array();
	var wan_mode;

	if(str == "-1") {
		e.innerHTML = _("status notSupport");
		return ;
	}
	all = str.split(",");
	var j="1";
	for(var i=0; i < all.length-1; i+=3) {
	#if (defined V_ETH_PORT_4pw_llll)
		if(i==12) i+=3; //skip port 4
	#else
		if(i==9) i+=6;//skip port 3&4
	#endif
		if(all[i] == "1") {
			if(all[i+1] == "1000")
				doc = "<img src='/images/1000.gif'>";
			else if(all[i+1] == "100")
				doc = "<img src='/images/100.gif'>";
			else
				doc = "<img src='/images/10.gif'>";
			if( all[i+2] == "F" )
				doc += "&nbsp;&nbsp;&nbsp;&nbsp;"+_("full");
			else //( all[i+2] == "H" )
				doc += "&nbsp;&nbsp;&nbsp;&nbsp;"+_("half");
		}
		else if(all[i] == "0") {
				doc = "<img src='/images/empty.gif'>";
		}

		e=document.getElementById( "port"+j );
		if(e)
			e.innerHTML = doc;
		j++;
	}
	wan_mode=<%temp_val=get_single("link.profile.0.wan_mode");%>"@@temp_val";
	e=document.getElementById( "portwan");
	if(e) {
		if( wan_mode == "wan")
			e.innerHTML = "&nbsp;<b> WAN </b> &nbsp;<small>(Ethernet Port)</small>";
		else {
	#if (defined V_ETH_PORT_4pw_llll)
			e.innerHTML = "&nbsp;<b> LAN 1 </b> &nbsp;<small>(Ethernet Port)</small>";
	#else
			e.innerHTML = "&nbsp;<b> LAN 1 </b> &nbsp;<small>(Ethernet Port)</small>";
	#endif
		}
	}
	e=document.getElementById( "selWanatp6C");
	if(e) {
		if( wan_mode == "wan")
			e.innerHTML =_("current wan2");
		else
			e.innerHTML =_("current lan");
	}
#elif (defined V_ETH_PORT_2plw_l)
	var all = new Array();
	all = str.split(",");
	for(var i=0; i < all.length; i++) {
		if(all[i].indexOf("Up")!= -1) {
			$("#port"+(i+1)+"_text").html("Up"+all[i].substr(3, all[i].indexOf("Mbps")+11));
		}
		else {
			$("#port"+(i+1)+"_text").html("Down");
		}
	}
#elif (defined V_ETH_PORT_2pll)
	var doc = "";
	var e=document.getElementById( "port1" );
	var all = new Array();
	var wan_mode;

	if(str == "-1") {
		e.innerHTML = _("status notSupport");
		return ;
	}
	all = str.split(",");
	var j="1";
	for(var i=0; i < 4; i+=3) { //only shows port 1&2
		//doc = "LAN"+j+": &nbsp;&nbsp;&nbsp;"+;
		if(all[i] == "1") {
			if(all[i+1] == "1000")
				doc = "<img src='/images/1000.gif' width='20' height='13'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; LAN"+j+": &nbsp;&nbsp;&nbsp;"+"Up &nbsp; / &nbsp; 1000.0 &nbsp; /";
			else if(all[i+1] == "100")
				doc = "<img src='/images/100.gif' width='20' height='13'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; LAN"+j+": &nbsp;&nbsp;&nbsp;"+"Up &nbsp; / &nbsp; 100.0 Mbps &nbsp; /";
			else
				doc = "<img src='/images/10.gif' width='20' height='13'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; LAN"+j+": &nbsp;&nbsp;&nbsp;"+"Up &nbsp; / &nbsp; 10.0 Mbps &nbsp; /";
			doc+=" &nbsp; "+(all[i+2]=="F"?_("full"):_("half"))

		}
		else if(all[i] == "0") {
			doc = "<img src='/images/empty.gif' width='20' height='13'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; LAN"+j+": &nbsp;&nbsp;&nbsp;"+_("cable unplugged");
		}
		e=document.getElementById( "port"+j );
		if(e)
			e.innerHTML = doc;
		j++;
	}
#endif
}
/* =================end of port_status.js ================= */

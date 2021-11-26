<!-- #### 3G Failover Settings host script #### -->
<%
	form['failoverType']=get_single('service.failover.mode');
	form['alwaysConnect']=get_single('service.failover.alwaysConnectWWAN');

	form['pingdst']=get_single('service.failover.destaddress');
	form['pingdst2']=get_single('service.failover.destaddress2');

	form['pingtimer']=get_single('service.failover.periodicpingtimer');
	form['pingacctmr']=get_single('service.failover.pingacceleratedtimer');
	form['pingfailcnt']=get_single('service.failover.failcount');
	form['pingsucccnt']=get_single('service.failover.succcount');

	wan_mode=get_single('link.profile.0.wan_mode');
%>

<!-- #### 3G Failover Settings client script #### -->
<script language="JavaScript">

function showElementById(id,show) {
	document.getElementById(id).style["display"]=show?"":"none";
}
	
function failoverWwanChange() {
	var wwanConnType;

	wwanConnType=document.getElementById("alwaysConnect").value;

	// show the description
	// show the description
	if (wwanConnType == 'enable')
		document.getElementById("failoverWwanDesc").innerHTML=_("failover wwan wlandesc enable");
	else
		document.getElementById("failoverWwanDesc").innerHTML=_("failover wwan wlandesc disable");
}

function failoverTypeOnChange() {
	var failoverType;

	failoverType=document.getElementById("failoverType").value;

#if (defined V_PRODUCT_swi_nhd1w)
	// hide wwan configuration
	showElementById("tblFailover3",false );
#else
	// show wwan configuration if fail-over mode
	showElementById("tblFailover3",("@@wan_mode" == "wan") && (failoverType == "ping" || failoverType == "phylink") );
#endif	
	// show ping configuration if ping mode
	showElementById("tblFailover2",("@@wan_mode" == "wan") && (failoverType == "ping") );

	// show the description
	if (failoverType == 'wan')
		document.getElementById("failoverDesc").innerHTML=_("failover desc wan");
	else if (failoverType == 'wwan')
		document.getElementById("failoverDesc").innerHTML=_("failover desc wwan");
	else if (failoverType == 'phylink')
		document.getElementById("failoverDesc").innerHTML=_("failover desc phylink");
	else if (failoverType == 'ping')
		document.getElementById("failoverDesc").innerHTML=_("failover desc ping");
}

function checkFailOverItems() {       
	var f=document.getElementById("form");

	failoverType=document.getElementById("failoverType").value;

	if(failoverType == "ping" ) {
		try {
			// bypass if failover is disabled
			if(f.failoverType.value=="disable")
				return true;

			// check if any of timer is set up
			if(f.pingdst.value=="" && f.pingdst2.value=="") {
				alert(_("warningMsg04"));
				f.pingdst.select();
				return false;
			}

			if ( f.pingtimer.value=="" || parseInt(f.pingtimer.value) < 3 || parseInt(f.pingtimer.value) > 65535 ) {
				alert(_("monitor warningMsg5b"));
				f.pingtimer.select();
				return false;
			}

			if ( f.pingacctmr.value=="" || parseInt(f.pingacctmr.value) < 2 || parseInt(f.pingacctmr.value) > 65535 ) {
				alert(_("monitor warningMsg6b"));
				f.pingacctmr.select();
				return false;
			}

			if ( f.pingfailcnt.value=="" || parseInt(f.pingfailcnt.value) < 1 || parseInt(f.pingfailcnt.value) > 65535 ) {
				alert(_("monitor warningMsg3 eti"));
				f.pingfailcnt.select();
				return false;
			}

			if ( f.pingsucccnt.value=="" || parseInt(f.pingsucccnt.value) < 1 || parseInt(f.pingsucccnt.value) > 65535 ) {
				alert(_("monitor warningMsg3b"));
				f.pingsucccnt.select();
				return false;
			}

			return true;
		}
		catch(e) {
			//alert(e.message);
			return false;
		}
	}
	return true;
} 
</script>

<!-- #### 3G Failover Settings UI #### -->
<table width="90%" id="tblFailover4">
    <tr><th><script language="JavaScript">document.write(_("failover settings"))</script></th></tr>
</table>

<table width="75%" id="tblFailover1">

<tr> 
	<td width="22%"><script language="JavaScript">document.write(_("failover type"))</script></td>
	<td>
		<select id="failoverType" name="failoverType" onChange="failoverTypeOnChange()">
			<option value="wan"><script language="JavaScript">document.write(_("failover wan"))</script></option>
			<option value="wwan"><script language="JavaScript">document.write(_("failover wwan"))</script></option>
			<option value="phylink"><script language="JavaScript">document.write(_("failover phylink"))</script></option>
			<option value="ping"><script language="JavaScript">document.write(_("failover ping"))</script></option>
		</select>
		<p></p>
		<p id="failoverDesc"></p>
	</td>
</tr>

</table>

<table width="75%" id="tblFailover3">
<tr>
	<td width="22%"><script language="JavaScript">document.write(_("failover wwan always"));</script></td>
	<td>
		<select id="alwaysConnect" name="alwaysConnect" onChange="failoverWwanChange()">
			<option value="disable"><script language="JavaScript">document.write(_("failover wwan dial"))</script></option>
			<option value="enable"><script language="JavaScript">document.write(_("failover wwan alwayson"))</script></option>
		</select>
		<p></p>
		<p id="failoverWwanDesc"></p>
	</td>
</tr>
</table>

<table width="75%" id="tblFailover2">
<tr>
	<td width="22%"><script language="JavaScript">document.write(_("monitor destinationAddress"));</script></td>
	<td><input type=text name='pingdst' size=32 maxlength=64 value="@@form['pingdst']">&nbsp;</td>
</tr>
<tr>
	<td><script language="JavaScript">document.write(_("monitor secondAddress"));</script></td>
	<td><input type=text name='pingdst2' size=32 maxlength=64 value="@@form['pingdst2']">&nbsp;</td>
</tr>
<tr>
	<td><script language="JavaScript">document.write(_("monitor pingTimer"));</script></td>
	<td> <input type=text name='pingtimer' size=8 maxLength=5 value="@@form['pingtimer']" onkeyup=NumfieldEntry(this);> &nbsp;<script language="JavaScript">document.write(_("failover period"));</script></td>
</tr>
<tr> 
	<td><script language="JavaScript">document.write(_("monitor acceleratedTimer"));</script></td>
	<td><input type=text name='pingacctmr' size=8 maxLength=5 value="@@form['pingacctmr']" onkeyup=NumfieldEntry(this);> &nbsp;<script language="JavaScript">document.write(_("failover period2"));</script></td>
</tr>
<tr>
	<td><script language="JavaScript">document.write(_("monitor failCount"));</script></td>
	<td> <input type=text name='pingfailcnt' size=8 maxLength=5 value="@@form['pingfailcnt']" onkeyup=NumfieldEntry(this); ID="Text1"> &nbsp;<script language="JavaScript">document.write(_("failover count"));</script></td>
</tr>
<tr>
	<td><script language="JavaScript">document.write(_("monitor succCount"));</script></td>
	<td> <input type=text name='pingsucccnt' size=8 maxLength=5 value="@@form['pingsucccnt']" onkeyup=NumfieldEntry(this); ID="Text1"> &nbsp;<script language="JavaScript">document.write(_("failover count"));</script></td>
</tr>
</table>

<!-- #### 3G Failover Settings finalization javascript #### -->
<script language="JavaScript">
var failoverType;
var alwaysConnect;
	// exchange server variables to javascript ones
	failoverType="@@form['failoverType']";
	if(failoverType=="" || failoverType=="N/A")
		failoverType="phylink";

	alwaysConnect="@@form['alwaysConnect']";
	if(alwaysConnect=="" || alwaysConnect=="N/A")
		alwaysConnect="disable";

	// exchange failvertype to UI
	document.getElementById("failoverType").value=failoverType;
	failoverTypeOnChange();

	// exchange alwaysConnect to 
	document.getElementById("alwaysConnect").value=alwaysConnect;
	failoverWwanChange();

	// hide if not wan mode
	showElementById("tblFailover4", "@@wan_mode" == "wan");
	showElementById("tblFailover1", "@@wan_mode" == "wan");
</script>
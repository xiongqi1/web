<!-- #### wwan failover notification  #### -->
<%

	// failover notification type
	form['FOselect_id']=get_single('wwan.0.FailoverNoti.selectedidx');
	// failover notification device name
	form['deviceName']=get_single('wwan.0.FailoverNoti.DeviceName');

	syslog(7,"failovernoti-body.inc: FOselect_id="+form['FOselect_id'])

	// email configuration
	form['Email_to']=get_single('wwan.0.FailoverNoti.Email_to');
	form['Email_cc']=get_single('wwan.0.FailoverNoti.Email_cc');
	form['Email_from']=get_single('wwan.0.FailoverNoti.Email_from');
	form['Email_serveraddr']=get_single('wwan.0.FailoverNoti.Email_serveraddr');
	form['Email_serverport']=get_single('wwan.0.FailoverNoti.Email_serverport');
	form['Email_username']=get_single('wwan.0.FailoverNoti.Email_username');
	form['Email_passwd']=get_single('wwan.0.FailoverNoti.Email_passwd');

	// sms configuration
	form['SMS_to']=get_single('wwan.0.FailoverNoti.SMS_to');

	// set default configuration - smtp port
	if(form['Email_serverport']=="")
		form['Email_serverport']="25";

	// set default configuration - device name
	if(form['deviceName']=="")
		form['deviceName']=get_single('system.identity');

	wan_mode=get_single('link.profile.0.wan_mode');
	failover_mode=get_single('service.failover.mode');

	syslog(7,"failovernoti-body.inc: wan_mode='"+wan_mode+"' failover_mode='"+failover_mode+"'");

	// enable failover notification if failover is enabled
	if( (wan_mode == "wan") && (failover_mode == "phylink" || failover_mode == "ping") ) {
		disablenoti=0;

		syslog(7,"failovernoti-body.inc: enable notification");
	}
	else  {
		disablenoti=1;

		syslog(7,"failovernoti-body.inc: disable notification");
	}
%>

<%
	// show only when wan mode is wan
	if( wan_mode == "wan" ) {
%>

<script language="JavaScript">

function create_ajax_obj() {
	var ajaxHttp;    

	try {
		ajaxHttp = new ActiveXObject("Microsoft.XMLHTTP");
	}
	catch (e) {  // Internet Explorer  
		try {
			ajaxHttp = new ActiveXObject("Msxml2.XMLHTTP");
		}
		catch (e) {
			try {
				// Firefox, Opera 8.0+, Safari
				ajaxHttp = new XMLHttpRequest();

			}
			catch (e) {
				//alert("Your browser does not support AJAX!");
				return false;
			}
		}
	}      
	return ajaxHttp;
}

function sendTestSMSBtnOnClick() {
	var req;
	// build request
	req="/cgi-bin/failover_SMS_noti.cgi";
	content="SMS_to="+document.formFONoti.SMS_to.value+"&"+
		"deviceName="+document.formFONoti.deviceName.value;


	var ajaxTestSMS = create_ajax_obj();

	ajaxTestSMS.onreadystatechange = function() {
		if (ajaxTestSMS.readyState != 4)
			return;

		// enable form buttons
		document.getElementById("FOApply").disabled = false;
		document.getElementById("wCancel4").disabled = false;
		document.getElementById("sendTestSMSBtn").disabled = false;

		// apply normal cursor
		document.body.style.cursor = 'default';

		// bypass if error
		if (ajaxTestSMS.status != 200) {
			return;
		}

		var result;
		eval(ajaxTestSMS.responseText);

		if ( typeof(result)=="undefined" || result!=0 ) {
				alert(_("sendTestSMSError"));
				return;
		}
		alert(_("sendTestSMSSucc"));
	}
	
	// disable form buttons
	document.getElementById("FOApply").disabled = true;
	document.getElementById("wCancel4").disabled = true;
	document.getElementById("sendTestSMSBtn").disabled = true;
	// apply wait cursour
	document.body.style.cursor = 'wait';

	ajaxTestSMS.open('POST', req, true);
	ajaxTestSMS.send(content);
}

function sendTestEmailBtnOnClick() {
	var req;

	// build request
	req="/cgi-bin/failover_SMTP_noti.cgi";

	content="Email_to="+document.formFONoti.Email_to.value+"&"+
		"Email_cc="+document.formFONoti.Email_cc.value+"&"+
		"Email_from="+document.formFONoti.Email_from.value+"&"+
		"Email_serveraddr="+document.formFONoti.Email_serveraddr.value+"&"+
		"Email_serverport="+document.formFONoti.Email_serverport.value+"&"+
		"Email_username="+document.formFONoti.Email_username.value+"&"+
		"Email_passwd="+document.formFONoti.Email_passwd.value+"&"+
		"deviceName="+document.formFONoti.deviceName.value;

	var ajaxTestEmail = create_ajax_obj();

	ajaxTestEmail.onreadystatechange = function() {
		if (ajaxTestEmail.readyState != 4)
			return;

		// enable form buttons
		document.getElementById("FOApply").disabled = false;
		document.getElementById("wCancel4").disabled = false;
		document.getElementById("sendTestEmailBtn").disabled = false;

		// apply normal cursor
		document.body.style.cursor = 'default';

		// bypass if error
		if (ajaxTestEmail.status != 200) {
			return;
		}

		var result;
		eval(ajaxTestEmail.responseText);

		if ( typeof(result)=="undefined" || result!=0 ) {
				alert(_("sendTestEmailError"));
				return;
		}
		alert(_("sendTestEmailSucc"));
	}

	// disable form buttons
	document.getElementById("FOApply").disabled = true;
	document.getElementById("wCancel4").disabled = true;
	document.getElementById("sendTestEmailBtn").disabled = true;
	// apply wait cursour
	document.body.style.cursor = 'wait';

	ajaxTestEmail.open('POST', req, true);
	ajaxTestEmail.send(content);
}

function set_element_value_by_id(id,vale) {
	var element;

	element=document.getElementById(id);
	if(element)
		element.value=value;
}

function set_element_value_by_resource(id,resource) {
	var element;

	element=document.getElementById(id);
	if(element) 
		element.value = _(resource);
}

function show_element_by_id(id,show) {
	var element;

	element=document.getElementById(id);
	if(element)
		element.style['display']=show?'':'none';
}

function enable_element_by_id(id,enable) {
	var element;

	element=document.getElementById(id);
	if(element)
		element.disabled=!enable;
}

function FOSelectchange(value) {
	var element;

	element=document.getElementById( "FOselect_id" );

	if(typeof(value)!="undefined" && value!=null) {
		element.value=value;
	}

	switch(element.value) {
		case "0": //disable
			show_element_by_id( "sms_trid",false);
			show_element_by_id( "email_trid",false);
			show_element_by_id( "chooseName",false);
			break;

		case "1": //SMS Only
			show_element_by_id( "chooseName",true);
			show_element_by_id( "sms_trid",true);
			show_element_by_id( "email_trid",false);
			break;

		case "2": //Email Only
			show_element_by_id( "chooseName",true);
			show_element_by_id( "sms_trid",false);
			show_element_by_id( "email_trid",true);

			// apply default port
			if(document.formFONoti.Email_serverport.value=="")
				document.formFONoti.Email_serverport.value="25";
			break;

		case "3": //Both
			show_element_by_id( "chooseName",true);
			show_element_by_id( "sms_trid",true);
			show_element_by_id( "email_trid",true);

			// apply default port
			if(document.formFONoti.Email_serverport.value=="")
				document.formFONoti.Email_serverport.value="25";
			break;
	}
}

function FONoticlick() {
	element=document.getElementById( "FOselect_id" );

	// check email password if email alone or sms/email mode
	if( (element.value==2) || (element.value==3) ) {

		if (document.formFONoti.Email_passwd.value != document.formFONoti.Email_passwdConf.value) {
			document.formFONoti.Email_passwd.value="";
			document.formFONoti.Email_passwdConf.value="";
			alert(_("wwan warningMsg2"));
			return;
		}

		Email_port=document.formFONoti.Email_serverport.value;
		if (Email_port <1 || Email_port > 65535) {
			alert("ERROR: The port of mail server is out of range!!");
			document.formFONoti.Email_serverport.value="";
			return;
		}
	}

	document.formFONoti.submit();
}

function SMSAddrfieldEntry(field) {

	if (field.value.match(/[^\\+0-9;]/g) != null) {
		field.value=field.value.replace(/[^\\+0-9;]/g,"");
	}

	var resultArray = field.value.match(/;/g);

	// bypass if the filed does not have any 
	if(resultArray==null) {
		return;
	}

	if(resultArray.length > 10) {
		var tempArray = field.value.split(";");
		tempstring="";
		for(i=0; i < 10; i ++) {
			tempstring=tempstring.concat(tempArray[i]);
			tempstring=tempstring.concat(";");
		}
		field.value=tempstring;
	}
	else if ((resultArray.length == 10) && (field.value.length-1 > field.value.lastIndexOf(";"))) {
		field.value=field.value.slice(0, field.value.lastIndexOf(";")+1);
	}
}

function EmailportfieldEntry(field) {
	if (field.value.match(/[^0-9]/g) != null) {
		field.value=field.value.replace(/[^0-9]/g,"");
	}
}

</script>

<div id="wwanSettings3" >
<form name="formFONoti" class="content" method="POST" autocomplete="off" action="@@requst['SCRIPT_NAME']?form_name=formFONoti"  >
<input type="hidden" name="temp_val" value="@@form['temp_val']"/>

<table>
	<tr><th><script language="JavaScript">document.write(_("failover noti settings"))</script></th></tr>
</table>

<table border="0" cellpadding="2" cellspacing="1">	
<tr>
	<td width="30%" class="head"><script language="JavaScript">document.write(_("failover noti settings"))</script></td>
	<td>
		<select id="FOselect_id" name="FOselect_id" value="@@form['FOselect_id']" onchange="FOSelectchange();">
			<option value="0">&nbsp;<script language="JavaScript">document.write(_("disable"))</script>
			#ifdef V_SMS
			<option value="1">&nbsp;<script language="JavaScript">document.write(_("sms only"))</script>
			#endif
			<option value="2">&nbsp;<script language="JavaScript">document.write(_("email only"))</script>
			#ifdef V_SMS
			<option value="3">&nbsp;<script language="JavaScript">document.write(_("both"))</script>
			#endif
		</select> 
	<div id="disablenoti"><h2><script language="JavaScript">document.write(_("failover noti warning01"))</script></div>
	</td>
</tr>
</table>

<table id="chooseName" border="0" cellpadding="2" cellspacing="1">
<tr>
	<td width="30%" class="head"><script language="JavaScript">document.write(_("choose name")+"<br/>"+_("max 30 char"))</script></td>
	<td><input type="text" size="35%" maxlength="30" name="deviceName" value="@@form['deviceName']"></td>
</tr>  
</table>

<table border="0" cellpadding="2" cellspacing="1" id="sms_trid">
<tr> 
	<td width="30%" ><script language="JavaScript">document.write(_("sms setting"))</script></td>
	<td>
		<table border="0" cellpadding="2" cellspacing="1">
		<tr>
			<td width="30%">
				<script language="JavaScript">document.write(_("sms dest addr")+":")</script>
			</td>
			<td><input type="text" name="SMS_to" value="@@form['SMS_to']" size="35%" onkeyup=SMSAddrfieldEntry(this); /></td>
		</tr>
		<tr>
			<td width="30%"></td>
			<td>
				<script language="JavaScript">document.write("[ "+_("sms to notify explain2")+" ; "+_("quote right")+" ]")</script>
			</td>
		</tr>
		</table>
		<table border="0" cellpadding="2" cellspacing="1">
		<tr>
			<td width="30%"><script language="JavaScript">document.write(_("sendTestSMSText"))</script></td>
			<td><input type="button" id="sendTestSMSBtn" name="sendTestSMSBtn" value="" size="35%" onClick="sendTestSMSBtnOnClick()"/></td>
		</tr>
		</table>
	</td>
</tr>

</table>
<table border="0" cellpadding="2" cellspacing="1" id="email_trid">
<tr> 
	<td width="30%"><script language="JavaScript">document.write(_("email setting"))</script></td>
	<td>
		<table border="0" cellpadding="2" cellspacing="1">
		<tr>
			<td width="30%">
				<script language="JavaScript">document.write(_("email_to"))</script>
			</td>
			<td><input type="text" name="Email_to" value="@@form['Email_to']" size="35%"/></td>
		</tr>
		<tr>
			<td width="30%"></td>
			<td>
				<script language="JavaScript">document.write("[ "+_("email to notify explain2")+" ; "+_("quote right")+"]")</script>
			</td>
		</tr>
		</table>
#if defined(SKIN_ts)
		<table style="display:none">
#else
		<table>
#endif
		<tr >
			<td width="30%"><script language="JavaScript">document.write(_("cc seperator"))</script>( ; )</td>
			<td><input type="text" name="Email_cc" value="@@form['Email_cc']" size="35%"/></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("from"))</script></td>
			<td><input type="text" name="Email_from" value="@@form['Email_from']" size="35%"/></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("smtp server addr"))</script></td>
			<td><input type="text" name="Email_serveraddr" value="@@form['Email_serveraddr']" size="35%"/></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("smtp server port"))</script></td>
			<td><input type="text" name="Email_serverport" value="@@form['Email_serverport']" size="35%" onkeyup=EmailportfieldEntry(this); /></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("GUI username"))</script></td>
			<td><input type="text" name="Email_username" value="@@form['Email_username']" size="35%"/></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("password"))</script></td>
			<td><input type="password" name="Email_passwd" value="@@form['Email_passwd']" size="35%"/></td>
		</tr>
		<tr>
			<td><script language="JavaScript">document.write(_("confirmPassword"))</script></td>
			<td><input type="password" name="Email_passwdConf" value="@@form['Email_passwd']" size="35%"/></td>
		</tr>
		</table>

		<table border="0" cellpadding="2" cellspacing="1">
		<tr>
			<td width="30%"><script language="JavaScript">document.write(_("sendTestEmailText"))</script></td>
			<td><input type="button" id="sendTestEmailBtn" name="sendTestEmailBtn" value="" size="35%" onClick="sendTestEmailBtnOnClick()"/></td>
		</tr>
		</table>
	</td>
</tr>
</table>
<table width="540" cellpadding="2" cellspacing="1">
<tr align="center">
	<td>
	<input type=button style="width:120px;" value="Apply" name='FOApply' id='FOApply'  onClick="FONoticlick()">&nbsp;&nbsp;
	<input type=reset  style="width:120px;" value="Cancel" name='wCancel4' id='wCancel4' onClick="window.location.reload()">
	</td>
</tr>	
</table>
</form>
</div>

<script language="JavaScript">
	// disable notification selection if failover is disabled
	enable_element_by_id("FOselect_id",@@disablenoti==0);
	// show the instruction when failover is disabled
	show_element_by_id("disablenoti",@@disablenoti==1);

	// apply local language
	set_element_value_by_resource("sendTestEmailBtn","Send Test Email");
	set_element_value_by_resource("sendTestSMSBtn","Send Test SMS");
	set_element_value_by_resource("FOApply","Apply");
	set_element_value_by_resource("wCancel4","Cancel");

	// set selectbox
	FOSelectchange((@@disablenoti==1)?0:@@form['FOselect_id']);
</script>

<%
	}
%>
#if (defined PLATFORM_Platypus)
var platform ="Platypus";
#elif (defined PLATFORM_Platypus2)
var platform ="Platypus2";
#elif (defined PLATFORM_Avian)
var platform ="Avian";
#else
var platform ="Bovine";
#endif
var uiBase = 'adv';

var http_request = false;
var cmd_line;
var UssdDialString;
var UssdStatus;
var UssdMsgBody;
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function CommonRequestHandler()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	//alert("http_request.readyState = "+http_request.readyState+", http_request.responseText.length = "+http_request.responseText.length);
	if(http_request.readyState == 4  && http_request.status == 200 && http_request.responseText.length > 0)
	{
		ajaxerror = 0;
		eval( http_request.responseText );
		//alert("http_request.responseText = "+http_request.responseText);
		return true;
	}
	else if (0)
	{
		if(http_request.responseText.length <= 0)
		{
			alert("http_request.responseText.length <= 0");
		}
		else if(http_request.readyState != 4)
		{
			alert("http_request.readyState!=4");
		}
		return false;
	}
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function ClearRequestHandler()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	http_request.responseText.clear;
	http_request.close;
	http_request=0;
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function DisplayControl(control)
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	var animation, button_control, i;
	if (control == 'disable') {
		animation = '';
		button_control = 'disabled';
	}
	else {
		animation = 'none';
		button_control = '';
	}
	document.getElementById( "waitanimation" ).style['display']=animation;
	if( uiBase != 'basic' ) {
		document.getElementById("SelectButton").disabled = button_control;
		document.getElementById("EndButton").disabled = button_control;
		if (control == 'enable') {
			if (document.USSD.ussd_status.value == 'Active') {
				document.getElementById("SelectButton").value = "  "+_("send msg")+"  ";
				document.getElementById("EndButton").style['display']='';
				document.USSD.ussd_selection.value = '';
			}
			else {
				document.getElementById("SelectButton").value = "  "+_("start session")+"  ";
				document.getElementById("EndButton").style['display']='none';
				document.USSD.ussd_selection.value = _("ussd dial string");		// "Enter USSD dial string here"
			}
		}
	}
	else {
		DisplayControl_basic( button_control );
	}
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function UssdConfigGet()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	cmd_line = "/cgi-bin/ussd.cgi?CMD=USSD_CONF_GET";
	makeRequest(cmd_line, "n/a", UssdConfigGetHandler);
	DisplayControl('disable');
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function UssdConfigGetHandler()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	if (CommonRequestHandler()) {
		document.USSD.ussd_status.value = UssdStatus;
		document.USSD.ussd_message.value = UssdMsgBody;
		ClearRequestHandler();
		DisplayControl('enable');
		if( uiBase == 'basic' && document.USSD.ussd_status.value == "Active") {
			UssdAction('end');
		}
	}
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function IsPromptDialString()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	if (document.USSD.ussd_selection.value == _("ussd dial string"))	// "Enter USSD dial string here"
		return 1;
	return 0;
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function ClearUssdSelection()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	if (document.USSD.ussd_status.value == 'Inactive' && IsPromptDialString())
	{
		document.USSD.ussd_selection.value = '';
	}
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function UssdAction(action)
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	if( uiBase == 'basic' ) {
		document.USSD.ussd_cmd.value = action;
	}
	if (action == 'end')
	{
		cmd_line="/cgi-bin/ussd.cgi?CMD=USSD_END";
	}
	else
	{
	if (document.USSD.ussd_status.value == 'Inactive')
	{
		if ( uiBase !="basic" && IsPromptDialString())
		{
			alert(_("ussd warning07"));		// "Enter USSD dial string and press Start Session button!"
			return;
		}
		else if (action.length == 0)
		{
			alert(_("ussd warning01"));		// "Empty dial string!"
			return;
		}
		else
		{
			cmd_line="/cgi-bin/ussd.cgi?CMD=USSD_START&UssdMenuSelection="+encodeUrl(action);
		}
	}
	else
	{
		cmd_line="/cgi-bin/ussd.cgi?CMD=USSD_SELECTION&UssdMenuSelection="+action;
		if (action < 0 || isNaN(action) || action.length == 0)
		{
			alert(_("ussd warning03"));		// "Menu selection is out of range!"
			document.USSD.ussd_selection.value = '';
			return;
		}
	}
	}
	makeRequest(cmd_line, "n/a", UssdActionHandler);
	DisplayControl('disable');
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
function UssdActionHandler()
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
{
	if (CommonRequestHandler()) {
		document.USSD.ussd_status.value = UssdStatus;
		document.USSD.ussd_message.value = UssdMsgBody;
		if( uiBase != 'basic' )
			document.USSD.ussd_selection.value = '';
		if (UssdCmdResult == 'failure') {
			if (platform == "Platypus" || platform == "Platypus2")
				alert(_("ussd warning08")+"\r\n"+_("ussd warning05")+"\r\n"+_("ussd warning06"));
			else if (platform == "Avian")
				alert(_("ussd warning04")+"\r\n"+_("ussd warning05")+"\r\n"+_("ussd warning06"));
			else
				alert("The router has not received a response from the server.\r\n"+
									"Please check that the message you have entered is valid, and try again.\r\n"+
									"You may need to end the session and begin a new one before proceeding.");
		}
		ClearRequestHandler();
		DisplayControl('enable');
		if( uiBase == 'basic' ) {
			if( document.USSD.ussd_status.value == "Active" ) {
				UssdMsgBody_prv=UssdMsgBody;
				UssdAction('end');
			}
			else {
				document.USSD.ussd_message.value = UssdMsgBody_prv;
			}
		}
	}
}


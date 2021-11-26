/*****************net_util.js************************/

function WinExpIP(field,event) {
	if( field.value=="") {
		if($(field).hasClass("required")) {
			$(field).valid();
		}
	}
	else {
		if(!isValidIpEntry(field,event))
			validate_alert( "", _("dhcp warningMsg11"));
		else
			clear_alert();
	}
}

function WinExpIP_1(field,event) {
	if( field.value=="") {
		if($(field).hasClass("required")) {
			$(field).valid();
		}
	}
	else {
		switch(isValidIpEntry_1(field,event)) {
		case -1:
			validate_alert( "", _("dhcp warningMsg12"));
		break;
		case -2:
			validate_alert( "", _("dhcp warningMsg13"));
		break;
		default:
			clear_alert();
		break;
		}
	}
}

function hostMin(ip, mask) {
	var host_array=[];
	ip_array = ip.split('.');
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		host_array[i] = parseInt(ip_array[i])&parseInt(mask_array[i]);
	}
	host_array[3]++;
	return host_array.join(".");
}

function hostMax(ip, mask) {
	var host_array=[];
	var wildcard_array=[];
	ip_array = ip.split('.');
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		wildcard_array[i]=~parseInt(mask_array[i])&255;
		host_array[i] = parseInt(ip_array[i])&parseInt(mask_array[i])|wildcard_array[i];
	}
	host_array[3]--;
	return host_array.join(".");
}

function checkIPrange(ipMin, ipMax, myip) {
var fromArr = ipMin.split(".");
var toArr = ipMax.split(".");
var myipArr = myip.split(".");
var fromip=0; toip=0; myip=0;
	for(i=0;i<4;i++) {
		fromip=fromip*1000+parseInt(fromArr[i]);
		toip=toip*1000+parseInt(toArr[i]);
		myip=myip*1000+parseInt(myipArr[i]);
	}
	if(myip<fromip || myip>toip) {
		return false;
	}
	return true;
}

function isWithinHostIpRange(ip, mask, myip) {
	return checkIPrange(hostMin(ip, mask), hostMax(ip, mask), myip);
}

function checkIpAddr(field, ismask, msg) {
	if (field.value == "") {
		if (ismask) {
			validate_alert( "", typeof(msg)!="undefined"?msg+_("routing warningMsg5"):_("routing warningMsg5"));//The netmask is empty.
		}
		else {
			validate_alert( "", typeof(msg)!="undefined"?msg+_("warningMsg01"):_("warningMsg01"));//IP address is empty.
		}
		field.value = field.defaultValue;
		field.focus();
		return false;
	}

	if (isAllNum(field.value) == 0) {
		validate_alert( "", typeof(msg)!="undefined"?msg+_("warningMsg02"):_("warningMsg02"));//'It should be a [0-9] number.'
		field.value = field.defaultValue;
		field.focus();
		return false;
	}

	if (ismask) {
		if ((!checkRange(field.value, 1, 0, 256)) ||
			(!checkRange(field.value, 2, 0, 256)) ||
			(!checkRange(field.value, 3, 0, 256)) ||
			(!checkRange(field.value, 4, 0, 256)))
		{
			validate_alert( "", typeof(msg)!="undefined"?msg+_("routing warningMsg2"):_("routing warningMsg2"));//'The netmask has wrong format.'
			field.value = field.defaultValue;
			field.focus();
			return false;
		}
	}
	else {
		if ((!checkRange(field.value, 1, 0, 255)) ||
			(!checkRange(field.value, 2, 0, 255)) ||
			(!checkRange(field.value, 3, 0, 255)) ||
			(!checkRange(field.value, 4, 1, 254)))
		{
			if (ismask) {
				validate_alert( "", typeof(msg)!="undefined"?msg+_("routing warningMsg2"):_("routing warningMsg2"));//'The netmask has wrong format.'
			}
			else {
				validate_alert( "", typeof(msg)!="undefined"?msg+_("warningMsg03"):_("warningMsg03"));//'IP adress format error.'
			}
			field.value = field.defaultValue;
			field.focus();
			return false;
		}
	}
	clear_alert();
	return true;
}

function parse_ip_into_fields(ipaddr, name) {
	var i;
	var ip_array;

	ip_array=ipaddr.split(".");
	if(ip_array.length != 4 ) {ip_array[0]=''; ip_array[1]='';ip_array[2]=''; ip_array[3]='';}
	for(i=0;i<4;i++) {
		$("input[name="+name+(i+1)+"]").val(ip_array[i]||"");
	}
}

function parse_ip_from_fields(documentItem) {
	var ip_array=[];
	var val;
	var j=0;

	for(var i=0;i<4;i++) {
		val=$("input[name="+documentItem+(i+1)+"]").val();
		if(val!="") {
			ip_array[j++]=val;
		}
//svn revision #44954 is applied
#ifndef V_WEBIF_SPEC_vdf
		else {
			return "";
		}
#endif
	}
	return ip_array.join(".");
}

function ValidSubnetMask(n, t){
	NumfieldEntry(t);
	if(isValidSubnetMask(parse_ip_from_fields(n))<0) {
		for(i=0;i<4;i++) {
			$("input[name="+n+(i+1)+"]").removeClass("success-text").addClass("failure-text");
		}
	}
	else {
		for(i=0;i<4;i++) {
			$("input[name="+n+(i+1)+"]").removeClass("failure-text").addClass("success-text");
		}
	}
}

//Preprocessor is applied to fix some web pages broken on Vodafone varients, which is caused by svn revision 60705.
#ifdef V_WEBIF_SPEC_vdf
function WinExpIP_0(field,event) {
	if( field.value=="") {
		if($(field).hasClass("required")) {
			$(field).valid();
		}
	}
	else {
		if(isNValidIP(field.value))
			validate_alert( "", _("ip warningMsg3"));
		else
			clear_alert();
	}
}

function WinExpIP_127(field,event) {
	if( field.value=="") {
		if($(field).hasClass("required")) {
			$(field).valid();
		}
	}
	else {
		if(isNValidIP(field.value) || field.value <1 || field.value >223)
			validate_alert( "", _("dhcp warningMsg1"));
		else
			clear_alert();
	}
}

#ifdef WEBUI_V2B
// Generate the HTML code for an IP Address ( the 4 boxes currently)
// name is the is used to identify the fields produced
//    .i.e "ip_addr" will produce fields "ip_addr1" "ip_addr2" "ip_addr3" "ip_addr4"
// winExp is an array of 4 function names used to validate each field
// required is a bool that adds "required" to the class
// return value is a string of html
function _genHtmlIpBlocks(name, winExp, required) {
	var html = "";
	var classDef = "ip-adress";
	if (required)
		classDef = "required " + classDef;
	[".",".",".",""].forEach(function(dot, i) {
		var id = name + (i+1);
		var inputAttribs = {type:'text', class: classDef, maxLength: 3, name: id, id: id, onkeyup: "NumfieldEntry(this);" + winExp[i] + "(this,event)"};
		html += htmlDiv( {class:'field'}, htmlTag("input", inputAttribs, "")) + htmlTag("label", {for: id, class:'input-connect-dot'}, dot);
	});
	return html;
}

function genHtmlIpBlocks(name) {return _genHtmlIpBlocks(name, ["WinExpIP_1","WinExpIP","WinExpIP","WinExpIP"], true);}

function genHtmlIpBlocks0(name) {return _genHtmlIpBlocks(name, ["WinExpIP_0","WinExpIP_0","WinExpIP_0","WinExpIP_0"], true);}

function genHtmlIpBlocks127(name) {return _genHtmlIpBlocks(name, ["WinExpIP_127","WinExpIP","WinExpIP","WinExpIP"], true);}

function genHtmlMaskBlocks(name) {return _genHtmlIpBlocks(name, ["WinExpIP","WinExpIP","WinExpIP","WinExpIP"], true);}

function genHtmlIpBlocksWithoutRequired(name) {return _genHtmlIpBlocks(name, ["WinExpIP_1","WinExpIP","WinExpIP","WinExpIP"], false);}

function genHtmlIpBlocksWithoutRequired0(name) {return _genHtmlIpBlocks(name, ["WinExpIP_0","WinExpIP","WinExpIP","WinExpIP"], false);}

function genHtmlMaskBlocksWithoutRequired(name) {return _genHtmlIpBlocks(name, ["WinExpIP","WinExpIP","WinExpIP","WinExpIP"], false);}

#else // WEBUI_V2B

function htmlGenIpBlocks(name_in) {
	h="<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"1' id='"+name_in+"1' onkeyup='NumfieldEntry(this);WinExpIP_1(this,event)'></div>\
	<label for='"+name_in+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"2' id='"+name_in+"2' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"3' id='"+name_in+"3' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"4' id='"+name_in+"4' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenIpBlocks0(name_in) {
	h="<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"1' id='"+name_in+"1' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"2' id='"+name_in+"2' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"3' id='"+name_in+"3' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"4' id='"+name_in+"4' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenIpBlocks127(name_in) {
	h="<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"1' id='"+name_in+"1' onkeyup='NumfieldEntry(this);WinExpIP_127(this,event)'></div>\
	<label for='"+name_in+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"2' id='"+name_in+"2' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"3' id='"+name_in+"3' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='"+name_in+"4' id='"+name_in+"4' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenMaskBlocks(name_in) {
	printf("<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='%s1' id='%s1' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label for='"+name_in+"1' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='%s2' id='%s2' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label for='"+name_in+"2' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='%s3' id='%s3' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label for='"+name_in+"3' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='required ip-adress' maxLength='3' name='%s4' id='%s4' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label for='"+name_in+"4' class='input-connect-dot'></label>");
}

function htmlGenIpBlocksWithoutRequired(name_in) {
	h="<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"1' id='"+name_in+"1' onkeyup='NumfieldEntry(this);WinExpIP_1(this,event)'></div>\
	<label for='"+name_in+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"2' id='"+name_in+"2' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"3' id='"+name_in+"3' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"4' id='"+name_in+"4' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>\
	<label for='"+name_in+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenIpBlocksWithoutRequired0(name_in) {
	h="<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"1' id='"+name_in+"1' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"2' id='"+name_in+"2' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"3' id='"+name_in+"3' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='ip-adress' maxLength='3' name='"+name_in+"4' id='"+name_in+"4' onkeyup='NumfieldEntry(this);WinExpIP_0(this,event)'></div>\
	<label for='"+name_in+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenMaskBlocksWithoutRequired(name_in) {
	printf("<div class='field'><input type='text' class='ip-adress' maxLength='3' name='%s1' id='%s1' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='ip-adress' maxLength='3' name='%s2' id='%s2' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='ip-adress' maxLength='3' name='%s3' id='%s3' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='ip-adress' maxLength='3' name='%s4' id='%s4' onkeyup='NumfieldEntry(this);WinExpIP(this,event)'></div>",name_in,name_in);
	printf("<label class='input-connect-dot'></label>");
}

#endif //WEBUI_V2B

#else // not VDF

#ifdef WEBUI_V2B

// Generate the HTML code for an IP Address ( the 4 boxes currently)
// name is the is used to identify the fields produced
//    .i.e "ip_addr" will produce fields "ip_addr1" "ip_addr2" "ip_addr3" "ip_addr4"
// classes is an array of 4 function class strings(including the validation function) used for each field
// onKeyUp is the name of a function
// return value is a string of html
function _genHtmlIpBlocks(name, classes, onKeyUp) {
	var html = "";
	if(!isDefined(onKeyUp)) onKeyUp = "NumfieldEntry(this);";
	[".",".",".",""].forEach(function(dot, i) {
		var id = name + (i+1);
		var inputAttribs = {type:'text', class: classes[i] +" ip-adress", maxLength: 3, name: id, id: id, onkeyup: onKeyUp, onchange: "validate_group('" + name + "')"};
		html += htmlDiv( {class:'field'}, htmlTag("input", inputAttribs, "")) + htmlTag("label", {for: id, class:'input-connect-dot'}, dot);
	});
	return html;
}

function getValidateStr(parm, func) {return "validate[" + parm + ",funcCall[" + func + "]]";}

function getRequiredValidateStr(func) {return getValidateStr("required", func);}

function genHtmlIpBlocks(n) {
	var val1 =  getRequiredValidateStr("validate_ip_1");
	var valx = getRequiredValidateStr("validate_ip");
	return _genHtmlIpBlocks(n, [val1, valx, valx, valx]);
}

function genHtmlIpBlocks0(n) {
	var val = getRequiredValidateStr("validate_ip_0");
	return _genHtmlIpBlocks(n, [val, val, val, val]);
}

function genHtmlIpBlocks127(n) {
	var val1 = getRequiredValidateStr("validate_ip_127");
	var valx = getRequiredValidateStr("validate_ip");
	return _genHtmlIpBlocks(n, [val1, valx, valx, valx]);
}

function genHtmlMaskBlocks(n) {
	var valx = "success-text " + getRequiredValidateStr("validate_netmask");
	return _genHtmlIpBlocks(n, [valx, valx, valx, valx], "ValidSubnetMask('" + n + "',this)");
}

function getcondRequired(n) {
	return "condRequired[" + n + "1," + n + "2," + n + "3," + n + "4]";
}

function genHtmlIpBlocksWithoutRequired(n) {
	var condRequired = getcondRequired(n);
	var val1 = "validate[" + condRequired + ",funcCall[validate_ip_1]]";
	var valx = "validate[" + condRequired + ",funcCall[validate_ip]]";
	return _genHtmlIpBlocks(n, [val1, valx, valx, valx]);
}

function genHtmlIpBlocksWithoutRequired0(n) {
	var valx = "validate[" + getcondRequired(n) + ",funcCall[validate_ip_0]]";
	return _genHtmlIpBlocks(n, [valx, valx, valx, valx]);
}

function genHtmlMaskBlocksWithoutRequired(n) {
	var valx = "validate[" + getcondRequired(n) + ",funcCall[validate_ip]]";
	return _genHtmlIpBlocks(n, [valx, valx, valx, valx]);
}

#else  //WEBUI_V2B

function htmlGenIpBlocks(n) {
	h="<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_1]] ip-adress' maxLength='3' name='"+n+"1' id='"+n+"1' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"2' id='"+n+"2' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"3' id='"+n+"3' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"4' id='"+n+"4' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenIpBlocks0(n) {
	h="<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_0]] ip-adress' maxLength='3' name='"+n+"1' id='"+n+"1' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_0]] ip-adress' maxLength='3' name='"+n+"2' id='"+n+"2' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_0]] ip-adress' maxLength='3' name='"+n+"3' id='"+n+"3' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_0]] ip-adress' maxLength='3' name='"+n+"4' id='"+n+"4' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenIpBlocks127(n) {
	h="<div class='field'><input type='text' class='validate[required,funcCall[validate_ip_127]] ip-adress' maxLength='3' name='"+n+"1' id='"+n+"1' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"1' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"2' id='"+n+"2' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"2' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"3' id='"+n+"3' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"3' class='input-connect-dot'>.</label>\
	<div class='field'><input type='text' class='validate[required,funcCall[validate_ip]] ip-adress' maxLength='3' name='"+n+"4' id='"+n+"4' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\""+n+"\")'></div>\
	<label for='"+n+"4' class='input-connect-dot'></label>";
	printf(h);
}

function htmlGenMaskBlocks(n) {
	printf("<div class='field'><input type='text' class='success-text validate[required,funcCall[validate_netmask]] ip-adress' maxLength='3' name='%s1' id='%s1' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n);
	printf("<label for='"+n+"1' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[required,funcCall[validate_netmask]] ip-adress' maxLength='3' name='%s2' id='%s2' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n);
	printf("<label for='"+n+"2' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[required,funcCall[validate_netmask]] ip-adress' maxLength='3' name='%s3' id='%s3' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n);
	printf("<label for='"+n+"3' class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[required,funcCall[validate_netmask]] ip-adress' maxLength='3' name='%s4' id='%s4' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n);
	printf("<label for='"+n+"4' class='input-connect-dot'></label>");
}

function htmlGenIpBlocksWithoutRequired(n) {
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip_1]] ip-adress' maxLength='3' name='%s1' id='%s1' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s2' id='%s2' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s3' id='%s3' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s4' id='%s4' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'></label>");
}

function htmlGenIpBlocksWithoutRequired0(n) {
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip_0]] ip-adress' maxLength='3' name='%s1' id='%s1' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip_0]] ip-adress' maxLength='3' name='%s2' id='%s2' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip_0]] ip-adress' maxLength='3' name='%s3' id='%s3' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip_0]] ip-adress' maxLength='3' name='%s4' id='%s4' onKeyUp='NumfieldEntry(this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'></label>");
}

function htmlGenMaskBlocksWithoutRequired(n) {
	printf("<div class='field'><input type='text' class='success-text validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s1' id='%s1' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s2' id='%s2' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s3' id='%s3' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'>.</label>");
	printf("<div class='field'><input type='text' class='success-text validate[condRequired[%s1,%s2,%s3,%s4],funcCall[validate_ip]] ip-adress' maxLength='3' name='%s4' id='%s4' onKeyUp='ValidSubnetMask(\"%s\",this)' onchange='validate_group(\"%s\")'></div>",n,n,n,n,n,n,n,n);
	printf("<label class='input-connect-dot'></label>");
}
#endif  //WEBUI_V2B

#endif

function clear_ip_group(group) {
	for(var i=4; i>=1; i--) {
		$("#"+group+i).val("");
	}
}

function validate_group( group ) {
	var err_counter=0;

	for(var i=4; i>=1; i--) {
		if($("#"+group+i).validationEngine("validate")) {
			if($("#"+group+i).val()!="") {
				err_counter++;
				for(var j=i+1; j<=4; j++) {
					$("#"+group+j).validationEngine("hide");
				}
			}
			else if(err_counter++) {
				$("#"+group+i).validationEngine("hide");
			}
		}
	}
	return err_counter;
}

function validate_ip_1(field, rules, i, options) {
	if(isNValidIP(field.val()) || field.val() <1 || field.val() >223) {
		field.value="192";
		return _("ip warningMsg1");
	}
	else if(field.val()==127) {
		field.value="192";
		//field.select();
		return _("dhcp warningMsg13");
	}
	field.val(parseInt(field.val()));
	return true;
}

function validate_ip_127(field, rules, i, options) {
	if(isNValidIP(field.val()) || field.val() <1 || field.val() >223) {
		field.value="192";
		return _("ip warningMsg1");
	}
	field.val(parseInt(field.val()));
	return true;
}

function validate_ip_0(field, rules, i, options) {
	if(isNValidIP(field.val())) {
		return _("ip warningMsg3");
	}
	field.val(parseInt(field.val()));
	return true;
}

function validate_ip(field, rules, i, options) {
	if(isNValidIP(field.val())) {
		return _("ip warningMsg2");
	}
	field.val(parseInt(field.val()));
	return true;
}

function validate_netmask(field, rules, i, options) {
/*	if(!isNaN(field.val())) {
	field.val(parseInt(field.val()));
}*/
	if(isNValidIP(field.val())) {
	//	field.val("255");
		return _("ip warningMsg3");
	}
	field.val(parseInt(field.val()));
	return true;
}

function ip_to_decimal(addr) {
	var deci;
	deci = parseInt(addr[0])*16777216 + parseInt(addr[1])*65536 + parseInt(addr[2])*256 + parseInt(addr[3]);
	return deci;
}

function decimal_to_ip(deci) {
	var addr = [];
	deci_int = deci - (deci % 16777216);
	addr[0] = deci_int / 16777216;
	addr[1] = (deci & 0x00ff0000) >> 16;
	addr[2] = (deci & 0x0000ff00) >> 8;
	addr[3] = deci & 0x000000ff;
	return addr;
}

function is_large(addr1, addr2) {
	var gap;
	gap = ip_to_decimal(addr1) - ip_to_decimal(addr2);
	if (gap >=0 )
		return 1;
	else
		return 0;
}

function ip_gap(addr1, addr2) {
	var gap;
	if (is_large(addr1, addr2))
		gap = ip_to_decimal(addr1) - ip_to_decimal(addr2);
	else
		gap = ip_to_decimal(addr2) - ip_to_decimal(addr1);
	return gap;
}

function subnetID(ip, mask) {
	var host_array=[];
	ip_array = ip.split('.');
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		host_array[i] = parseInt(ip_array[i])&parseInt(mask_array[i]);
	}
	return host_array.join(".");
}

function broadcastIP(ip, mask) {
	var host_array=[];
	var wildcard_array=[];
	ip_array = ip.split('.');
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		wildcard_array[i]=~parseInt(mask_array[i])&255;
		host_array[i] = parseInt(ip_array[i])&parseInt(mask_array[i])|wildcard_array[i];
	}
	return host_array.join(".");
}

function totalSubnet(mask) {
var sum=0;
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		sum=sum*256+(~parseInt(mask_array[i])&255);
	}
	return sum-1;
}

function netInfo() {
	clear_alert();
	$("#lan_addr").val(parse_ip_from_fields("lan_addr"));
	$("#mask").val(parse_ip_from_fields("mask"));
	switch(isValidSubnetMask($("#mask").val())) {
		case -1:
			validate_alert("",_("invalidSubnetMask"));
			return;
		break;
		case -2:
			validate_alert("",_("wlan warningMsg16"));
			return;//The subnet mask has to be contiguous. Please enter a valid mask
		break;
	}
	if(!isValidIpAddress($("#lan_addr").val())) {
		validate_alert("",_("warningMsg05"));
		return;
	}

	var msg="<div class='message_box' style='text-align:left;'>"+_("subnetID")+subnetID($("#lan_addr").val(), $("#mask").val())+" / "+maskBits($("#mask").val())\
	+"<br/>"+_("broadcastAddress")+broadcastIP($("#lan_addr").val(), $("#mask").val())\
	+"<br/>"+_("subnetIpRange")+hostMin($("#lan_addr").val(), $("#mask").val())+"-"+hostMax($("#lan_addr").val(), $("#mask").val())\
	+"<br/>"+_("hostsPerSubnet")+totalSubnet($("#mask").val())\
	+"<br/></div><div style='margin-left:180px'><button class='secondary mini' onClick='$.unblockUI();'>"+_("CSok")+"</button><div/>";
	$.blockUI({message:msg});
	return;
}


/******************End of net_util.js***********************/

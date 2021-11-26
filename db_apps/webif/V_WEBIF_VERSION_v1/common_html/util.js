//----------------------------------------------------------------------------------------------------
// This file includes Windows popup box overriding function and it these overriding function is
// defined twice it will make web browsers hang up. To prevent this problem, include
// overriding function only once.
//----------------------------------------------------------------------------------------------------
//   UTIL.JS FILE INCLUDING CHECK START
//----------------------------------------------------------------------------------------------------
if ( (typeof(util_js_included) == "undefined") || util_js_included == false )
{
	var util_js_included = true;
//----------------------------------------------------------------------------------------------------

function isHexaDigit(digit) {
var hexVals = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "a", "b", "c", "d", "e", "f");
var len = hexVals.length;
var i = 0;
var ret = false;
	for ( i = 0; i < len; i++ )
		if ( digit == hexVals[i] ) break;
	if ( i < len )
		ret = true;
	return ret;
}

function isValidKey(val, size) {
var ret = false;
var len = val.length;
var dbSize = size * 2;

	if ( len == size )
		ret = true;
	else if ( len == dbSize ) {
		for ( i = 0; i < dbSize; i++ )
			if ( isHexaDigit(val.charAt(i)) == false )
				break;
		if ( i == dbSize )
			ret = true;
	} else
		ret = false;
	return ret;
}


function isValidHexKey(val, size) {
var ret = false;
	if (val.length == size) {
		for ( i = 0; i < val.length; i++ ) {
			if ( isHexaDigit(val.charAt(i)) == false ) {
				break;
			}
		}
		if ( i == val.length ) {
			ret = true;
		}
	}
	return ret;
}

function nameFilter(field) {
	for ( i = 0; i < field.value.length; i++ ) {
		if ( isNameUnsafe(field.value.charAt(i)) == true ) {
			field.value=field.value.replace(field.value.charAt(i), '');
		}
	}
}

function isNameUnsafe(compareChar) {
var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.: \t";
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
			&& compareChar.charCodeAt(0) < 123 ) {
		return false; // found no unsafe chars, return false
	}
	else {
		return true;
	}
}   

// Check if a name valid
function isValidName(name) {
var i = 0;
	for ( i = 0; i < name.length; i++ ) {
		if ( isNameUnsafe(name.charAt(i)) == true )
			return false;
	}
	return true;
}

// same as is isNameUnsafe but allow spaces
function isCharUnsafe(compareChar) {
var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.:\t";
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) >= 32
			&& compareChar.charCodeAt(0) < 123 )
		return false; // found no unsafe chars, return false
	else
		return true;
}   

function isValidNameWSpace(name) {
var i = 0;
	for ( i = 0; i < name.length; i++ ) {
		if ( isCharUnsafe(name.charAt(i)) == true )
			return false;
	}
	return true;
}

function isSameSubNet(lan1Ip, lan1Mask, lan2Ip, lan2Mask) {
var count = 0;
	lan1a = lan1Ip.split('.');
	lan1m = lan1Mask.split('.');
	lan2a = lan2Ip.split('.');
	lan2m = lan2Mask.split('.');

	for (i = 0; i < 4; i++) {
		l1a_n = parseInt(lan1a[i]);
		l1m_n = parseInt(lan1m[i]);
		l2a_n = parseInt(lan2a[i]);
		l2m_n = parseInt(lan2m[i]);
		if ((l1a_n & l1m_n) == (l2a_n & l2m_n))
			count++;
	}
	if (count == 4)
		return true;
	else
		return false;
}
function KeyCode(e) {
	if(e&&e.which){ //NN
		e=e;
		return(e.which);
	}
	else{
		e=event;
		return(e.keyCode);
	}
}
function isBlank(s) {
	for (i=0;i<s.length;i++) {
		c=s.charAt(i);
		if ((c!=' ') && (c!='\n') && (c!='\t'))
			return false;
	}
	return true;
}
function isNValidIP(s) {
	if((isBlank(s))||(isNaN(s))||(s<0||s>255)) 
		return true;
	else
		return false;
}
function IPfieldEntry(field) {
	if(isNaN(field.value.charAt(field.value.length-1))&&field.value.charAt(field.value.length-1)!='.')
		field.value=field.value.slice(0,field.value.length-1);
}
function NumfieldEntry(field) {
	if(isNaN(field.value.charAt(field.value.length-1))&&field.value.charAt(field.value.length-1)!='.')
		field.value=field.value.slice(0,field.value.length-1);
}
function lastEntryChar(field,spchar) {
	if(field.value.charAt(field.value.length-1)==spchar) {
		field.value=field.value.slice(0,field.value.length-1);
		if(field.value.length)
			return(1);
	}
	return(0);
}
function isValidIpEntry(field,e) {
	if(KeyCode(e)!=9) {
		IPfieldEntry(field);
		if(lastEntryChar(field,' '))
			field.value=field.value.substring(0,field.value.length);
		if(lastEntryChar(field,'.')||field.value.length==3) {
			if(isNValidIP(field.value)) {
				field.value="255";
				field.select();
				return false;
			}
			else if(field.value.length<3)
				focusOnNext(field);
		}
	}
	return true;
}
function isValidIpEntry_1(field,e) {
	if(KeyCode(e)!=9) {
		IPfieldEntry(field);
		if(lastEntryChar(field,' '))
			field.value=field.value.substring(0,field.value.length);
			if(lastEntryChar(field,'.')||field.value.length==3) {
				if(isNValidIP(field.value) || field.value <1 || field.value >223) {
					field.value="192";
					field.select();
					return -1;
				}
			else if(field.value==127) {
				field.value="192";
				field.select();
				return -2;
			}
			else if(field.value.length<3)
				focusOnNext(field);
		}
	}
	return 1;
}
function isValidIpAddress(address) {
var i = 0;
	if ( address == '0.0.0.0' || address == '255.255.255.255' )
		return false;
	addrParts = address.split('.');
	if ( addrParts.length != 4 ) return false;
	for (i = 0; i < 4; i++) {
		if (isNaN(addrParts[i]) || addrParts[i] =="")
			return false;
		num = parseInt(addrParts[i]);
		if ( num < 0 || num > 255 )
			return false;
	}
	return true;
}

function getLeftMostZeroBitPos(num) {
var i = 0;
var numArr = [128, 64, 32, 16, 8, 4, 2, 1];
	for ( i = 0; i < numArr.length; i++ )
		if ( (num & numArr[i]) == 0 )
			return i;
	return numArr.length;
}

function getRightMostOneBitPos(num) {
var i = 0;
var numArr = [1, 2, 4, 8, 16, 32, 64, 128];
	for ( i = 0; i < numArr.length; i++ )
		if ( ((num & numArr[i]) >> i) == 1 )
			return (numArr.length - i - 1);
	return -1;
}
function printf(fmt) {
var reg = /%s/;
var result = new String(fmt);
	for (var i = 1; i < arguments.length; i++)   
		result = result.replace(reg, new String(arguments[i]));
	document.write(result); 
}
function htmlGenIpBlocks(name_in) {
	var name = new String(name_in);	
	printf("<input maxLength='3' name='%s1' size='3' onkeyup=WinExpIP_1(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s2' size='3' onkeyup=WinExpIP(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s3' size='3' onkeyup=WinExpIP(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s4' size='3' onkeyup=WinExpIP(this,event);>",name);
}

function htmlGenMaskBlocks(name_in) {
	var name = new String(name_in);	
	printf("<input maxLength='3' name='%s1' size='3' onkeyup=WinExpIP(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s2' size='3' onkeyup=WinExpIP(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s3' size='3' onkeyup=WinExpIP(this,event);> <b>.</b> ",name);
	printf("<input maxLength='3' name='%s4' size='3' onkeyup=WinExpIP(this,event);>",name);
}

function isValidSubnetMask(mask) {
//m[0] can be 128, 192, 224, 240, 248, 252, 254, 255
//m[1] can be 128, 192, 224, 240, 248, 252, 254, 255 if m[0] is 255, else m[1] must be 0
//m[2] can be 128, 192, 224, 240, 248, 252, 254, 255 if m[1] is 255, else m[2] must be 0
//m[3] can be 128, 192, 224, 240, 248, 252, 254, 255 if m[2] is 255, else m[3] must be 0
var correct_range = {128:1,192:1,224:1,240:1,248:1,252:1,254:1,255:1,0:1};
var m = mask.split('.');
	for (var i = 0; i <= 3; i ++) {
		if (!(m[i] in correct_range)) {
			return -2;
		}
	}	
	if (m.length!=4 || (m[0] == 0) || (m[0] != 255 && m[1] != 0) || (m[1] != 255 && m[2] != 0) || (m[2] != 255 && m[3] != 0)) {
		return -1;
	}    
	return 1;
}

function isValidPort(port) {
var fromport = 0;
var toport = 100;
	portrange = port.split(':');
	if ( portrange.length < 1 || portrange.length > 2 ) {
		return false;
	}
	if ( isNaN(portrange[0]) )
		return false;
	fromport = parseInt(portrange[0]);
	if ( portrange.length > 1 ) {
		if ( isNaN(portrange[1]) )
			return false;
		toport = parseInt(portrange[1]);
		if ( toport <= fromport )
			return false;
	}
	if ( fromport < 1 || fromport > 65535 || toport < 1 || toport > 65535 )
		return false;
	return true;
}

function isValidNatPort(port) {
var fromport = 0;
var toport = 100;
	portrange = port.split('-');
	if ( portrange.length < 1 || portrange.length > 2 ) {
		return false;
	}
	if ( isNaN(portrange[0]) )
		return false;
	fromport = parseInt(portrange[0]);

	if ( portrange.length > 1 ) {
		if ( isNaN(portrange[1]) )
			return false;
		toport = parseInt(portrange[1]);
		if ( toport <= fromport )
			return false;
	}
	if ( fromport < 1 || fromport > 65535 || toport < 1 || toport > 65535 )
		return false;
	return true;
}

function isValidMacAddress(address) {
var c = '';
var i = 0, j = 0;

	if ( address == 'ff:ff:ff:ff:ff:ff' ) return false;

	addrParts = address.split(':');
	if ( addrParts.length != 6 ) return false;

	for (i = 0; i < 6; i++) {
		if ( addrParts[i] == '' )
			return false;
		for ( j = 0; j < addrParts[i].length; j++ ) {
			c = addrParts[i].toLowerCase().charAt(j);
			if ( (c >= '0' && c <= '9') ||
				(c >= 'a' && c <= 'f') )
				continue;
			else
				return false;
		}
	}
	return true;
}

var hexVals = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
              "A", "B", "C", "D", "E", "F");
var unsafeString = "\"<>%\\^[]`\+\$\,'#&";
// deleted these chars from the include list ";", "/", "?", ":", "@", "=", "&" and #
// so that we could analyze actual URLs

function isUnsafe(compareChar) {
// this function checks to see if a char is URL unsafe.
// Returns bool result. True = unsafe, False = safe
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
			&& compareChar.charCodeAt(0) < 123 )
		return false; // found no unsafe chars, return false
	else
		return true;
}

function decToHex(num, radix) {
// part of the hex-ifying functionality
var hexString = "";
	while ( num >= radix ) {
		temp = num % radix;
		num = Math.floor(num / radix);
		hexString += hexVals[temp];
	}
	hexString += hexVals[num];
	return reversal(hexString);
}

function reversal(s) {
// part of the hex-ifying functionality
var len = s.length;
var trans = "";
	for (i = 0; i < len; i++)
		trans = trans + s.substring(len-i-1, len-i);
	s = trans;
	return s;
}

function convert(val) {
// this converts a given char to url hex form
var hstr = decToHex(val.charCodeAt(0), 16);
	if (hstr.length > 1)
		return  "%" + hstr;
	else if (hstr.length > 0)
		return  "%0" + hstr;
	//return  "%" + decToHex(val.charCodeAt(0), 16);
}


function encodeUrl(val) {
var len     = val.length;
var i       = 0;
var newStr  = "";
var original = val;
	for ( i = 0; i < len; i++ ) {
		if ( val.substring(i,i+1).charCodeAt(0) < 255 ) {
			// hack to eliminate the rest of unicode from this
			if (isUnsafe(val.substring(i,i+1)) == false)
				newStr = newStr + val.substring(i,i+1);
			else
				newStr = newStr + convert(val.substring(i,i+1));
		} else {
			// woopsie! restore.
			alert ("Found a non-ISO-8859-1 character at position: " + (i+1) + ",\nPlease eliminate before continuing.");
			newStr = original;
			// short-circuit the loop and exit
			i = len;
		}
	}
	return newStr;
}

var markStrChars = "\"'";

// Checks to see if a char is used to mark begining and ending of string.
// Returns bool result. True = special, False = not special
function isMarkStrChar(compareChar) {
	if ( markStrChars.indexOf(compareChar) == -1 )
		return false; // found no marked string chars, return false
	else
		return true;
}

// use backslash in front one of the escape codes to process
// marked string characters.
// Returns new process string
function processMarkStrChars(str) {
var i = 0;
var retStr = '';
	for ( i = 0; i < str.length; i++ ) {
		if ( isMarkStrChar(str.charAt(i)) == true )
			retStr += '\\';
		retStr += str.charAt(i);
	}
	return retStr;
}

// Web page manipulation functions

function showhide(element, sh) {
var status;
	if (document.getElementById) {
		if (sh == 1)
			status = "";
		else
			status = "none"
		// standard
		document.getElementById(element).style.display = status;
	}
	else if (document.all) {
		if (sh == 1)
			status = "block";
		else
			status = "none"
		// old IE
		document.all[element].style.display = status;
	}
	else if (document.layers) {
		if (sh == 1)
			status = "block";
		else
			status = "none"
		// Netscape 4
		document.layers[element].display = status;
	}
}

// Load / submit functions
function getSelect(item) {
var idx;
	if (item.options.length > 0) {
	    idx = item.selectedIndex;
	    return item.options[idx].value;
	}
	else {
		return '';
    }
}

function setSelect(item, value) {
	for (i=0; i<item.options.length; i++) {
		if (item.options[i].value == value) {
			item.selectedIndex = i;
			break;
		}
	}
}

function setCheck(item, value) {
	if ( value == '1' )
		item.checked = true;
	else
		item.checked = false;
}

function setDisable(item, value) {
	if ( value == 1 || value == '1' )
		item.disabled = true;
	else
		item.disabled = false;
}

function submitText(item) {
	return '&' + item.name + '=' + item.value;
}

function submitSelect(item) {
	return '&' + item.name + '=' + getSelect(item);
}


function submitCheck(item) {
var val;
	if (item.checked == true) {
		val = 1;
	} 
	else {
		val = 0;
	}
	return '&' + item.name + '=' + val;
}

function HostDate() {
var currentTime = new Date();
var seconds = currentTime.getUTCSeconds();
var minutes = currentTime.getUTCMinutes();
var hours = currentTime.getUTCHours();
var month = currentTime.getUTCMonth() + 1;
var day = currentTime.getUTCDate();
var year = currentTime.getFullYear();
var seconds_str = " ";
var minutes_str = " ";
var hours_str = " ";
var month_str = " ";
var day_str = " ";
var year_str = " ";

	if(seconds < 10)
		seconds_str = "0" + seconds;
	else
		seconds_str = ""+seconds;
	if(minutes < 10)
		minutes_str = "0" + minutes;
	else
		minutes_str = ""+minutes;
	if(hours < 10)
		hours_str = "0" + hours;
	else
		hours_str = ""+hours;
	if(month < 10)
		month_str = "0" + month;
	else
		month_str = ""+month;
	if(day < 10)
		day_str = "0" + day;
	else
		day_str = day;
//	return  month_str+day_str+hours_str+minutes_str+year;
	return  year+"."+month_str+"."+day_str+"-"+hours_str+":"+minutes_str;
}

function f_filterResults(n_win, n_docel, n_body) {
var n_result = n_win ? n_win : 0;
	if (n_docel && (!n_result || (n_result > n_docel)))
		n_result = n_docel;
	return n_body && (!n_result || (n_result > n_body)) ? n_body : n_result;
}

function f_clientWidth() {
	return f_filterResults (
		window.innerWidth ? window.innerWidth : 0,
		document.documentElement ? document.documentElement.clientWidth : 0,
		document.body ? document.body.clientWidth : 0
	);
}

var pre_wwidth = 0;
var left;
var currentLeft;
var top;
var currentTop;
function init_moveGUI() {
	var wwidth = f_clientWidth();
	if(wwidth>1024) {
		left = currentLeft = 558;
		top = currentTop = 110;
		document.getElementById( "banner" ).style['display']='';
	}
	else if(wwidth>666) {
		left = currentLeft = wwidth-460;
		top = currentTop = 110;
		document.getElementById( "banner" ).style['display']='';
	}
	else {
		left = currentLeft = 20;
		top = currentTop = 20;
		document.getElementById( "banner" ).style['display']='none';
	}
	document.getElementById( "basicGUI" ).style['left']=currentLeft+'px';
	document.getElementById( "basicGUI" ).style['top']=currentTop+'px';
}
function moveGUI() {
	var wwidth = f_clientWidth();
	if( Math.abs(wwidth - pre_wwidth)>20 ) {
		pre_wwidth = wwidth;
		if(wwidth>1024) {
			left=558;
			top=110;
			document.getElementById( "banner" ).style['display']='';
		}
		else if(wwidth>666) {
			left=wwidth-460;
			top=110;
			document.getElementById( "banner" ).style['display']='';
		}
		else {
			left=20;
			top=20;
			document.getElementById( "banner" ).style['display']='none';
		}
	}
	diff=Math.abs( currentLeft-left );
	if( diff>=2 ) {
		if( currentLeft<left )
			currentLeft=currentLeft+diff/2;
		else if( currentLeft>left )
			currentLeft=currentLeft-diff/2;
		document.getElementById( "basicGUI" ).style['left']=currentLeft+'px';
	}
	diff=Math.abs( currentTop-top );
	if( diff>=2 ) {
		if( currentTop<top )
			currentTop=currentTop+diff/2;
		else if( currentTop>top )
			currentTop=currentTop-diff/2;
		document.getElementById( "basicGUI" ).style['top']=currentTop+'px';
	}
}

function toAdvV(url) {
	$('#basicGUI').animate({
	opacity: 0.25,
	left: '+=50',
	height: 'toggle'
	}, 1000, function() {
	/*	$('#banner').animate({
			opacity: 0.25,
			left: '+=50',
			height: 'toggle'
		}, 1000, function() { */
			location.href=url;
	//	});
		});
}

function IpCheck(IP1,IP2,IP3,IP4) {
	if (((isBlank(IP1))||(isNaN(IP1))||(IP1<0||IP1>255))||((isBlank(IP2))||(isNaN(IP2))||(IP2<0||IP2>255))||((isBlank(IP3))||(isNaN(IP3))||(IP3<0||IP3>255)) || ((isBlank(IP4))||(isNaN(IP4))||(IP4<0||IP4>255)))
		return false;
	else
		return true;
}

function set_var_tag() {
	$("var").each(function(e) {this.innerHTML=eval(this.innerHTML);});
}

function WinExpIP(field,event) {
var val=field.value;
	if(!isValidIpEntry(field,event))
		window.alert(_("dhcp warningMsg11"));
}
function WinExpIP_1(field,event) {
var val=field.value;
	switch(isValidIpEntry_1(field,event)) {
	case -1:
		alert(_("dhcp warningMsg12"));
	break;
	case -2:
		alert(_("dhcp warningMsg13"));
	break;
	}
}

function isValidNameEntry(field,e) {
	if(KeyCode(e)!=9){
		if(isCharUnsafe(field.value.charAt(field.value.length-1))) {
			field.value=field.value.slice(0,field.value.length-1);
			return false;
		}
	}
	return true;
}

function atoi(str, num) {
i=1;
	if(num != 1 ) {
		while (i != num && str.length != 0){
			if(str.charAt(0) == '.'){
				i++;
			}
			str = str.substring(1);
		}
		if(i != num )
			return -1;
	}
	for(i=0; i<str.length; i++) {
		if(str.charAt(i) == '.') {
			str = str.substring(0, i);
			break;
		}
	}
	if(str.length == 0)
		return -1;
	return parseInt(str, 10);
}

function isAllNum(str) {
	for (var i=0; i<str.length; i++){
		if((str.charAt(i) >= '0' && str.charAt(i) <= '9') || (str.charAt(i) == '.'))
			continue;
		return 0;
	}
	return 1;
}

function isAllNumAndSlash(str) {
	for (var i=0; i<str.length; i++){
		if( (str.charAt(i) >= '0' && str.charAt(i) <= '9') || (str.charAt(i) == '.') || (str.charAt(i) == '/'))
			continue;
		return 0;
	}
	return 1;
}

function isNumOnly(str) {
	for (var i=0; i<str.length; i++){
		if((str.charAt(i) >= '0' && str.charAt(i) <= '9') )
			continue;
		return 0;
	}
	return 1;
}

function checkRange(str, num, min, max) {
	d = atoi(str,num);
	if(d > max || d < min)
		return false;
	return true;
}

function checkIpAddr(field, ismask, msg) {
	if (field.value == "") {
		if (ismask) {
			alert( typeof(msg)!="undefined"?msg+_("routing warningMsg5"):_("routing warningMsg5"));//The netmask is empty.
		}
		else {
			alert( typeof(msg)!="undefined"?msg+_("warningMsg01"):_("warningMsg01"));//IP address is empty.
		}
		field.value = field.defaultValue;
		field.focus();
		return false;
	}

	if (isAllNum(field.value) == 0) {
		alert(typeof(msg)!="undefined"?msg+_("warningMsg02"):_("warningMsg02"));//'It should be a [0-9] number.'
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
			alert(typeof(msg)!="undefined"?msg+_("routing warningMsg2"):_("routing warningMsg2"));//'The netmask has wrong format.'
			field.value = field.defaultValue;
			field.focus();
			return false;
		}
	}
	else {
		if ((!checkRange(field.value, 1, 0, 255)) ||
			(!checkRange(field.value, 2, 0, 255)) ||
			(!checkRange(field.value, 3, 0, 255)) ||
			(!checkRange(field.value, 4, 0, 256)))
		{
			if (ismask) {
				alert(typeof(msg)!="undefined"?msg+_("routing warningMsg2"):_("routing warningMsg2"));//'The netmask has wrong format.'
			}
			else {
				alert(typeof(msg)!="undefined"?msg+_("warningMsg03"):_("warningMsg03"));//'IP adress format error.'
			}
			field.value = field.defaultValue;
			field.focus();
			return false;
		}
	}
	return true;
}

function hasSubStr(str, substr) {
	return str.search(substr) >= 0;
}

function multiLangRadio(txt) {
	if(Butterlate.getLang()=="ar")
		document.write("<font dir=\"rtl\">"+_(txt));
	else
		document.write("<font>"+_(txt));
}

function toUpTime( uptime ) {
var	upday = parseInt(uptime / (24 * 3600));
var uphr = parseInt((uptime - upday * 24 * 3600) / (3600));
var upmin = parseInt((uptime - upday * 24 * 3600 - uphr * 3600) / 60);
var upsec = parseInt(uptime - upday * 24 * 3600 - uphr * 3600 - upmin * 60);
	uphr=uphr<10?"0"+uphr.toString():uphr.toString();
	upmin=upmin<10?"0"+upmin.toString():upmin.toString();
	upsec=upsec<10?"0"+upsec.toString():upsec.toString();
	if (upday) {
		var buf2=upday.toString() + " Day";
		if (upday > 1)
			buf2=buf2+"s";
		buf2=buf2+"  ";
	}
	else {
		buf2="";
	}
	return buf2+uphr+":"+upmin+":"+upsec;// printf("uptime=\"%s%02u : %02u : %02u\";", buf2, uphr, upmin, upsec);
}

function checkIE10() {
var pos=navigator.userAgent.indexOf("MSIE");
	if(pos!=-1 && (parseInt(navigator.userAgent.substr(pos+4, 10))>=9) ) {
		$('.Rotate-90').removeClass('Rotate-90').addClass('Rotate-90-IE10');
	}
}

function set_var_tag() {
	$("var").css("font-style","normal");
	$("var").each(function(e) {
		this.innerHTML=eval($(this).html());
	});
}

/******** rdb tool class ********/
function rdb_tool() {
	/* init. mset opt */
	var opt_idx=1;
	var opt_obj=new Object();

	this.reset=function() {
		opt_idx=1;
		opt_obj=new Object();
	};

	/* add */
	this.add=function(rdb_var,rdb_val) {
		var rdbs=Array();
		/* add rdb_var to rdbs */
		if(rdb_var instanceof Array) {
			$.merge(rdbs,rdb_var)
		}
		else {
			rdbs.push(rdb_var);
		}
		/* add rdb_val to rdbs */
		if(rdb_val instanceof Array) {
			$.merge(rdbs,rdb_val)
		}
		else if(rdb_val!==undefined) {
			rdbs.push(rdb_val);
		}
		/* convert rdbs to opt */
		$.each(rdbs,function(i,v){
			opt_obj["opt"+opt_idx]=v;
			opt_idx++;
		});
	};
	/* submit mget json */
	this.mget=function(func) {
		opt_obj["cmd"]="rdb_mget";
		$.getJSON(
			"./cgi-bin/rdb_tool.cgi",
			opt_obj,
			func
		);
	};
	/* submit mset json */
	this.mset=function(func) {
		opt_obj["cmd"]="rdb_mset";
		$.getJSON(
			"./cgi-bin/rdb_tool.cgi",
			opt_obj,
			func
		);
	};
	
	/* wait for rdb result */
	this.wait_for_rdb_result=function(rdb_to_wait,timeout,func) {
		
		var timer;
		
		var s;
		var n;
		
		/* get start time */
		s=$.now();
		
		var rdb=this;
		
		/* periodic timer function */
		var timer_func=function(){
			
			/* check timeout */
			n=$.now();
			if( n-s >= timeout ) {
				func("");
				return;
			}
			
			/* check rdb */
			rdb.reset();
			rdb.add(rdb_to_wait);
			rdb.mget(function(res){
				if(res[rdb_to_wait]=="") {
					timer=setTimeout(timer_func,500);
				}
				else {
					func(res[rdb_to_wait]);
				}
			});
		};
		
		/* start timer */
		timer=setTimeout(timer_func,500);
	};
	
}

/* standard cgi call */
function cgi(bin) {
	
	var url=bin;
	
	/* init. mset opt */
	var opt_idx=1;
	var opt_obj=new Object();

	this.reset=function() {
		opt_idx=1;
		opt_obj=new Object();
	};
	
	this.dn=function(cmd,func) {
		opt_obj["cmd"]=cmd;
		
		/* build form */
		var form=$("<form/>");
		form.attr("action",url+"?"+$.param(opt_obj));
		form.attr("method","post");
		form.attr("encType","multipart/form-data");
		form.attr("style","display:none");
		
		/* hook up elements */
		form.appendTo("body");
		
		/* submit */
		form.submit();
		
		/* destory */
		form.remove();
	}
	
	/* reset upload */
	this.reset_up=function(el) {
		$(el).closest("form").each(function(){
			this.reset();
		});
	}
	
	/* upload */
	this.up=function(el,complete_func) {
		var upload_input=$(el);
		
		/* create iframe */
		if(!$("#postiframe").length) {
			$("<iframe id='postiframe' name='postiframe' style='width: 0; height: 0; border: none;'></iframe>").appendTo("body");
		}
		
		/* get form */
		var form = $(el).closest("form");
		
		/* hook up func to iframe load */
		$("#postiframe").unbind("load");
		$("#postiframe").load(function(){
			/* get body of iframe */
			var doc=$("#postiframe").contents();
			var res=doc.find("body pre").html();
			
			/* call func */
			complete_func($.parseJSON(res));
		});
		
		/* setup attrs */
		form.attr("action", url+"?"+$.param(opt_obj));
		form.attr("method", "post");
		form.attr("enctype", "multipart/form-data");
		form.attr("encoding", "multipart/form-data");
		form.attr("target", "postiframe");
		/* submit */
		form.submit();
	};
	
	/* add */
	this.add=function(opt) {
		var opts=Array();
		
		/* add */
		if(opt instanceof Array) {
			$.merge(opts,opt)
		}
		else {
			opts.push(opt);
		}
		
		/* convert rdbs to opt */
		$.each(opts,function(i,v){
			opt_obj["opt"+opt_idx]=v;
			opt_idx++;
		});
	};
	
	this.setcmd=function(cmd) {
		opt_obj["cmd"]=cmd;
	};
	
	/* submit mget json */
	this.run=function(cmd,func) {
		opt_obj["cmd"]=cmd;
		$.getJSON(
			url,
			opt_obj,
			func
		);
	};
}

//--------------------------------------------------------------------------------
var windowAlert;
var windowConfirm;
var windowPrompt;

function check_insert_rtl( txt ) {
	if(Butterlate.getLang()!="ar") 
		return txt;
	var ray = new Array();
	var retStr="";
	ray = txt.split("\n");
	for(i=0; i<ray.length; i++)
		retStr = retStr+"\u202b"+ray[i]+"\n";
	return retStr;
}

function overridewindowAlert() {
	windowAlert=window.alert;
	windowConfirm=window.confirm;
	windowPrompt=window.prompt;
	// alert
	window.alert = function(txt) {
		windowAlert( check_insert_rtl( txt ) );
	}
	// confirm
	window.confirm = function(txt) {
		return windowConfirm( check_insert_rtl( txt ) );
	}
	// prompt
	window.prompt = function(txt,def) {
		return windowPrompt(check_insert_rtl( txt ),check_insert_rtl( def ));	
	}
}

overridewindowAlert();

function row_display(id, display) {
	if(document.getElementById){
		var el = document.getElementById(id);
		el.style.display = display ? '' : 'none';
	}
}

function load_value_to_element(el,val) {
	var toggle_element;
	
	if($(el).is("input:checkbox"))
		$(el).prop("checked",val);
	else if($(el).is("input:radio.access")) {
		
		if($.type(val)=="boolean") {
			
			if( $(el).filter("[value=on]").length>0 )
				filter=val?"[value=on]":"[value=off]";
			else if( $(el).filter("[value=1]").length>0 )
				filter=val?"[value=1]":"[value=0]";
			else
				filter=val?":first":":last";
		}
		else if ( ($.type(val)=="undefined") || (val=="") )
			filter=":first";
		else
			filter="[value="+val+"]";
		
		$(el).filter(filter).prop("checked",true);
		$(el).blur();
		
		toggle_element=$(el).parent().prop("data-toggle-element");
		if(toggle_element!==undefined) {
			$("#"+toggle_element).toggle(val);
		}
	}
	else {
		$(el).val(val);
	}
}

function load_values_to_elements(cfg) {
	$.each(cfg,
		function(el,val) {
			load_value_to_element(el,val);
		}
	);
}

//----------------------------------------------------------------------------------------------------
//   UTIL.JS FILE INCLUDING CHECK END
//----------------------------------------------------------------------------------------------------
}

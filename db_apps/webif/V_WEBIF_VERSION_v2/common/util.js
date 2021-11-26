//----------------------------------------------------------------------------------------------------
// This file includes Windows popup box overriding function and it these overriding function is
// defined twice it will make web browsers hang up. To prevent this problem, include
// overriding function only once.
//----------------------------------------------------------------------------------------------------
//   UTIL.JS FILE INCLUDING CHECK START
//----------------------------------------------------------------------------------------------------
//
// Copyright (C) 2018 NetComm Wireless Limited.
if ( (typeof(util_js_included) == "undefined") || util_js_included == false )
{
	var util_js_included = true;
//----------------------------------------------------------------------------------------------------

function isDigit(digit) {
	return (digit >= '0' && digit <= '9');
}

function isHexaDigit(digit) {
	return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F');
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

function filterUsingFn(field, unsafefn ) {
var i = 0;
	for ( i = 0; i < field.value.length; i++ ) {
		if ( unsafefn(field.value.charAt(i)) == true ) {
			field.value=field.value.replace(field.value.charAt(i), '');
			i--; //string has compacted, charAt(i) now has new value, recheck it
		}
	}
}

function nameFilter(field) {
	filterUsingFn( field, isNameUnsafe );
}

function emailFilter(field) {
	filterUsingFn( field, isEmailUnsafe );
}

function nameFilterWSpace(field) {
	filterUsingFn( field, isCharUnsafe );
}

// Allows '0-9','a-z','A-Z','-','.'
function hostNameFilter(field) {
	filterUsingFn( field, isHostNameUnsafe );
}

function isHostNameUnsafe(compareChar) {
	// Numbers are ok
	var charCode = compareChar.charCodeAt(0);
	if ( charCode >=48 && charCode <= 57 )
		return false;
	// Alphabetic characters are ok
	if ( charCode >=65 && charCode <= 90 )
		return false;
	// Alphabetic characters are ok
	if ( charCode >=97 && charCode <= 122 )
		return false;
	// '-' '_' and '.' are ok. '_'=95 '.'=46 '-'=45
	if ( charCode == 95 || charCode == 46 || charCode == 45 )
		return false;
	// Everything else is bad
	return true;
}

// Allowed URL characters from RFC 3986
// * unreserved	= ALPHA / DIGIT / "-" / "." / "_" / "~"
// * reserved	= gen-delims / sub-delims
// * gen-delims	= ":" / "/" / "?" / "#" / "[" / "]" / "@"
// * sub-delims	= "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
function urlFilter(field) {
	filterUsingFn( field, isURLUnsafe );
}

function isURLUnsafe(compareChar) {
	//var patt=new RegExp("[^a-zA-Z0-9\-._~!*'();:@&=+$,\/?#\[\]]");
	var patt=/[^a-zA-Z0-9-._~!*'();:@&=+$,\/?#\[\]]/

	return patt.test(compareChar);
}

function ssidFilter(field) {
	filterUsingFn( field, isSSIDUnsafe );
}

function isSSIDUnsafe(compareChar) {
	var unsafeString = "\"\t\\$";
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) >= 32
		&& compareChar.charCodeAt(0) < 123 )
		return false; // found no unsafe chars, return false
	return true;
}

function isNameUnsafe(compareChar) {
var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.:\t";
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
			&& compareChar.charCodeAt(0) < 123 ) {
		return false; // found no unsafe chars, return false
	}
	return true;
}

// Test if the email contains unsafe character.
// Unsafe characters are outside ASCII range "!" and "z" or are listed in unsafeString.
// Returns true if email is unsafe
function isEmailUnsafe(compareChar) {
var unsafeString = "\"<>%\\^[]`\+\$\,='#&:\t";
	if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
			&& compareChar.charCodeAt(0) < 123 ) {
		return false; // found no unsafe chars, return false
	}
	return true;
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
	return false;
}
function KeyCode(e) {
	if(e&&e.which){ //NN
		e=e;
		return(e.which);
	}
	e=event;
	return(e.keyCode);
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
	return false;
}
function IPfieldEntry(field) {
	if(isNaN(field.value.charAt(field.value.length-1))&&field.value.charAt(field.value.length-1)!='.')
		field.value=field.value.slice(0,field.value.length-1);
	field.value=parseInt(field.value);
}

// Called as text is typed into a editableBoundedInteger() field.
// Dynamically removes any characters the user types that would render the text in invalid (signed)
// integer.  A leading minus sign is fine, but anything after that has to be a decimal digit.
function SignedNumfieldEntry(field) {
		// Remove all but the first "-" and then remove anything not a digit or a "-"
		field.value=field.value.replace(/(?!^)-/g, "").replace(/[^-0-9]+/g, "");
}

function NumfieldEntry(field) {
	field.value=field.value.replace(/[^0-9]+/g, "");
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
//	if(KeyCode(e)!=9) {
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
	//}
	return true;
}
function isValidIpEntry_1(field,e) {
//	if(KeyCode(e)!=9) {
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
	//}
	return 1;
}

function is_valid_domain_name(addr) {
	/*  this domain name validation check is following  */

	var valid=false;

	$.each(addr.split("."),function(i,v){
		/* RFC 1035 (Domain Implementation and Specification) */
		valid=v.match(/^[a-zA-Z]([a-zA-Z0-9\\-]*[a-zA-Z0-9]$|[a-zA-Z0-9]*$)/)!=null;
		return valid;
	});

	return valid;
}

function isDomainNameFormat(addr) {
	/* Though original domain naming was specified in RFC1035 but in 1998
	 * Internationalizing Domain Names in Applications (IDNA) was adopted that was
	 * proposed and implemented by IDN (Internationalized Domain Name) in order to provide
	 * wider and flexible naming. IDNA is to support Arabic, Chinese, Cyrillic, Tamil, Hebrew
	 * or the Latin alphabet-based characters with diacritics, such as French and it has been
	 * implemented in several top-level domains already.
	 * return true : maybe domain name format address, not numeric IP address
	 * return false : numeric IP address so should pass isValidIpAddress() togather
	 */

	/* Normal IP address form will be checked by isValidIpAddress() */
	addrParts = addr.split('.');
	return ( addrParts.length != 4 || isNaN(addrParts[0]) || isNaN(addrParts[1]) ||
			 isNaN(addrParts[2]) || isNaN(addrParts[3]) );
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

function isValidIpAddress0(address) {
var i = 0;
	if ( address == '255.255.255.255' )
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

function isValidIpv6Address(addr) {
// Regexp from: https://community.helpsystems.com/forums/intermapper/miscellaneous-topics/5acc4fcf-fa83-e511-80cf-0050568460e4
var ipv6RegexpPattern=/^\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\s*$/
	return ipv6RegexpPattern.test(addr);
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

function RevIpBlocks(address, blocks){
var addrParts = address.split('.');
	if ( addrParts.length != 4 ) return false;
	for (i = 0; i < 4; i++) {
		if (isNaN(addrParts[i]) || addrParts[i] =="")
			return false;
		num = parseInt(addrParts[i]);
		if ( num < 0 || num > 255 )
			return false;
		eval(blocks+(i+1)+".value="+num);
	}
	return true;
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
	if (m.length!=4 || (m[0] != 255 && m[1] != 0) || (m[1] != 255 && m[2] != 0) || (m[2] != 255 && m[3] != 0)) {
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

	// Trim any trailing whitespace first
	address = address.replace(/\s+$/g,'');

	if ( address == 'ff:ff:ff:ff:ff:ff' ) return false;

	addrParts = address.split(':');
	if ( addrParts.length != 6 ) return false;

	for (i = 0; i < 6; i++) {
		if ( addrParts[i].length!=2 )
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

function maskBits(mask) {
	var bits=0;
	mask_array = mask.split('.');
	for (i = 0; i < 4; i++) {
		bits+=parseInt(mask_array[i]).toString(2).replace(/0/g, "").length;
	}
	return bits;
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
			validate_alert ( "", "Found a non-ISO-8859-1 character at position: " + (i+1) + ",\nPlease eliminate before continuing.");
			newStr = original;
			// short-circuit the loop and exit
			i = len;
		}
	}
	return newStr;
}

// this converts a given char to \x?? hex form
function convertCharToAscii(val) {
    var pad = "00";
    var hstr = val.charCodeAt(0).toString(16);
    // padding zero
    hstr = pad.substring(0, pad.length - hstr.length) + hstr;
    return "\\x" + hstr;
}

// this converts a given char to &#??; html entity form
function convertCharToEntity(val) {
    var pad = "00";
    var hstr = val.charCodeAt(0).toString();
    // padding zero
    hstr = pad.substring(0, pad.length - hstr.length) + hstr;
    return "&#" + hstr + ";";
}

/*
 * escape special characters in the string val.
 * Params:
 *   val: input string
 *   method: encoding method. optional, default='e'
 *             'e': html entity encoding - &#??;
 *             'a': ascii encoding - \x??
 *   esc_chars: characters to be escaped. optional
 */
function specialCharEscape(val, method, esc_chars) {
    var len     = val.length;
    var i       = 0;
    var newStr  = "";

    if (typeof method === 'undefined') {
        method = 'e'; // default method
    }
    if (typeof esc_chars === 'undefined') {
        // default characters to be escaped
        esc_chars = method == 'e' ? "\"'&<>" : "\"'\\";
    }

    for ( i = 0; i < len; i++ ) {
        var nextChar = val.substring(i,i+1);
        if (esc_chars.indexOf(nextChar) == -1) {
            newStr = newStr + nextChar;
        } else {
            newStr = newStr + (method == 'e' ? convertCharToEntity(nextChar) :
                                               convertCharToAscii(nextChar));
        }
    }
    return newStr;
}

/*
 * Converts a two digit hex value to its ascii representation.
 */
function hex2a (hexx)
{
    var hex = hexx.toString();
    var str = '';
    for (var ix = 0; ix < hex.length; ix += 2) {
        str += String.fromCharCode(parseInt(hex.substr(ix, 2), 16));
    }
    return str;
}

/*
 * Decodes all percentage encoded strings within str.
 */
function percent_decode (str)
{
    var percent_index;
    var search_index = 0;
    var result_str = '';
    var hex;

    percent_index = str.indexOf("%", search_index);
    while (percent_index != -1) {
        /* Get the two hex digits that come immediately after the percent */
        hex = str.slice(percent_index + 1, percent_index + 3);

        /* Copy everything before the percent as well as the converted hex */
        result_str += str.slice(search_index, percent_index) + hex2a(hex);

        /* Move past the percent and hex digits */
        search_index = percent_index + 3;
        percent_index = str.indexOf("%", search_index);
    }

    /* Copy rest of string */
    result_str += str.slice(search_index);

    return result_str;
}

var markStrChars = "\"'";

// Checks to see if a char is used to mark begining and ending of string.
// Returns bool result. True = special, False = not special
function isMarkStrChar(compareChar) {
	if ( markStrChars.indexOf(compareChar) == -1 )
		return false; // found no marked string chars, return false
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
	return '';
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
	return true;
}

function set_var_tag() {
	$("var").css("font-style","normal");
	$("var").each(function(e) {
		this.innerHTML=eval($(this).html());
	});
}

function sprintf(fmt) {
	var reg = /%s/;
	var result = new String(fmt);
	for (var i = 1; i < arguments.length; i++) {
		result = result.replace(reg, new String(arguments[i]));
	}
	return result;
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

function breakWord(inString, maxWordLength, htmlEncode) {
	if( inString=="" || inString==null || inString.length==null ) {
		return "";
	}
	var mystr = inString.match(new RegExp('[\\S]{1,}', 'g'));
	var retstr="";

	for(x=0;x<mystr.length;x++) {
		var mystr2 = mystr[x].match(new RegExp('[\\s\\S]{1,'+maxWordLength+'}', 'g'));
		if(mystr2.length==1) {
			retstr+=htmlEncode==1?htmlNumberEncode(mystr[x]):mystr[x]+" ";
		}
		else {
			for(y=0;y<mystr2.length;y++) {
				retstr+=htmlEncode==1?htmlNumberEncode(mystr2[y]):mystr2[y]+"<br/>";
			}
		}
	}
	return retstr;
}

function add_options(myid, mylist, def) {
	$.each( ["#"+myid], function(idx,el) {
		$.each(
			mylist, function(val,txt) { $(el).append("<option value='"+val+"'>"+txt+"</option>");}
		);
	});
	if(def!="") {
		$("#"+myid).val(def);
	}
}

//------------- functions for V2 UI-------------------------------

function set_menu(top, side, user) {

	// This generates all the html for a simple menu with no submenus
	// The first parm MenuLabel. This is a parameter of the outer set_menu call used to identify the page
	// The second parameter is the url
	// 3rd is the displayed text
	function genHtmlForSimpleMenu(label,name,url) {
		var active = ' ';
		if (label==side)
			active=" class='active' ";
		return "<li><a"+active+"href="+url+">"+name+"</a></li>";
	}

	// This generates all the html for a menu and submenus
	// The first parm is the text seen on the page when unexpanded
	// The second parameter is an array of arrays. The latter array has 3 elements
	// 1st is subMenuLabel. This is a parameter of the outer set_menu call used to identify the page
	// 2nd is subMenuName. The text seen when expanded
	// 3rd is subMenu Url
	function generateHtmlForMenu(menuName, subMenus) {
		var open ="";
		var hide = " hide";
		var html="";
		var numMenus = subMenus.length;
		if (numMenus==0)
			return "";
		for ( var i = 0; i < numMenus; i++ )
		{
			var subMenu=subMenus[i];
			if (subMenu.length < 3 ) {
				continue;
			}
			var subMenuLabel=subMenu[0];
			var subMenuName=subMenu[1];
			var subMenuUrl=subMenu[2];

			var active = " ";
			if (subMenuLabel==side) {
				active =" class='active' ";
				open = " class='open'";
				hide = "";
			}
			html += "<a"+active+"href="+subMenuUrl+">"+subMenuName+"</a>";
		}
		html += "</div></li>";
		return "<li"+open+"><a class='expandable'>"+menuName+"</a><div class='submenu"+hide+"'>"+html;
	}

	var top_menu_list = ["Status",
#if !defined V_NETWORKING_UI_none
						 "Internet",
#endif /* !defined V_NETWORKING_UI_none */
#if !defined V_SERVICES_UI_none
						 "Services",
#endif /* !defined V_SERVICES_UI_none */
						 "System",
#ifndef V_HELP_UI_none
						 "Help",
#endif
#ifdef V_WEBIF_SPEC_lark
						 "NIT",
						 "OWA",
#endif
						 ];
	var c=new Array();
	$.each(top_menu_list, function(i,j) {c[j]="";});
	c[top]=" class='active'";
#ifndef V_HELP_UI_none
	c["Help"]=" class='help' ";
#endif
/************************************************************************/
#ifdef V_WEBIF_SPEC_vdf
/************************************************************************/
var h_top="<div class='container'><header class='site-header'>\
	<nav class='top-right grid-9 omega'>\
		<ul class='main-menu list-inline'>\
			<li"+c["Status"]+"><a href='/status.html' style='border-left:none;'>"+_("status")+"</a></li>\
			<li"+c["Internet"]+"><a href='/Profile_Name_List.html'>"+_("CSinternet")+"</a></li>\
			<li"+c["Services"]+"><a href='/ddns.html'>"+_("services")+"</a></li>\
			<li"+c["System"]+"><a href='/administration.html'>"+_("system")+"</a></li>\
			<li"+c["Help"]+"><a href='/help.html' style='border-right:1px solid #ddd;'>"+_("help")+"</a></li>\
		</ul>\
		<div class='top-right-btn'>\
		<div class='power-btn'>";
			if(typeof(user)=="undefined" || user=="&nbsp;" || user=="") {
				h_top+="<a href='/index.html' class='power-btn' id='log-off'><span class='btn-text'>"+_("login")+"</span></a>";
			}
			else {
				h_top+="<span class='logout-foot'></span><a href='/index.html?logoff' class='power-btn' id='log-off'><span class='btn-text'>"+_("logoff")+"</span></a>";
			}
			h_top+="</div>\
			<div class='account-btn'>\
				<span class='login-foot'></span><span class='btn-text'>"+user+"</span>\
			</div>\
		</div>\
	</nav></header>";
	h_top+="<div class='logo'><a href='/status.html'><img src='/img/logo.png' alt='Vodafone Home'></a></div></div>";
	$("#main-menu").append(h_top);

	if(side!="") {
	switch(top) {
	case "Internet":
		var h_side="<ul>";
		var dynamicMenus = [];
		dynamicMenus.push(["Profile_List",_("dataConnection"),'/Profile_Name_List.html']);
		dynamicMenus.push(["BAND",_("band bandTitle"),'/setband.html']);
		if( roam_simcard=="1" ) {
			dynamicMenus.push(["ROAMINGSETTING",_("roamingsettings"),'/roaming_settings.html']);
		}
#if defined(V_SIMMGMT_y) || defined(V_SIMMGMT_v2)
		dynamicMenus.push(["SIMMGMT",_("SimMgmt"),'/sim_management.html']);
#endif
		dynamicMenus.push(["SIM_Security",_("simSecurity"),'/pinsettings.html']);
#ifdef V_DIAL_ON_DEMAND
		dynamicMenus.push(["DOD",_("dialonDemand"),'/dod.html']);
#endif
		h_side += generateHtmlForMenu(_("wireless WAN"), dynamicMenus );
		h_side += generateHtmlForMenu(_("lan"), [
				["LAN", _("lan"), '/LAN.html'],
				["DHCP",_("DHCP"),'/DHCP.html']
				]);

#ifdef V_MODCOMMS_y
	if (rf_mice_ready == "ready") {
#endif
		//
		// Construct sub-menu: "Wireless settings"
		//
		dynamicMenus = [];
#ifdef V_WIFI_ralink
		// The menu for Ralink will be deprecated, but is left for now
#if defined (V_WIFI) || defined (V_WIFI_CLIENT)
		if (wlan_wifi_mode == 'AP') {
#if defined (V_WIFI) && defined (V_WIFI_CLIENT)
			dynamicMenus.push(["Mode_switch",_("wlmode"),'/wlanswitch.html']);
#endif
			dynamicMenus.push(["Basic",_("wireless basic"),'/wlan.html']);
			dynamicMenus.push(["Advanced",_("advanced"),'/advanced.html']);
			dynamicMenus.push(["Mac_filtering",_("mac blocking"),'/wifimacblock.html']);
			dynamicMenus.push(["Station_info",_("stationInfo"),'/wlstationlist.html']);
#ifdef V_WIFI_HOTSPOT_y
			dynamicMenus.push(["Wireless Hotspot",_("hotspot"),'/wifihotspot.html']);
#endif
		} else {
#if defined (V_WIFI) && defined (V_WIFI_CLIENT)
			dynamicMenus.push( ["Mode_switch",_("wlmode"),'/wlanswitch.html']);
#endif
			dynamicMenus.push( ["Client_conf",_("clientConfiguration"),'/wlan_sta.html']);
		}
#endif
#else // V_WIFI_ralink
    // The Linux WiFi driver allows simultaneous AP and STA/Client operation.
#if defined (V_WIFI)
		dynamicMenus.push(["Basic",_("apBasicSettings"),'/wlan.html']);
		dynamicMenus.push(["Advanced",_("apAdvancedSettings"),'/advanced.html']);
		dynamicMenus.push(["Mac_filtering",_("mac blocking2"),'/wifimacblock.html']);
		dynamicMenus.push(["Station_info",_("stationInfo2"),'/wlstationlist.html']);
#ifdef V_WIFI_HOTSPOT_y
		dynamicMenus.push(["Wireless Hotspot",_("hotspot2"),'/wifihotspot.html']);
#endif
#if defined (V_WIFI_CLIENT)
			dynamicMenus.push(["Client_conf",_("wireless client"),'/wlan_sta_linux.html']);
#endif
#endif // V_WIFI
#endif // V_WIFI_ralink

#ifdef V_MODCOMMS_y
	}
#endif

		h_side += generateHtmlForMenu(_("wirelessLAN"), dynamicMenus );
#ifdef V_MULTIPLE_LANWAN_UI
		h_side += generateHtmlForMenu(_("ethernet lanwan"), [
				["LANWAN_Switch", _("ethernetGroupMenu"), '/lanwan_switch.html'],
				["ETHWAN",_("ethernetWANMenu"),'/ethwan.html']
				#if V_IPBASED_VLAN_y
				, ["VLAN_IPBASED",_("ipsubnetBasedVlan"),'/vlan_ipbased.html']
				#endif
				]);
#endif
#ifdef V_ROUTER_TERMINATED_PPPOE
		h_side+=genHtmlForSimpleMenu("PPPOE",_("pppoe"),'/pppoe.html');
#endif

#ifdef V_MULTIPLE_LANWAN_UI
		h_side+=genHtmlForSimpleMenu("WAN",_("WAN failover"),'/wan_summary.html');
#endif

		dynamicMenus = [];
		dynamicMenus.push(["STATIC_ROUTING", _("static"), '/routing.html']);
#ifdef V_RIP
		dynamicMenus.push(["RIP", _("RIP"), '/RIP.html']);
#endif
#ifdef V_VRRP
		dynamicMenus.push(["VRRP", _("VRRP"), '/VRRP.html']);
#endif
#ifndef V_WAN_INTERFACE_none
		dynamicMenus.push(["NAT", _("NAT"), '/NAT.html']);
		dynamicMenus.push(["DMZ", _("treeapp dmz"), '/DMZ.html']);
		if(user=="root") {
			dynamicMenus.push(["FIREWALL", _("router firewall"), '/firewall.html']);
		}
#endif
		dynamicMenus.push(["PORT_FILTER", _("port basic filter"), '/port_filtering.html']);
		h_side += generateHtmlForMenu(_("routing"), dynamicMenus );

#ifndef V_VPN_none
		h_side += generateHtmlForMenu(_("VPN"), [
				["IP_Sec", _("IPsec"), '/VPN_ipsec.html'],
				["OpenVPN",_("OpenVPN"),'/VPN_openvpn.html'],
				[ "pptpClient", _("pptpClient"),'/VPN_pptpc.html'],
				[ "GRE",_("GRE"),'/VPN_gre.html']
#ifndef V_SCEP_CLIENT_none
				,[ "scepClient",_("scepClient"),'/VPN_scep.html']
#endif
				]);
#endif
        h_side+="</ul>";
	break;
	case "Services":
		var h_side="<ul>";
#if (!defined V_WAN_INTERFACE_none) && (!defined V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y) && (!defined V_DDNS_WEBUI_none)
		h_side += genHtmlForSimpleMenu("DDNS",_("man ddns"),'/ddns.html');
#endif
		h_side += genHtmlForSimpleMenu("NTP",_("NTP"),'/NTP.html');
#ifdef V_DATA_STREAM_SWITCH_y
		h_side += generateHtmlForMenu(_("dataStreamSwitch"), [
				["EDP",_("endPoints"),'/end_points.html'],
				["DSS",_("streams"),'/data_stream.html']
			]);
#endif
#if !defined(V_SERIAL_none) && !defined(V_IOBOARD_clarke)
#ifdef V_MODCOMMS_y
		// display/hide GPS menu depending on GPS mice status
		if (io_mice_ready == "ready" || aeris_mice_ready == "ready") {
			h_side += generateHtmlForMenu(_("legacyDataManagers"), [
					["EMU",_("modemEmulator"),'/v250.html'],
					["PADD",_("PADD"),'/padd.html']
				]);
		} else {
			h_side += generateHtmlForMenu(_("legacyDataManagers"), [
					["PADD",_("PADD"),'/padd.html']
				]);
		}
#else
		h_side += generateHtmlForMenu(_("legacyDataManagers"), [
				["EMU",_("modemEmulator"),'/v250.html'],
				["PADD",_("PADD"),'/padd.html']
			]);
#endif
#endif
#ifdef	V_NETSNMP
		h_side += genHtmlForSimpleMenu( "SNMP",_("SNMP"),'/snmp.html');
#endif
#if !defined(V_WAN_INTERFACE_none) && !defined(V_TR069_none)
		h_side += genHtmlForSimpleMenu("TR",_("tr069"),'/TR069.html');
#endif
#ifndef V_OMA_DM_LWM2M_none
		h_side += genHtmlForSimpleMenu("LWM2M",_("LWM2M"),'/LWM2M.html');
#endif
#ifdef V_GPS
#ifdef V_MODCOMMS_y
		// display/hide GPS menu depending on GPS mice status
		if (gps_mice_ready == "ready" || gps_can_mice_ready == "ready") {
#endif
		h_side += generateHtmlForMenu(_("gps"), [
				["GPS",_("gps configuration"),'/gps.html']
#ifdef V_HAS_AGPS_msb
				,["MSB",_("agps"),'/msb.html']
#endif
#ifdef V_ODOMETER_y
				,["ODOMETER",_("odometer"),'odometer.html']
#endif
#ifdef V_GPS_GEOFENCE_y
				,["GEOFENCE",_("geofence"),'/gps_geofence.html']
#endif
			]);
#ifdef V_MODCOMMS_y
		}
#endif
#endif	/* GPS */
#ifdef V_USSD
		h_side += genHtmlForSimpleMenu("USSD",_("ussd"),'ussd.html');
#endif
#if defined(V_IOMGR_kudu) || defined(V_IOMGR_clarke)
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), '/IO_configuration.html');
#elif defined(V_MODCOMMS_y)
	if ( typeof IO_configurationPage != 'undefined'
		&& (io_mice_ready == 'ready' || aeris_mice_ready == 'ready' || chubb_mice_ready == 'ready'))
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), IO_configurationPage );
#else
	if ( typeof IO_configurationPage != 'undefined' )
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), IO_configurationPage );
#endif
#ifdef V_POWERSAVE_y
		h_side += genHtmlForSimpleMenu("POWERSAVE",_("lowPowerMode"),'/low_power_standby.html');
#endif
#ifdef V_EVENT_NOTIFICATION
		h_side += generateHtmlForMenu(_("event noti"), [
				["EVENT_NOTI",_("eventNotificationConf"),'event_noti.html'],
				["EVENT_DEST",_("eventDestinationConf"),'event_dest.html']
			]);
#endif
#ifdef V_EMAIL_CLIENT
		h_side += genHtmlForSimpleMenu("EMAIL_CLIENT",_("email settings"),'email_client.html');
#endif
#ifdef V_SMS
		h_side += generateHtmlForMenu(_("sms title"), [
				["SMS_Setup",_("setup"),'/SMS_Setup.html'],
				["SMS_NewMag",_("newmsg"),'/SMS_New_Message.html'],
				["SMS_Inbox",_("inbox"),'/SMS_Inbox.html'],
				["SMS_Outbox",_("sentItems"),'/SMS_Outbox.html'],
				["SMS_Diag",_("diag"),'SMS_Diagnostics.html']
			]);
#endif
#ifdef V_SPEED_TEST_y
		h_side += genHtmlForSimpleMenu("NETWORK_QUALITY",_("networkQuality"),'network_quality.html');
#endif
		h_side += "</ul>";
	break;
	case "System":
		var h_side="<ul>";
		h_side+= generateHtmlForMenu(_("log"), [
				["LOG",_("systemLog"),'/logfile.html']
#ifdef V_DIAGNOSTIC_LOG
				,["DIAGNOSTICLOG",_("DiagnosticLog"),'/diagnosticlog.html']
#endif
#ifndef V_IPSEC_none
				,["IPSECLOG",_("IPsecLog"),'/ipseclog.html']
#endif
#ifdef V_EVENT_NOTIFICATION
				,["EVENTNOTILOG",_("event noti log"),'/eventnotilog.html']
#endif
				,["LOGSETTINGS",_("systemLogSettings"),'/logsettings.html']
			]);

#if !defined V_SYSTEM_CONFIG_UI_none
		if(user=="root") {
			h_side+= genHtmlForSimpleMenu("Sys_Monitor",_("treeapp sysMonitor"),'/ltph.html');
			h_side+= generateHtmlForMenu(_("systemConfiguration"), [
					["SETTINGS",_("settingsBackupRestore"),'/SaveLoadSettings.html']
					,["UPLOAD",_("upload"),'/AppUpload.html']
					,["PKG_MANAGER",_("pkg manager"),'pkManager.html']
#ifdef V_IPK_FW_SIGNING
					,["FW_SIGNATURE",_("firmwareSignature"),'/fw_signature.html']
#endif
				]);
		}
#endif  /* !defined V_SYSTEM_CONFIG_UI_none */
		h_side+= generateHtmlForMenu(_("administration"), [
				["ADMINISTRATION",_("administrationSettings"),'/administration.html']
#ifndef V_WAN_INTERFACE_none
				,["CERTIFICATE",_("serverCertificate"),'server_certificate.html']
#endif
				,["SSH",_("sshManagement"),'SSH.html']
#ifndef V_LEDPW_SAVE_none
				,["LED",_("led operation mode"),'led_mode.html']
#endif
#ifndef V_HW_PUSH_BUTTON_SETTINGS_none
				,["BUTTON",
#ifdef V_WEBIF_SPEC_vdf
					_("hardwareResetSettings")
#else
					_("buttonSettings")
#endif
					,'button_settings.html']
#endif
			]);
		if(user=="root") {
			h_side+="<li id='customMenu' style='display:none'>\
					<a class='expandable'>"+_("customMenu")+"</a>\
					<div class='submenu' id='subCustomMenu'></div>\
				</li>";
#ifdef V_USB_OTG_MANUAL_MODE_SELECTION
			h_side+=genHtmlForSimpleMenu("USBOTG",_("otgusb"),'/usbotg.html');
#endif

			// display/hide SD card menu depending on installed package
			if (nas_installed == "1") {
				h_side+=genHtmlForSimpleMenu("NAS",_("nas"),'/nas.html');
			}
		}
		h_side+= genHtmlForSimpleMenu("RESET",_("setman reboot"),'/Reboot.html')
			+ "</ul>";
	break;
	}
	$("#side-menu").append(h_side);
	}

	set_var_tag();

	hidePPPoE()

	$.get("/cgi-bin/usermenu.cgi", function(v) {
		if (v!="") {
			$("#customMenu").css("display", "");
			$("#subCustomMenu").html(v);
		}
	});

	$("input[type=text]").keyup(function(e) {
		var code = e.keyCode || e.which;
		if (code == '9') {
			$(this).select();
		}
	});
	$(this).attr("title", _("HTML title"));
}
/************************************************************************/
#else // V_WEBIF_SPEC_vdf - NTC skin:
/************************************************************************/
var h_top="<div class='container'><header class='site-header'>\
	<a href='http://www.netcommwireless.com' target='_blank'><h1 class='grid-4 alpha'>M2M</h1></a>\
	<nav class='top-right grid-9 omega'>\
		<ul class='main-menu list-inline'>"

#if !defined V_WEBIF_SPEC_lark
		h_top+="<li"+c["Status"]+"><a href='/status.html'>"+_("status")+"</a></li>";
#else
		h_top+="<li"+c["NIT"]+"><a href='/mlsystem.html'>"+_("NIT")+"</a></li>";
#endif

#if !defined V_NETWORKING_UI_none
#if defined V_MODULE_none
		h_top+="<li"+c["Internet"]+"><a href='/LAN.html'>"+_("CSinternet")+"</a></li>";
#elif defined V_CELL_NW_cdma
		h_top+="<li"+c["Internet"]+"><a href='/Profile_Settings.html'>"+_("CSinternet")+"</a></li>";
#else
		h_top+="<li"+c["Internet"]+"><a href='/Profile_Name_List.html'>"+_("CSinternet")+"</a></li>";
#endif
#endif /* !defined V_NETWORKING_UI_none */
#ifndef V_SERVICES_UI_none
#ifndef V_NTP_WEBUI_none
		h_top+="<li"+c["Services"]+"><a href='/NTP.html'>"+_("services")+"</a></li>";
#elif !defined V_REMOTE_MGMT_WEBUI_none && defined V_NETSNMP
		h_top+="<li"+c["Services"]+"><a href='/snmp.html'>"+_("services")+"</a></li>";
#elif !defined V_REMOTE_MGMT_WEBUI_none && !defined V_WAN_INTERFACE_none && !defined V_TR069_none
		h_top+="<li"+c["Services"]+"><a href='/TR069.html'>"+_("services")+"</a></li>";
#elif defined V_GPS && !defined V_GPS_WEBUI_none
		h_top+="<li"+c["Services"]+"><a href='/gps.html'>"+_("services")+"</a></li>";
#elif defined V_CBRS_SAS_y && !defined V_CBRS_SAS_WEBUI_none
		h_top+="<li"+c["Services"]+"><a href='/CBRS_parameters.html'>"+_("services")+"</a></li>";
#endif
#endif /* !defined V_SERVICES_UI_none */

#if !defined V_WEBIF_SPEC_lark
		h_top+="<li"+c["System"]+"><a href='"
#ifdef V_ADMINISTRATION_UI_none
		h_top+="/logfile.html'>"
#else
		h_top+="/administration.html'>"
#endif
		h_top+=_("system")+"</a></li>"
#else
		h_top+="<li"+c["OWA"]+"><a href='/mmfirmware.html'>"+_("OWA")+"</a></li>";
#endif

#ifndef V_HELP_UI_none
		h_top+="<li"+c["Help"]+"><a href='/help.html'>"+_("help")+"</a></li>"
#endif
		h_top+="</ul>\
	</nav></header>\
	<div class='header_bar'>";

	if(typeof(multi_lang)!="undefined" && multi_lang>0) {
		h_top+="<div class='left-item'>";
		if(typeof(current_lang)!="undefined" && current_lang=="en") {
			h_top+="<a href='javascript:setLanguage(\"en\")'>English</a>";
		}
		else {
			h_top+="<a href='javascript:setLanguage(\"en\")' style='color:#aaa'>English</a>";
		}
#ifdef V_LANGUAGE_FR_y
		if(typeof(lang_fr)!="undefined" && parseInt(lang_fr)) {
			if(typeof(current_lang)!="undefined" && current_lang=="fr") {
				h_top+="<a href='javascript:setLanguage(\"fr\")' style='padding-left:20px'>français</a>";
			}
			else {
				h_top+="<a href='javascript:setLanguage(\"fr\")' style='padding-left:20px;color:#aaa'>français</a>";
			}
		}
#endif
#ifdef V_LANGUAGE_AR_y
		if(typeof(lang_ar)!="undefined" && parseInt(lang_ar)) {
			if(typeof(current_lang)!="undefined" && current_lang=="ar") {
				h_top+="<a href='javascript:setLanguage(\"ar\")' style='padding-left:20px'>العربية</a>";
			}
			else {
				h_top+="<a href='javascript:setLanguage(\"ar\")' style='padding-left:20px;color:#aaa'>العربية</a>";
			}
		}
#endif
#ifdef V_LANGUAGE_DE_y
		if(typeof(lang_de)!="undefined" && parseInt(lang_de)) {
			if(typeof(current_lang)!="undefined" && current_lang=="de") {
				h_top+="<a href='javascript:setLanguage(\"de\")' style='padding-left:20px'>Deutsch</a>";
			}
			else {
				h_top+="<a href='javascript:setLanguage(\"de\")' style='padding-left:20px;color:#aaa'>Deutsch</a>";
			}
		}
#endif
#ifdef V_LANGUAGE_JP_y
		if(typeof(lang_jp)!="undefined" && parseInt(lang_jp)) {
			if(typeof(current_lang)!="undefined" && current_lang=="jp") {
				h_top+="<a href='javascript:setLanguage(\"jp\")' style='padding-left:20px'>日本語</a>";
			}
			else {
				h_top+="<a href='javascript:setLanguage(\"jp\")' style='padding-left:20px;color:#aaa'>日本語</a>";
			}
		}
#endif
		h_top+="</div>"
	}

	if ("/index.html" == window.location.pathname) {
		h_top+="<div class='right-item account-btn'>\
				<span class='login-foot'></span><span style='color:#fff;margin-left:65px;position:relative;top:-20px'>"+user+"</span>\
			</div>\
		</div>\
		</div>";
	}
	else {
		h_top+="<div class='right-item account-btn'>\
				<span class='login-foot'></span><span style='color:#fff;margin-left:65px;position:relative;top:-20px'>"+user+"</span>\
				<span id='logOff'><a class='log-off' href='/index.html?logoff'></a></span>\
			</div>\
		</div>\
		</div>";
	}

	$("#main-menu").append(h_top);

	var h_side="<ul>";
	if(side!="") {
	switch(top) {

#ifdef V_WEBIF_SPEC_lark
	case "NIT":
		h_side += genHtmlForSimpleMenu("System", _("system"), '/mlsystem.html');
		h_side += genHtmlForSimpleMenu("Status", _("status"), '/mlstatus.html');
		h_side += genHtmlForSimpleMenu("Upgrade", _("upgrade"), '/mlupgrade.html');
		h_side += genHtmlForSimpleMenu("DecryptKey", _("decryptKey"), '/mldecrypt.html');
		h_side += genHtmlForSimpleMenu("Certificate", _("certificate"), '/mlcertificate.html');
		h_side += genHtmlForSimpleMenu("Auto powerdown", _("autoPowerdown"), '/autopowerdown.html');
		h_side +=  "</ul>";
	break;

	case "OWA":
		h_side += genHtmlForSimpleMenu("Firmware", _("Firmware"), '/mmfirmware.html');
		h_side += genHtmlForSimpleMenu("Configuration", _("configuration"), '/mmrtconf.html');
		h_side += genHtmlForSimpleMenu("Upload firmware", _("uploadFirmware"), '/mmupload.html');
		h_side += genHtmlForSimpleMenu("Add configuration", _("addConfiguration"), '/mmrtcadd.html');
		h_side +=  "</ul>";
	break;
#endif

#if !defined V_NETWORKING_UI_none
	case "Internet":
#ifdef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
		h_side += genHtmlForSimpleMenu(_("wireless WAN"), _("dataConnection"), '/Profile_Name_List.html');
		h_side += genHtmlForSimpleMenu(_("VLAN_List"), _("vlans"), '/VLAN_List.html');
		h_side += genHtmlForSimpleMenu(_("routing"), _("static"), '/routing.html');
#else
#ifndef V_WIRELESS_WAN_WEBUI_none
		h_side += generateHtmlForMenu(_("wireless WAN"), [
#ifndef V_CELL_NW_cdma
				["Profile_List",_("dataConnection"),'/Profile_Name_List.html'],
#ifdef V_DIAL_ON_DEMAND
				["DOD",_("dialonDemand"),'/dod.html'],
#endif
#if defined (V_CUSTOM_FEATURE_PACK_myna) || defined(V_CUSTOM_FEATURE_PACK_myna_lite)
				["AttachType",_("Network Attach Type"),'/networkattachtype.html'],
				["IPHandover",_("IP Handover"),'/iphandover.html'],
#endif
				["BAND",_("band operatorSettings"),'/setband.html'],
#if defined(V_SIMMGMT_y) || defined(V_SIMMGMT_v2)
				["SIMMGMT",_("SimMgmt"),'/sim_management.html'],
#endif
				["SIM_Security",_("simSecurity"),'/pinsettings.html']
#else // V_CELL_NW_cdma
				["Profile_Settings",_("dataConnection"),'/Profile_Settings.html'],
#ifdef V_DIAL_ON_DEMAND
				["DOD",_("dialonDemand"),'/dod.html'],
#endif
#if defined(V_SIMMGMT_y) || defined(V_SIMMGMT_v2)
				["SIMMGMT",_("SimMgmt"),'/sim_management.html'],
#endif
				["BAND",_("band operatorSettings"),'/setband.html']
#endif // V_CELL_NW_cdma
				]);
#else
		h_side += generateHtmlForMenu(_("wireless WAN"), [
			["PROFILE_NAME_LIST", _("dataConnection"), '/Profile_Name_List.html']
		]);
#endif // V_WIRELESS_WAN_WEBUI_none

#ifndef V_LAN_WEBUI_none
	h_side += generateHtmlForMenu(_("lan"), [
		["LAN", _("lan"), '/LAN.html'],
		["DHCP",_("DHCP"),'/DHCP.html']
		]);
#endif

#ifdef V_MODCOMMS_y
	if (rf_mice_ready == "ready") {
#endif

		//
		// Construct sub-menu: "Wireless settings"
		//
#ifdef V_WIFI_ralink
		// The menu for Ralink will be deprecated, but is left for now
#if defined (V_WIFI) || defined (V_WIFI_CLIENT)
		if (wlan_wifi_mode == 'AP') {
			h_side += generateHtmlForMenu(_("wirelessLAN"), [
#if defined (V_WIFI) && defined (V_WIFI_CLIENT)
					["Mode_switch", _("wlmode"), '/wlanswitch.html'],
#endif
					["Basic", _("wireless basic"), '/wlan.html']
					,["Advanced", _("advanced"), '/advanced.html']
					,["Mac_filtering",_("mac blocking"),'/wifimacblock.html']
					,["Station_info",_("stationInfo"),'/wlstationlist.html']
#ifdef V_WIFI_HOTSPOT_y
					,["Wireless Hotspot",_("hotspot"),'/wifihotspot.html']
#endif
					]);
		} else {
			h_side += generateHtmlForMenu(_("wirelessLAN"), [
#if defined (V_WIFI) && defined (V_WIFI_CLIENT)
					["Mode_switch", _("wlmode"), '/wlanswitch.html'],
#endif
					["Client_conf", _("clientConfiguration"), '/wlan_sta.html']
					]);
		}
#endif
#else // V_WIFI_ralink
    // The Linux WiFi driver allows simultaneous AP and STA/Client operation.
#if defined (V_WIFI)
		h_side += generateHtmlForMenu(_("wirelessLAN"), [
				["Basic", _("ap")+" "+_("wirelessBasic"), '/wlan.html']
				,["Advanced", _("ap")+" "+_("advanced2"), '/advanced.html']
				,["Mac_filtering", _("ap")+" "+_("mac blocking"), '/wifimacblock.html']
				,["Station_info", _("stationInfo2"), '/wlstationlist.html']
#ifdef V_WIFI_HOTSPOT_y
				,["Wireless Hotspot", _("hotspot2"), '/wifihotspot.html']
#endif
#if defined (V_WIFI_CLIENT)
				,["Client_conf", _("clientConfiguration"), '/wlan_sta_linux.html']
#endif
				]);
#endif // V_WIFI
#endif // V_WIFI_ralink

#ifdef V_MODCOMMS_y
	}
#endif

#ifdef V_MULTIPLE_LANWAN_UI
        // We have decided for now to disable Ethernet WAN functionality, due to
        // issues with USB port on Atmel platforms. But we still need failover from WiFi to 3G
        // so V_ETHWAN_INTERFACE V-variable must be defined
// enable this feature for Sep 2014 release to see what happens, might need to disable it again
// plus on nwl12 models
//#if !defined (V_PRODUCT_ntc_30wv) && !defined (V_PRODUCT_ntc_40wv)
#if !defined(V_PRODUCT_hth_70) && !defined(V_PRODUCT_ntc_20)
		h_side += generateHtmlForMenu(_("ethernet lanwan"), [
				["LANWAN_Switch", _("ethernetGroupMenu"), '/lanwan_switch.html'],
				["ETHWAN",_("ethernetWANMenu"),'/ethwan.html']
				#if V_IPBASED_VLAN_y
				,["VLAN_IPBASED",_("ipsubnetBasedVlan"),'/vlan_ipbased.html']
				#endif
				]);
#endif
//#endif
#endif

#ifdef V_ROUTER_TERMINATED_PPPOE
		h_side+=genHtmlForSimpleMenu("PPPOE",_("pppoe"),'/pppoe.html');
#endif

#ifdef V_MULTIPLE_LANWAN_UI
		h_side+=genHtmlForSimpleMenu("WAN",_("WAN failover"),'/wan_summary.html');
#endif

#ifndef V_ROUTING_UI_none
		dynamicMenus = [];
		dynamicMenus.push(["STATIC_ROUTING", _("static"), '/routing.html']);
#ifdef V_RIP
		dynamicMenus.push(["RIP", _("RIP"), '/RIP.html']);
#endif
#ifdef V_VRRP
		dynamicMenus.push(["VRRP", _("VRRP"), '/VRRP.html']);
#endif
#ifndef V_WAN_INTERFACE_none
		dynamicMenus.push(["NAT", _("treeapp port forwarding"), '/NAT.html']);
		dynamicMenus.push(["DMZ", _("treeapp dmz"), '/DMZ.html']);
		if(user == "root") {
			dynamicMenus.push(["FIREWALL", _("router firewall"), '/firewall.html']);
		}
#endif
		dynamicMenus.push(["PORT_FILTER", _("port basic filter"), '/port_filtering.html']);
		h_side += generateHtmlForMenu(_("routing"), dynamicMenus );
#endif  /* V_ROUTING_UI_none */

#ifndef V_VPN_none
#ifndef V_PRODUCT_hth_70
		h_side += generateHtmlForMenu(_("VPN"), [
				["IP_Sec", _("IPsec"), '/VPN_ipsec.html'],
				["OpenVPN",_("OpenVPN"),'/VPN_openvpn.html'],
				[ "pptpClient", _("pptpClient"),'/VPN_pptpc.html'],
				[ "GRE",_("GRE"),'/VPN_gre.html']
#ifndef V_SCEP_CLIENT_none
				,[ "scepClient",_("scepClient"),'/VPN_scep.html']
#endif
				]);
#endif
#endif
#ifdef V_CUSTOM_FEATURE_PACK_hitachi_nedo
		h_side += generateHtmlForMenu(_("zigbeeMenu"), [
				["zigbeeSubmenu1", _("zigbeeSubmenu1"),'/zigbeeConfig.html'],
				["zigbeeSubmenu2", _("zigbeeSubmenu2"),'/zigbeeStationList.html'],
				["zigbeeSubmenu3", _("zigbeeSubmenu3"),'/zigbeeData.html']
				] );
#endif
#ifdef V_BLUETOOTH
#ifdef V_MODCOMMS_y
	if (rf_mice_ready == "ready") {
#endif
		h_side += generateHtmlForMenu(_("bluetooth"), [
				["BT_CONFIG", _("bluetooth_config"),'bluetooth_config.html'],
				["BT_DEVICES", _("bluetooth_devices"),'bluetooth_devices.html']
				] );
#ifdef V_MODCOMMS_y
	}
#endif
#endif
#ifdef V_QOS_CBQ_INIT
		h_side += generateHtmlForMenu(_("qOs"), [
				["cbqInit_menuItem", _("cbqInit"), '/qos_cbq_init.html']
				]);
#endif
#endif

        h_side+="</ul>";
	break;
#endif /* !defined V_NETWORKING_UI_none */

#if !defined V_SERVICES_UI_none
	case "Services":
		var h_side="<ul>";
#if (!defined V_WAN_INTERFACE_none) && (!defined V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y) && (!defined V_DDNS_WEBUI_none)
		h_side += genHtmlForSimpleMenu("DDNS",_("man ddns"),'/ddns.html');
#endif
#ifndef V_NTP_WEBUI_none
		h_side += genHtmlForSimpleMenu("NTP",_("NTP"),'/NTP.html');
#endif
#ifdef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
				h_side += generateHtmlForMenu(_("remoteManagement"), [
					["SNMP",_("SNMP"),'/snmp.html'],
					["TR",_("tr069"),'/TR069.html']
				]);
#else
#ifdef V_DATA_STREAM_SWITCH_y
		h_side += generateHtmlForMenu(_("dataStreamSwitch"), [
				["EDP",_("endPoints"),'/end_points.html'],
				["DSS",_("streams"),'/data_stream.html']
			]);
#endif

#if !defined(V_SERIAL_none) && !defined(V_IOBOARD_clarke)
#ifdef V_MODCOMMS_y
		// display/hide GPS menu depending on GPS mice status
		if (io_mice_ready == "ready" || aeris_mice_ready == "ready") {
			h_side += generateHtmlForMenu(_("legacyDataManagers"), [
					["EMU",_("modemEmulator"),'/v250.html'],
					["PADD",_("PADD"),'/padd.html']
				]);
		} else {
			h_side += generateHtmlForMenu(_("legacyDataManagers"), [
					["PADD",_("PADD"),'/padd.html']
				]);
		}
#else
		h_side += generateHtmlForMenu(_("legacyDataManagers"), [
				["EMU",_("modemEmulator"),'/v250.html'],
				["PADD",_("PADD"),'/padd.html']
			]);
#endif
#elif defined(V_DATA_STREAM_SWITCH_y)
		h_side += genHtmlForSimpleMenu("PADD",_("PADD"),'padd.html');
#endif

#ifndef V_REMOTE_MGMT_WEBUI_none
		var remoteManagementMenus = [];
#ifdef	V_NETSNMP
		remoteManagementMenus.push( ["SNMP",_("SNMP"),'/snmp.html']);
#endif
#if !defined V_WAN_INTERFACE_none && !defined V_TR069_none
		remoteManagementMenus.push(["TR",_("tr069"),'/TR069.html'] );
#endif
#ifndef V_OMA_DM_LWM2M_none
		remoteManagementMenus.push(["LWM2M",_("LWM2M"),'/LWM2M.html']);
#endif
		h_side += generateHtmlForMenu(_("remoteManagement"), remoteManagementMenus );
#endif

#if defined V_GPS && !defined V_GPS_WEBUI_none
#ifdef V_MODCOMMS_y
		// display/hide GPS menu depending on GPS mice status
		if (gps_mice_ready == "ready" || gps_can_mice_ready == "ready") {
#endif
		h_side += generateHtmlForMenu(_("gps"), [
				["GPS",_("gps configuration"),'/gps.html']
#ifdef V_HAS_AGPS_msb
				,["MSB",_("agps"),'/msb.html']
#endif
#ifdef V_ODOMETER_y
				,["ODOMETER",_("odometer"),'odometer.html']
#endif
#ifdef V_GPS_GEOFENCE_y
				,["GEOFENCE",_("geofence"),'/gps_geofence.html']
#endif
			]);
#ifdef V_MODCOMMS_y
		}
#endif
#endif	/* GPS */
#ifdef V_AUTODIAL
		h_side += genHtmlForSimpleMenu("AUTODIAL",_("autodial menu title"),'autodial.html');
#endif
#ifdef V_USSD
		h_side += genHtmlForSimpleMenu("USSD",_("ussd"),'ussd.html');
#endif
#if defined(V_IOMGR_kudu) || defined(V_IOMGR_clarke)
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), '/IO_configuration.html');
#elif defined(V_MODCOMMS_y)
	if ( typeof IO_configurationPage != 'undefined'
		&& (io_mice_ready == 'ready' || aeris_mice_ready == 'ready' || chubb_mice_ready == 'ready'))
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), IO_configurationPage );
#else
	if ( typeof IO_configurationPage != 'undefined' )
		h_side += genHtmlForSimpleMenu("IOCONFIG",_("ioConfiguration"), IO_configurationPage );
#endif
#ifdef V_SLIC
		h_side += genHtmlForSimpleMenu("VOICE",_("voiceMenu"),'voice.html');
#endif
#ifdef V_EVENT_NOTIFICATION
		h_side += generateHtmlForMenu(_("event noti"), [
				["EVENT_NOTI",_("eventNotificationConf"),'event_noti.html'],
				["EVENT_DEST",_("eventDestinationConf"),'event_dest.html']
			]);
#endif
#ifdef V_EMAIL_CLIENT
		h_side += genHtmlForSimpleMenu("EMAIL_CLIENT",_("email settings"),'email_client.html');
#endif
#ifdef V_SMS
		h_side += generateHtmlForMenu(_("sms title"), [
				["SMS_Setup",_("setup"),'/SMS_Setup.html'],
				["SMS_NewMag",_("newmsg"),'/SMS_New_Message.html'],
				["SMS_Inbox",_("inbox"),'/SMS_Inbox.html'],
				["SMS_Outbox",_("sentItems"),'/SMS_Outbox.html'],
				["SMS_Diag",_("diag"),'SMS_Diagnostics.html']
			]);
#endif
#ifdef V_CUSTOM_FEATURE_PACK_hitachi_nedo
		h_side += genHtmlForSimpleMenu("XEMS",_("XEMSCfg"),'/XEMS.html');
#endif
#endif
#ifdef V_SPEED_TEST_y
		h_side += genHtmlForSimpleMenu("NETWORK_QUALITY",_("networkQuality"),'network_quality.html');
#endif
#if defined V_CBRS_SAS_y && defined V_CBRS_SAS_WEBUI_y
		h_side += generateHtmlForMenu(_("sas title"), [
				["CbrsParameters",_("CBRS parameters"),'/CBRS_parameters.html'],
				["SpeedTest",_("speed test"),'/speed_test.html'],
			]);
#endif
		h_side += "</ul>";
	break;
#endif /* !defined V_SERVICES_UI_none */

	case "System":
		var h_side="<ul>";
		h_side+= generateHtmlForMenu(_("log"), [
				["LOG",_("systemLog"),'/logfile.html']
#ifdef V_DIAGNOSTIC_LOG
				,["DIAGNOSTICLOG",_("DiagnosticLog"),'/diagnosticlog.html']
#endif
#if (!defined V_PRODUCT_hth_70) && (!defined V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y)
#ifndef V_IPSEC_none
				,["IPSECLOG",_("IPsecLog"),'/ipseclog.html']
#endif
#endif
#ifdef V_EVENT_NOTIFICATION
				,["EVENTNOTILOG",_("event noti log"),'/eventnotilog.html']
#endif
				,["LOGSETTINGS",_("systemLogSettings"),'/logsettings.html']
			]);
#if !defined V_SYSTEM_CONFIG_UI_none
#ifdef V_CUSTOM_FEATURE_PACK_bellca
		if(user=="admin") {
			h_side+= generateHtmlForMenu(_("systemConfiguration"), [
					["SETTINGS",_("restorefactoryDefaults"),'/SaveLoadSettings.html']
				]);
		}
#elif !defined V_SYSTEM_CONFIG_WEBUI_none
		if(user=="root") {
			h_side+= generateHtmlForMenu(_("systemConfiguration"), [
#if !defined V_SYSTEM_CONFIG_SETTINGS_WEBUI_none
#if !defined V_SYSTEM_CONFIG_SETTINGS_BACKUP_RESTORE_WEBUI_none
					["SETTINGS",_("settingsBackupRestore"),'/SaveLoadSettings.html']
#else
					["SETTINGS",_("factoryReset"),'/SaveLoadSettings.html']
#endif
#endif
#if !defined V_SYSTEM_CONFIG_UPLOAD_WEBUI_none
					,["UPLOAD",_("upload"),'/AppUpload.html']
#endif
#if !defined V_SYSTEM_CONFIG_PKG_MNGR_WEBUI_none
					,["PKG_MANAGER",_("pkg manager"),'pkManager.html']
#endif
#ifdef V_IPK_FW_SIGNING
					,["FW_SIGNATURE",_("firmwareSignature"),'fw_signature.html']
#endif
				]);
		}
#endif
#endif  /* ! defined V_SYSTEM_CONFIG_UI_none */
#if !defined V_ADMINISTRATION_UI_none
		h_side+= generateHtmlForMenu(_("administration"), [
				["ADMINISTRATION",_("administrationSettings"),'/administration.html']
#ifndef V_SERVER_CERTIFICATE_WEBUI_none
				,["CERTIFICATE",_("serverCertificate"),'server_certificate.html']
#endif
#ifndef V_SSH_KEY_MGMT_WEBUI_none
				,["SSH",_("sshManagement"),'SSH.html']
#endif
#ifndef V_LEDPW_SAVE_none
				,["LED",_("led operation mode"),'led_mode.html']
#endif

#ifndef V_SFTPC_none
				,["SFTPSettings",_("sftpSettings"),'sftp_client.html']
#endif

#ifndef V_TCP_KEEPALIVE_none
				,["TCPKeepalive",_("keepalive settings"),'tcp_keepalive.html']
#endif
			]);
#endif /* !defined V_ADMINISTRATION_UI_none */
		h_side+="<li id='customMenu' style='display:none'>\
				<a class='expandable'>"+_("customMenu")+"</a>\
				<div class='submenu' id='subCustomMenu'></div>\
			</li>";
#if !defined V_WATCHDOG_UI_none
#ifndef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
#ifdef V_WATCHDOG_SETTING_UI_y
		h_side+= generateHtmlForMenu(_("treeapp sysMonitor"), [
			["Sys_Monitor",_("pingWatchDog"),'/ltph.html']

#ifdef V_STARTUP_WATCHDOG_y
			,["StartupWatchdog",_("startupWatchdog"),'/startup_watchdog.html']
#endif
#ifdef V_SHUTDOWN_WATCHDOG_y
			,["ShutdownWatchdog",_("shutdownWatchdog"),'/shutdown_watchdog.html']
#endif
#ifdef V_ETHERNET_PACKET_PORT_WATCHDOG_y
			,["EthernetPacketWatchdog",_("ethernetPacketWatchdog"),'/ethernet_packet_port_watchdog.html']
#endif
#ifdef V_USB_ENUMERATION_WATCHDOG_y
			,["USBEnumerationWatchdog",_("usbEnumerationWatchdog"),'/usb_enumeration_watchdog.html']
#endif
#ifdef V_DHCP_SERVER_WATCHDOG_y
			,["DHCPServerWatchdog",_("dhcpServerWatchdog"),'/dhcp_server_watchdog.html']
#endif
			]);
#else
		h_side+= genHtmlForSimpleMenu("Sys_Monitor",_("treeapp sysMonitor"),'/ltph.html');
#endif
#endif // V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
#endif /* !defined V_WATCHDOG_UI_none */

#ifdef V_POWERSAVE_y
		h_side+= genHtmlForSimpleMenu("POWERSAVE",_("powerManagement"),'/low_power_standby.html');
#endif
		if(user=="root") {
#ifdef V_USB_OTG_MANUAL_MODE_SELECTION
			h_side+=genHtmlForSimpleMenu("USBOTG",_("otgusb"),'/usbotg.html');
#endif
#ifndef V_NAS_none
			// display/hide SD card menu depending on installed package
			if (nas_installed == "1") {
				h_side+=genHtmlForSimpleMenu("NAS",_("nas"),'/nas.html');
			}
#endif
		}
		h_side+= genHtmlForSimpleMenu("RESET",_("setman reboot"),'/Reboot.html')
			+ "</ul>";
	break;
	}
	$("#side-menu").append(h_side);
	}

	set_var_tag();

	hidePPPoE();

	$.get("/cgi-bin/usermenu.cgi", function(v) {
		if (v!="") {
			$("#customMenu").css("display", "");
			$("#subCustomMenu").html(v);
		}
	});

	$("input[type=text]").keyup(function(e) {
		var code = e.keyCode || e.which;
		if (code == '9') {
			$(this).select();
		}
	});
//	$(this).attr("title", _("HTML title"));
	$(document).attr("title", _("HTML title"));
}
/************************************************************************/
#endif // V_WEBIF_SPEC_vdf
/************************************************************************/

function hidePPPoE()
{
#ifdef V_ROUTER_TERMINATED_PPPOE
	if(service_pppoe_server_0_enable=="1" && service_pppoe_server_0_wanipforward_enable=="1") {
#else
	if(service_pppoe_server_0_enable=="1") {
#endif
		$(".hide_for_pppoe_en").css("display", "none");
		$(".pppoeEnablesMsg").css("display", "");
	}
}


function blockUI_alert(msg, func) {
	if($.type(func)!="undefined") {
		myfunc=func;
	}
	else {
		myfunc=function() {
		};
	}

	$.blockUI( {message: msg+"\
		<div class='button-raw med'>\
		<button class='secondary med' onClick='$.unblockUI();myfunc();'>"+_("CSok")+"</button>\
		</div>", css: {width:'320px', padding:'20px 20px'}
	});
}

function blockUI_alert_l(msg, func) {
	if($.type(func)!="undefined") {
		myfunc=func;
	}
	else {
		myfunc=function() {
		};
	}

	if($.type(msg)!="undefined" && msg.length>50) {
		align="left";
	}
	else {
		align="center";
	}
	$.blockUI( {message: "<div style='text-align:"+align+";'>"+msg+"\
		<div class='button-raw med'>\
		<button class='secondary med' onClick='$.unblockUI();myfunc();'>"+_("CSok")+"</button>\
		</div></div>", css: {width:'320px', padding:'20px 20px'}
	});
}
/**
 * A wrapper of jquery blockUI function.
 * @param funcAction for "confirm" button
 * @param func2 Action for "cancel" button if it is defined.
 */
function blockUI_confirm(msg, func1, func2) {
	myfunc=func1;
	myfunc2=function(){return;} //Default func2 does nothing.
	if (typeof func2 == 'function') {
		myfunc2=func2
	}
	$.blockUI( {message: msg+"\
		<div class='button-double'>\
		<button class='secondary med' onClick='$.unblockUI();myfunc();'>"+_("CSok")+"</button><button class='secondary med' onClick='$.unblockUI();" + "myfunc2();'>"+_("cancel")+"</button>\
		</div>", css: {width:'380px', padding:'20px 20px'}
	});
}

function blockUI_confirm_l(msg, func1, func2) {
	myfunc=func1;
	myfunc2=function(){return;}
	if (typeof func2 == 'function') {
		myfunc2=func2
	}
	if(msg.length>50) {
		align="left";
	}
	else {
		align="center";
	}
	$.blockUI( {message: "<div style='text-align:"+align+";'>"+msg+"\
		<div class='button-double'>\
		<button class='secondary med' onClick='$.unblockUI();myfunc();'>"+_("CSok")+"</button><button class='secondary med' onClick='$.unblockUI();" + "myfunc2();'>"+_("cancel")+"</button>\
		</div></div>", css: {width:'380px', padding:'20px 20px'}
	});
}

/************************* Validator *******************************/
#ifdef V_WEBIF_SPEC_vdf
var VALIDATOR=VF;

function validate_alert( t1, t2, t3 ) {
	$.unblockUI();
	if(typeof(t1)=="undefined" || t1=="") {
		t1=_("errorsTitle");
	}
	if(typeof(t2)=="undefined" || t2=="") {
		t2=VALIDATOR.config.errors.summary;
	}
	var e=$(window.document.forms[0]), g="form-error";
	a='<li><a class="link-text jump-link" href="#{id}"><span class="icon icon-arrow-r"></span>{error}</a></li>';
	h='<div class="note" id="'+g+'">\
			<div class="wrap failure">\
				<h2><span class="access">'+VALIDATOR.config.strings.error+'</span>'+t1+'</h2>';
				if (t3==true) {
					var len = t2.length;
					for (i = 0; i < len; i++)
						h+='<p>'+t2[i]+'</p>';
				} else {
						h+='<p>'+t2+'</p>';
				}
						h+='<ul class="list-plain"></ul>\
			</div>\
		</div>';
	h=$(h);
	if(e.attr("data-summary")==="false") {
		return;
	}
	e.prev(".note").remove();
	e.before(h);
	window.location.hash=g;
}

function clear_alert() {
	$(".note").remove();
}

function success_alert(t1, t2) {
	if(typeof(t1)=="undefined" || t1=="") {
		t1=_("succCongratulations");
	}
	if(typeof(t2)=="undefined" || t2=="") {
		t2=_("submitSuccess");
	}
	var e=$(window.document.forms[0]), g="form-success";
	a='<li><a class="link-text jump-link" href="#{id}"><span class="icon icon-arrow-r"></span></a></li>';

	h='<div class="note">\
			<div class="wrap success" style="padding-bottom:6px">\
				<h2>'+t1+'</h2>\
				<p>'+t2+'</p>\
			</div>\
		</div>';
	h=$(h);
	if(e.attr("data-summary")==="false") {
		return;
	}
	e.prev(".note").remove();
	e.before(h);
	window.location.hash=g;
}

VF.config.errors.summary="Please correct these error(s)";
#else // V_WEBIF_SPEC_vdf - NTC skin:
function validate_alert( t1, t2, t3 ) {
	$.unblockUI();
	if(typeof(t1)=="undefined" || t1=="") {
		t1=_("errorsTitle");
	}
	if(typeof(t2)=="undefined" || t2=="") {
		t2=eval(VALIDATOR.config.errors.summary);
	}
	var e=$(window.document.forms[0]), g="form-error";
	a='<li><a class="link-text jump-link" href="#{id}"><span class="icon icon-arrow-r"></span>{error}</a></li>';
	h='<div class="note" id="'+g+'">\
			<div class="wrap failure">\
				<h2><span class="access">'+VALIDATOR.config.strings.error+'</span>'+t1+'</h2>';
				if (t3==true) {
					var len = t2.length;
					for (i = 0; i < len; i++)
						h+='<p>'+t2[i]+'</p>';
				} else {
					h+='<p>'+t2+'</p>';
				}
				h+='<ul class="list-plain"></ul>\
			</div>\
		</div>';
	h=$(h);
	if(e.attr("data-summary")==="false") {
		return;
	}
	e.prev(".note").remove();
	e.before(h);
	window.location.hash=g;
	$("#form").validationEngine("updatePromptsPosition");
}

function clear_alert() {
	var e=$(window.document.forms[0]);
	e.prev(".note").remove();
	$("#form").validationEngine("hideAll");
}

function success_alert(t1, t2) {
	if(typeof(t1)=="undefined" || t1=="") {
		t1=_("succCongratulations");
	}
	if(typeof(t2)=="undefined" || t2=="") {
		t2=_("submitSuccess");
	}
	var e=$(window.document.forms[0]), g="form-success";

	h='<div class="note">\
			<div class="wrap success" style="padding-bottom:6px">\
				<h2>'+t1+'</h2>\
				<p>'+t2+'</p>\
			</div>\
		</div>';
	h=$(h);
	if(e.attr("data-summary")==="false") {
		return;
	}
	e.prev(".note").remove();
	e.before(h);
	window.location.hash=g;
}

(function(){this.VALIDATOR={}}).call(this);

(function() {
VALIDATOR.config={
	strings:{
		error: "_('log Error')+' - '"
	},
	errors:{
		error: "_('log Error')+' - '",
		summary: "_('errorsSummary')",
		title: "_('validatorTitle')"
	}
};
}(VALIDATOR));

/*************************************************************/

(function($) {
$.fn.validationEngineLanguage = function(){};
$.validationEngineLanguage = {
	newLang: function(){
		$.validationEngineLanguage.allRules = {
			"required": { // Add your regex rules here, you can take telephone as an example
				"regex": "none",
				"alertText": "_('fieldRequired')",
				"alertTextCheckboxMultiple": "* Please select an option",
				"alertTextCheckboxe": "* This checkbox is required",
				"alertTextDateRange": "* Both date range fields are required"
			},
			"requiredInFunction": {
				"func": function(field, rules, i, options){
					return (field.val() == "test") ? true : false;
				},
				"alertText": "* Field must equal test"
			},
			"dateRange": {
				"regex": "none",
				"alertText": "* Invalid ",
				"alertText2": "Date Range"
			},
			"dateTimeRange": {
				"regex": "none",
				"alertText": "* Invalid ",
				"alertText2": "Date Time Range"
			},
			"minSize": {
				"regex": "none",
				"alertText": "* Minimum ",
				"alertText2": " characters required"
			},
			"maxSize": {
				"regex": "none",
				"alertText": "* Maximum ",
				"alertText2": " characters allowed"
			},
			"groupRequired": {
				"regex": "none",
				"alertText": "_('groupRequiredError')"
			},
			"min": {
				"regex": "none",
				"alertText": "* Minimum value is "
			},
			"max": {
				"regex": "none",
				"alertText": "* Maximum value is "
			},
			"past": {
				"regex": "none",
				"alertText": "* Date prior to "
			},
			"future": {
				"regex": "none",
				"alertText": "* Date past "
			},
			"maxCheckbox": {
				"regex": "none",
				"alertText": "* Maximum ",
				"alertText2": " options allowed"
			},
			"minCheckbox": {
				"regex": "none",
				"alertText": "* Please select ",
				"alertText2": " options"
			},
			"equals": {
				"regex": "none",
				"alertText": _("passwordMismatch")
			},
			"creditCard": {
				"regex": "none",
				"alertText": "* Invalid credit card number"
			},
			"phone": {
				// credit: jquery.h5validate.js / orefalo
				"regex": /^([\+][0-9]{1,3}[\ \.\-])?([\(]{1}[0-9]{2,6}[\)])?([0-9\ \.\-\/]{3,20})((x|ext|extension)[\ ]?[0-9]{1,4})?$/,
				"alertText": "* Invalid phone number"
			},
			"email": {
				// HTML5 compatible email regex ( http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#    e-mail-state-%28type=email%29 )
				"regex": /^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/,
				"alertText": "* Invalid email address"
			},
			"integer": {
				"regex": /^[\-\+]?\d+$/,
				"alertText": "* Not a valid integer"
			},
			"number": {
				// Number, including positive, negative, and floating decimal. credit: orefalo
				"regex": /^[\-\+]?((([0-9]{1,3})([,][0-9]{3})*)|([0-9]+))?([\.]([0-9]+))?$/,
				"alertText": "* Invalid floating decimal number"
			},
			"date": {
				//	Check if date is valid by leap year
		"func": function (field) {
				var pattern = new RegExp(/^(\d{4})[\/\-\.](0?[1-9]|1[012])[\/\-\.](0?[1-9]|[12][0-9]|3[01])$/);
				var match = pattern.exec(field.val());
				if (match == null)
					return false;

				var year = match[1];
				var month = match[2]*1;
				var day = match[3]*1;
				var date = new Date(year, month - 1, day); // because months starts from 0.

				return (date.getFullYear() == year && date.getMonth() == (month - 1) && date.getDate() == day);
			},
			"alertText": "* Invalid date, must be in YYYY-MM-DD format"
			},
			"ipv4": {
				"regex": /^((([01]?[0-9]{1,2})|(2[0-4][0-9])|(25[0-5]))[.]){3}(([0-1]?[0-9]{1,2})|(2[0-4][0-9])|(25[0-5]))$/,
				"alertText": "* Invalid IP address"
			},
			"url": {
				"regex": /^(https?|ftp):\/\/(((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:)*@)?(((\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5]))|((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.?)(:\d*)?)(\/((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)+(\/(([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)*)*)?)?(\?((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)|[\uE000-\uF8FF]|\/|\?)*)?(\#((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)|\/|\?)*)?$/i,
				"alertText": "* Invalid URL"
			},
			"coapurl": {
				// similar to above, but different proto and no query or fragment allowed
				"regex": /^(coaps?):\/\/(((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:)*@)?(((\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5])\.(\d|[1-9]\d|1\d\d|2[0-4]\d|25[0-5]))|((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.?)(:\d*)?)(\/((([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)+(\/(([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(%[\da-f]{2})|[!\$&'\(\)\*\+,;=]|:|@)*)*)?)?$/i,
				"alertText": "* Invalid URL; example: coap://server.com"
			},
			"onlyNumber": {
				"regex": /^[0-9]+$/,
				"alertText": "* Numbers only"
			},
			"onlyNumberSp": {
				"regex": /^[0-9\ ]+$/,
				"alertText": "* Numbers only"
			},
			"onlyLetterSp": {
				"regex": /^[a-zA-Z\ \']+$/,
				"alertText": "* Letters only"
			},
			"onlyLetterNumber": {
				"regex": /^[0-9a-zA-Z]+$/,
				"alertText": "* No special characters allowed"
			},
			// --- CUSTOM RULES -- Those are specific to the demos, they can be removed or changed to your likings
			"ajaxUserCall": {
				"url": "ajaxValidateFieldUser",
				// you may want to pass extra data on the ajax call
				"extraData": "name=eric",
				"alertText": "* This user is already taken",
				"alertTextLoad": "* Validating, please wait"
			},
			"ajaxUserCallPhp": {
				"url": "phpajax/ajaxValidateFieldUser.php",
				// you may want to pass extra data on the ajax call
				"extraData": "name=eric",
				// if you provide an "alertTextOk", it will show as a green prompt when the field validates
				"alertTextOk": "* This username is available",
				"alertText": "* This user is already taken",
				"alertTextLoad": "* Validating, please wait"
			},
			"ajaxNameCall": {
				// remote json service location
				"url": "ajaxValidateFieldName",
				// error
				"alertText": "* This name is already taken",
				// if you provide an "alertTextOk", it will show as a green prompt when the field validates
				"alertTextOk": "* This name is available",
				// speaks by itself
				"alertTextLoad": "* Validating, please wait"
			},
				"ajaxNameCallPhp": {
					// remote json service location
					"url": "phpajax/ajaxValidateFieldName.php",
					// error
					"alertText": "* This name is already taken",
					// speaks by itself
					"alertTextLoad": "* Validating, please wait"
				},
			"validate2fields": {
				"alertText": "* Please input HELLO"
			},
			//tls warning:homegrown not fielded
			"dateFormat":{
				"regex": /^\d{4}[\/\-](0?[1-9]|1[012])[\/\-](0?[1-9]|[12][0-9]|3[01])$|^(?:(?:(?:0?[13578]|1[02])(\/|-)31)|(?:(?:0?[1,3-9]|1[0-2])(\/|-)(?:29|30)))(\/|-)(?:[1-9]\d\d\d|\d[1-9]\d\d|\d\d[1-9]\d|\d\d\d[1-9])$|^(?:(?:0?[1-9]|1[0-2])(\/|-)(?:0?[1-9]|1\d|2[0-8]))(\/|-)(?:[1-9]\d\d\d|\d[1-9]\d\d|\d\d[1-9]\d|\d\d\d[1-9])$|^(0?2(\/|-)29)(\/|-)(?:(?:0[48]00|[13579][26]00|[2468][048]00)|(?:\d\d)?(?:0[48]|[2468][048]|[13579][26]))$/,
				"alertText": "* Invalid Date"
			},
			//tls warning:homegrown not fielded
			"dateTimeFormat": {
				"regex": /^\d{4}[\/\-](0?[1-9]|1[012])[\/\-](0?[1-9]|[12][0-9]|3[01])\s+(1[012]|0?[1-9]){1}:(0?[1-5]|[0-6][0-9]){1}:(0?[0-6]|[0-6][0-9]){1}\s+(am|pm|AM|PM){1}$|^(?:(?:(?:0?[13578]|1[02])(\/|-)31)|(?:(?:0?[1,3-9]|1[0-2])(\/|-)(?:29|30)))(\/|-)(?:[1-9]\d\d\d|\d[1-9]\d\d|\d\d[1-9]\d|\d\d\d[1-9])$|^((1[012]|0?[1-9]){1}\/(0?[1-9]|[12][0-9]|3[01]){1}\/\d{2,4}\s+(1[012]|0?[1-9]){1}:(0?[1-5]|[0-6][0-9]){1}:(0?[0-6]|[0-6][0-9]){1}\s+(am|pm|AM|PM){1})$/,
				"alertText": "* Invalid Date or Date Format",
				"alertText2": "Expected Format: ",
				"alertText3": "mm/dd/yyyy hh:mm:ss AM|PM or ",
				"alertText4": "yyyy-mm-dd hh:mm:ss AM|PM"
			}
		};
	}
};
$.validationEngineLanguage.newLang();
})(jQuery);

#endif // V_WEBIF_SPEC_vdf

function blockUI_wait(msg) {
	$.blockUI({centerX: true, centerY: true, css: { left: parseInt($(window).width()/2-150)+"px", top:"320px", width: "300px", padding: "20px 30px"}, message: msg+"&nbsp;&nbsp;<i class='progress-sml' style='padding-bottom:2px;'></i>"});
}

function blockUI_wait_progress(msg) {
	$.blockUI({centerX: true, centerY: true, css: { left: parseInt($(window).width()/2-150)+"px", top:"320px", width: "300px", padding: "20px 30px"}, message: "<span id='progress-message'>" + msg + "</span> <p><div id='progressbar'></div>"});
}

function blockUI_wait_confirm(msg, label, call_back) {
	$.blockUI({centerX: true, centerY: true,
		css: { left: parseInt($(window).width()/2-150)+"px", top:"320px", width: "300px", padding: "20px 30px"},
		message: msg+"<p><button type='button' id='wait_confirm' class='secondary'>"});
	$("#wait_confirm").text(label);
	$("#wait_confirm").click(call_back);
}

/******** rdb tool class ********/
function rdb_tool(token) {
	/* init. mset opt */
	var opt_idx=1;
	var opt_obj=new Object();
	var csrf_token=token;
	opt_obj["csrfTokenGet"]=csrf_token;

	this.reset=function() {
		opt_idx=1;
		opt_obj=new Object();
		opt_obj["csrfTokenGet"]=csrf_token;
	};

	this.set_flag=function(flags) {
		opt_obj["flag"]=flags;
	};

	this.add_to_mget=function(cfg){
		var rdb=this;

		$.each(cfg,function(i,o){
			if((typeof o != 'undefined') && (o.el != null))
				rdb.add(o.rdb);
		});
	};

	this.add_to_mset=function(cfg) {
		var rdb=this;

		$.each(cfg,function(i,o){
			var val;

			/* use default value if element not specified */
			if(typeof o != 'undefined')
				if(o.el == null)
					val=o.def;
				else
					val=load_value_from_element(o.el);

			if((typeof o != 'undefined') && (o.el != null))
				rdb.add(o.rdb,val);
		});
	};

	this.get_ctrls=function(cfg) {
		var ctrls=new Array();

		$.each(cfg,function(i,o){

			if((typeof o == 'undefined') || (o.el == null))
				return;

			if(is_ip_address_element(o.el))
				ctrls.push(get_ip_address_elements(o.el));
			else
				ctrls.push(o.el);
		});

		return ctrls.join(",");
	};

	this.disable_ctrl=function(cfg,dis) {
		$(this.get_ctrls(cfg)).attr("disabled",dis);
	};

	this.pour_to_ctrl=function(res,cfg) {
		$.each(cfg,function(i,o){

			/* bypass if element not specified */
			if((typeof o == 'undefined') || (o.el == null))
				return true;

			var val;

			if($.type(res[o.rdb]) === "undefined" || $.type(res[o.rdb]) === "null" || res[o.rdb] == "")
				val=o.def;
			else
				val=res[o.rdb];

			load_value_to_element(o.el,val);
		});
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


	this.wait_for_rdb_chg=function(rdb_to_wait,cur,timeout,func) {

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
				func(cur);
				return;
			}

			/* check rdb */
			rdb.reset();
			rdb.add(rdb_to_wait);
			rdb.mget(function(res){
				if(res[rdb_to_wait]==cur) {
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

	/* wait for rdb result */
	this.wait_for_rdb_result=function(rdb_to_wait,timeout,func) {
		this.wait_for_rdb_chg(rdb_to_wait,"",timeout,func);
	}

}

/* standard cgi call */
function cgi(bin, token) {

	var url=bin;

	/* init. mset opt */
	var opt_idx=1;
	var opt_obj=new Object();

	/* CSRF token */
	var csrf_token;
	if (token !== undefined) {
		csrf_token=token;
		opt_obj["csrfTokenGet"]=csrf_token;
	}

	this.reset=function() {
		opt_idx=1;
		opt_obj=new Object();
		if (csrf_token !== undefined) {
			opt_obj["csrfTokenGet"]=csrf_token;
		}
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
	this.up=function(el,complete_func,preserveResponseFormatting) {
		if (preserveResponseFormatting === undefined) {
			preserveResponseFormatting = false;
		}
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
			if (preserveResponseFormatting) {
				var res=doc.find("body pre").text();
			} else {
				var res=doc.find("body pre").html();
			}

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

	this.poll=function(interval,cmd,func) {
		var this_cgi=this;

		this.run(cmd,function(r){
			var res=func(r);

			if((res===undefined || res==true) && (interval>0))
				setTimeout(
					function() {
						this_cgi.poll(interval,cmd,func);
					},
					interval
				);
		});
	}
}


//--------------------------------------------------------------------------------
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

function check_phoneRegex(e) {
//var phoneRegEx=/^((\+\d{1,3}(-| )?\(?\d\)?(-| )?\d{1,3})|(\(?\d{2,3}\)?))(-| )?(\d{3,4})(-| )?(\d{4})(( x| ext)\d{1,5}){0,1}$/;
//if(!e.value.match(phoneRegEx))

var phoneRegEx = /[^(\d+\+)]/g;
	e.value=e.value.replace(phoneRegEx,'');
}

function overridewindowAlert() {
	windowConfirm=window.confirm;
	windowPrompt=window.prompt;

	// alert
	window.alert = function(txt) {
		blockUI_alert_l( check_insert_rtl( txt ) );
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

function get_ip_address_elements(el) {

	if(el[0]=="#")
		return false;

	var els=new Array();

	/* build element names */
	for(i=1;i<=4;i++)
		els.push("#" + el + i);

	var ids=els.join(",");
	if( $(ids).filter(".ip-adress").length==4 )
		return ids;

	return "";
}

function is_ip_address_element(el) {
	return get_ip_address_elements(el)!="";
}

function load_value_from_element(el) {

	if(is_ip_address_element(el))
		return parse_ip_from_fields(el);
	else if($(el).is("input:radio.access")) {
		if( $(el).filter(":checked").length>0 )
			return $(el).filter(":checked").val();
	}

	return $(el).val();
}

function load_value_to_element(el,val) {
	var toggle_element;

	if($(el).is("input:checkbox"))
		$(el).prop("checked",val);
	else if($(el).is("input:radio.access")) {

		if($.type(val)=="string") {
			val=(val=="on" || val=="1")?true:false;
		}
		else if($.type(val)=="number") {
			val=(val>0)?true:false;
		}

		if($.type(val)=="boolean") {

			if( $(el).filter("[value=on]").length>0 )
				filter=val?"[value=on]":"[value=off]";
			if( $(el).filter("[value=yes]").length>0 )
				filter=val?"[value=yes]":"[value=no]";
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

		toggle_element=$(el).parent().attr("data-toggle-element");
		if(toggle_element!==undefined) {
			$("#"+toggle_element).toggle(val);
		}
	}
	else if($(el).is("select")) {
		$(el).children("[value='"+val+"']").attr("selected",true);
		$(el).val(val);
	}
	else if(is_ip_address_element(el)) {
		parse_ip_into_fields(val,el);
	}
	else if($(el).is("span")) {
		$(el).html(val);
	}
	else {
		$(el).val(val);
	}
}

/*
	cfg={
		"#enable":"link.profile.1.enable"
		.
		.
	}
*/
function load_values_from_elements(cfg) {
	var res={};

	$.each(cfg, function(el,rdb) {
		res[rdb]=load_value_from_element(el);
	});

	return res;
}

/*
	cfg={
		"#priority":1,
		.
		.
	}
*/
function load_values_to_elements(cfg) {
	$.each(cfg,
		function(el,val) {
			load_value_to_element(el,val);
		}
	);
}

function lang_sentence(stc, arr) {
	for(i in arr) {
		stc=stc.replace("%%"+i, arr[i]);
	}
	return stc;
}

function is_touch_device() {
	return ('ontouchstart' in window) || (navigator.userAgent.indexOf('IEMobile') !== -1);
}

function is_edge_browser() {
	return (navigator.userAgent.indexOf('Edge') !== -1);
}

// This function breaks up the query paramaters ie ?param1=value1&param2=value2 into a returned array
// Typically this can be invoked with var queryParams = parseQueryString( window.location.search.substring(1) );
// Parameter can be accessed directly as queryParams['param1']. This could be undefined.
// Object function queryParams.getParamByName('param1') can also get the variable and will return "" if the value is undefined

function parseQueryString(queryString) {
	var params = {};

	// Split into key/value pairs
	queryString.split("&").forEach ( function(param) {
			temp = param.split('=');
			params[temp[0]] = temp[1];
		} );

	params.getParamByName = function ( paramName ) {
		param = this[paramName];
		if ( param === undefined )
			return "";
		return param;
	};

	return params;
};

//HTML number encode (&#number_entity;)
function htmlNumberEncode(input) {
	var i = 0;
	var c = 0;
	var string = "";
	for (i = 0; i < input.length; i++) {
		c = input.charCodeAt(i);
		string += "&#" + c + ";";
	}
	return string;
}

/**
*
*  Base64 encode / decode
*  http://www.webtoolkit.info/
*
**/
var Base64 = {
    // private property
    _keyStr : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=",
    // public method for encoding
    encode : function (input) {
        var output = "";
        var chr1, chr2, chr3, enc1, enc2, enc3, enc4;
        var i = 0;
        input = Base64._utf8_encode(input);
        while (i < input.length) {
            chr1 = input.charCodeAt(i++);
            chr2 = input.charCodeAt(i++);
            chr3 = input.charCodeAt(i++);
            enc1 = chr1 >> 2;
            enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
            enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
            enc4 = chr3 & 63;
            if (isNaN(chr2)) {
                enc3 = enc4 = 64;
            } else if (isNaN(chr3)) {
                enc4 = 64;
            }
            output = output +
            this._keyStr.charAt(enc1) + this._keyStr.charAt(enc2) +
            this._keyStr.charAt(enc3) + this._keyStr.charAt(enc4);
        }
        return output;
    },
    // public method for decoding
    decode : function (input) {
        var output = "";
        var chr1, chr2, chr3;
        var enc1, enc2, enc3, enc4;
        var i = 0;
        input = input.replace(/[^A-Za-z0-9\+\/\=]/g, "");
        while (i < input.length) {
            enc1 = this._keyStr.indexOf(input.charAt(i++));
            enc2 = this._keyStr.indexOf(input.charAt(i++));
            enc3 = this._keyStr.indexOf(input.charAt(i++));
            enc4 = this._keyStr.indexOf(input.charAt(i++));
            chr1 = (enc1 << 2) | (enc2 >> 4);
            chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
            chr3 = ((enc3 & 3) << 6) | enc4;
            output = output + String.fromCharCode(chr1);
            if (enc3 != 64) {
                output = output + String.fromCharCode(chr2);
            }
            if (enc4 != 64) {
                output = output + String.fromCharCode(chr3);
            }
        }
        output = Base64._utf8_decode(output);
        return output;
    },
    // private method for UTF-8 encoding
    _utf8_encode : function (string) {
        string = string.replace(/\r\n/g,"\n");
        var utftext = "";
        for (var n = 0; n < string.length; n++) {
            var c = string.charCodeAt(n);
            if (c < 128) {
                utftext += String.fromCharCode(c);
            }
            else if((c > 127) && (c < 2048)) {
                utftext += String.fromCharCode((c >> 6) | 192);
                utftext += String.fromCharCode((c & 63) | 128);
            }
            else {
                utftext += String.fromCharCode((c >> 12) | 224);
                utftext += String.fromCharCode(((c >> 6) & 63) | 128);
                utftext += String.fromCharCode((c & 63) | 128);
            }
        }
        return utftext;
    },
    // private method for UTF-8 decoding
    _utf8_decode : function (utftext) {
        var string = "";
        var i = 0;
        var c = c1 = c2 = 0;
        while ( i < utftext.length ) {
            c = utftext.charCodeAt(i);
            if (c < 128) {
                string += String.fromCharCode(c);
                i++;
            }
            else if((c > 191) && (c < 224)) {
                c2 = utftext.charCodeAt(i+1);
                string += String.fromCharCode(((c & 31) << 6) | (c2 & 63));
                i += 2;
            }
            else {
                c2 = utftext.charCodeAt(i+1);
                c3 = utftext.charCodeAt(i+2);
                string += String.fromCharCode(((c & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
                i += 3;
            }
        }
        return string;
    }
}

#ifdef V_MODCOMMS_y
function substMice(str) // This renames mice for branding reasons
{
	str=str.replace(/IO-Mice/i, "NMA-1400");
	str=str.replace(/Aeris-Mice/i, "NMA-1200");
	str=str.replace(/GPS-Mice/i, "NMA-1500");
	str=str.replace(/Chubb-Mice/i, "NMA-1300");
	str=str.replace(/RF-Mice/i, "NMA-0001");
	str=str.replace(/IND-IO-Mice/i, "NMA-2200");
	str=str.replace(/GPS-CAN-Mice/i, "NMA-1500");
	return str;
}
#endif

// alert invalid CSRF token
function alertInvalidCsrfToken() {
	validate_alert( _("invalidTokenTitle"), _("invalidTokenMsg"));
}

// alert invalid request
function alertInvalidRequest() {
	validate_alert( _("invalidRequestTitle"), _("invalidRequestMsg"));
}

// get length in bytes of a string encoded in UTF-8
function getUtf8StringLengthInBytes(string) {
	var length = 0;
	for (var n = 0; n < string.length; n++) {
		var c = string.charCodeAt(n);
		if (c < 128) {
			length++;
		}
		else if((c > 127) && (c < 2048)) {
			length += 2;
		}
		else {
			length += 3;
		}
	}
	return length;
}

// Check password strength before submit
// Input : PassStr : password string
//         score   : if valid number then bypass strength calculation
//                   undefined then calculate strength
// Output : true : valid
//          false : invalid
function passStrengthValidation(PassStr, score) {
	var token, PassLen, result, passScore;
	var nUpper=0, nLower=0, nSpecial=0, nNum=0, nCons=0;

	PassLen = PassStr.length;

	if (typeof(score) == "undefined") {
		// zxcvbn() returns result structure and score range is 0~4.
		result = zxcvbn(PassStr);
		passScore = result.score;
	} else {
		passScore = score;
	}

	// count upper/lower/numeric/special case characters
	token = PassStr.match(/[A-Z]/g);
	if (token)
		nUpper = token.length;
	token = PassStr.match(/[a-z]/g);
	if (token)
		nLower = token.length;
	token = PassStr.match(/[0-9]/g);
	if (token)
		nNum = token.length;
	nSpecial = PassLen - (nUpper + nLower + nNum);

	// to be valid, at least one number & one Upper case &
	// one special character needed & length should be longer than 8
	// minimum score from zxcvbn library should be greater than 2
	return (PassLen <= 128 && PassLen >= 8 && nNum > 0 && nUpper > 0 && nSpecial > 0 && passScore >= 2);
}

// Calculate & get password strength in readable form
// using zxcvbn library
// Input : PassStr : password string
// Output : strength (strong, medium, weak, none)
function getPassStrength(PassStr) {
	var result;

	if (PassStr.length == 0)
		return 'none';

	if (PassStr.length > 128)
		return 'too_long';

	// zxcvbn() returns result structure and score range is 0~4.
	// Only check the last 48 characters of any passwords longer than this.  The process simply
	// takes too long (climbing to seconds per new character entered.  Regular words found in the
	// remainder of the password will still leave this a strong password.
	var result = zxcvbn(PassStr.substring(PassStr.length - 48));
	if (result.score >= 2) {
		// Return 'strong' only when the password satisfies password
		// criterias (at least 1 upper & number & special characters & longer than 8)
		// and zxcvbn library function returns strong score.
		if (passStrengthValidation(PassStr, result.score))
			return 'strong';
		else
			return 'medium';
	} else if (result.score >= 1) {
		return 'medium';
	} else if (result.score >= 0) {
		return 'weak';
	}
	return 'none';
}

// Update password/password strength field or title & colour
// Input : t : password or password strength field
//         strength : strength indicator
//         title : inner html or title
// Output : display password strength with relavant font colour
//         or change title of password field
function updatePassStrengthField(t, strength, title) {
	if (strength == 'strong') {
		if (title)
			t.title = _("strongPassword");
		else
			t.innerHTML = _("strongPassword");
		t.style.color="Green";
	} else if (strength == 'medium') {
		if (title)
			t.title = _("mediumPassword");
		else
			t.innerHTML = _("mediumPassword");
		t.style.color="Orange";
	} else if (strength == 'weak') {
		if (title)
			t.title = _("weakPassword");
		else
			t.innerHTML = _("weakPassword");
		t.style.color="Red";
	} else if (strength == 'too_long') {
		if (title)
			t.title = _("passwordTooLong");
		else
			t.innerHTML = _("passwordTooLong");
		t.style.color="Red";
	} else {
		if (title)
			t.title = "";
		else
			t.innerHTML = "";
		t.style.color="Black";
	}
}


// Calculate & display password strength for WEBUI/Telnet
// Input : f : password input field
//         t : password strength field
// Output : display password strength with relavant font colour
function updatePassStrength(f, t) {
	var PassStr, strength = 0;

	PassStr = f.value;
	strength = getPassStrength(PassStr);

	updatePassStrengthField(t, strength, 0);
}

// Calculate & display password strength for SMS diagnostics
// Input : f : password input field
//         t : password strength field
// Output : display password strength with relavant font colour
function smsPassStrength(f, t) {
	var PassStr, strength = 0;

	// filtering unsafe name first
	filterUsingFn( f, isNameUnsafe );

	updatePassStrength(f, t);
}

// convert special characters to html entity
function convert_to_html_entity(value) {
    var newStr  = "";
	var patt=/[a-zA-Z0-9]/
    for ( i = 0; i < value.length; i++ ) {
        var nextChar = value.charAt(i);
		if (patt.test(nextChar))
			newStr = newStr + nextChar;
		else
			newStr = newStr + convertCharToEntity(nextChar);
    }
    return newStr;
}

var bulletHead=convert_to_html_entity("  • ");

#if (0)
/* notification message for password information icon
   Passwords configured on the router must meet the following criteria:
     • Be a minimum of 8 characters and no more than 128 characters in length.
     • Contain at least one upper case and one number.
     • Contain at least one special character, such as: `~!@#$%^&*()-_=+[{]}\|;:'",<.>/?.
   Additionally, the password must also satisfy an algorithm which analyses the characters
   as you type them, searching for commonly used patterns, passwords, names and surnames
   according to US census data, popular English words from Wikipedia and US television and
   movies and other common patterns such as dates, repeated characters (aaa), sequences (abcd),
   keyboard patterns (qwertyuiop) and substitution of numbers for letters.
 */
#endif
function showStrongPasswordInfo() {
	clear_alert();
	var msg="<div class='message_box' style='text-align:left;'>";
	msg+=_("passwordWarning6")+"<br/>";
	msg+=bulletHead+_("passwordWarning2")+"<br/>";
	msg+=bulletHead+_("passwordWarning3")+"<br/>";
	msg+=bulletHead+_("passwordWarning4")+convert_to_html_entity('`~!@#$%^&*()-_=+[{]}\|;:\'\",\<.>/?.')+"<br/>";
	msg+=_("passwordWarning9");
	msg+="<br/></div><div style='margin-left:180px'><button class='secondary mini' onClick='$.unblockUI();'>"+_("CSok")+"</button><div/>";
	$.blockUI({message:msg, css: {width:'380px', padding:'20px 20px'}});
	return;
}

#if (0)
/* notification message for password information icon
   Passwords configured on the router must meet the following criteria:
     • Be a minimum of 8 characters and no more than 128 characters in length.
     • Contain at least one upper case and one number.
     • Contain at least one of the following special characters: !*()?/
   Additionally, the password must also satisfy an algorithm which analyses the characters
   as you type them, searching for commonly used patterns, passwords, names and surnames
   according to US census data, popular English words from Wikipedia and US television and
   movies and other common patterns such as dates, repeated characters (aaa), sequences (abcd),
   keyboard patterns (qwertyuiop) and substitution of numbers for letters.
 */
#endif
function showStrongSmsPasswordInfo() {
	clear_alert();
	var msg="<div class='message_box' style='text-align:left;'>";
	msg+=_("passwordWarning6")+"<br/>";
	msg+=bulletHead+_("passwordWarning2")+"<br/>";
	msg+=bulletHead+_("passwordWarning3")+"<br/>";
	msg+=bulletHead+_("passwordWarning8")+ convert_to_html_entity('!*()?/.')+"<br/>";
	msg+=_("passwordWarning9");
	msg+="<br/></div><div style='margin-left:180px'><button class='secondary mini' onClick='$.unblockUI();'>"+_("CSok")+"</button><div/>";
	$.blockUI({message:msg});
	return;
}

//----------------------------------------------------------------------------------------------------
//   UTIL.JS FILE INCLUDING CHECK END
//----------------------------------------------------------------------------------------------------
}

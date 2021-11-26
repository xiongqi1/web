<%
// This file implements some utility functions

// This function helps to insert options to a Select
// Parameters:
//  selectId: Id of Select
//  optionValues: Array containing option values
//  optionsText: Array of option text
//
// Example:
// EJS code:
// form['snmpVersion'] = READ-CURRENT-VALUE;
// snmpVersionOptValues = new Array("v1v2c", "v3");
//
// HTML:
// <script language="JavaScript">
// var snmpVersionOptsText = {
//     v1v2c:_("snmpVersionV1V2c"),
//     v3:_("snmpVersionV3")
// };
// </script>
// ...
// <select name="snmpVersion" id="snmpVersion"></select>
// ...
// $(document).ready(function() {
// INVOKE-THIS-EJS-FUNCTION:
//     fillSelectOptions("snmpVersion", snmpVersionOptValues, "snmpVersionOptsText");
// ...
// });
//
function fillSelectOptions(selectId, optionValues, optionsText) {
%>
	var sel = document.getElementById("@@selectId");
	var opt;
<%
	var iSelectOptionIterator;
	for (iSelectOptionIterator = 0; iSelectOptionIterator < optionValues.length; iSelectOptionIterator++) {
%>
		opt = document.createElement("option");
		opt.value = "@@optionValues[iSelectOptionIterator]";
		opt.text = <% write(optionsText); %>.<% write(optionValues[iSelectOptionIterator]); %>;
		opt.selected = false;
		<% if (form[selectId] == optionValues[iSelectOptionIterator]) { %>
			opt.selected = true;
		<% } %>
		sel.add(opt);
<%
	}
}

// check whether a given value matches an element of an array
// Parameters:
//  value: input value
//  arrayValues: input array
// Return:
//  true: yes
//  false: no
function isElementOfArray(value, arrayValues) {
	var iArrayIterator;
	for (iArrayIterator = 0; iArrayIterator < arrayValues.length; iArrayIterator++) {
		if (value == arrayValues[iArrayIterator]) {
			return true;
		}
	}
	return false;
}

// operation executed on bad request
submittedRequestValidity = "submittedRequestValidity";
submittedRequestInvalid = "invalid";
function onBadRequest()
{
	badRequestRedirectLink = request["REQUEST_URI"];
	if (request["QUERY_STRING"] == undefined || request["QUERY_STRING"] == "") {
		badRequestRedirectLink = badRequestRedirectLink + "?";
	}
	else {
		badRequestRedirectLink = badRequestRedirectLink + "&";
	}
	badRequestRedirectLink = badRequestRedirectLink + submittedRequestValidity + "=" + submittedRequestInvalid;

	redirect(badRequestRedirectLink);
	exit(-1);
}

// indication of bad request
function indicateRequestValidity() {
	if(form[submittedRequestValidity]==submittedRequestInvalid) {
%>
<script type="text/javascript">
$(document).ready(function(){
	alertInvalidRequest();
});
</script>
<%
		request['QUERY_STRING'] = trim_string(request['QUERY_STRING'], "&"+submittedRequestValidity+"="+submittedRequestInvalid);
		request['QUERY_STRING'] = trim_string(request['QUERY_STRING'], submittedRequestValidity+"="+submittedRequestInvalid);
	}
}

// set RDB variable only if new value is different from current one
// Parameters:
//  rdbName: name of RDB variable to be set
//  newValue: new value
//  persistFlag: whether set persist flag (true/false)
function setRdbOnDiff(rdbName, newValue, persistFlag) {
	var curValue = get_single(rdbName, "N/A");
	if ((curValue == "N/A") || (curValue != newValue)) {
		var persistOpt = "";
		if (persistFlag == true) {
			persistOpt = "-p";
		}
		set_single_direct(persistOpt, rdbName, newValue);
	}
}

// validate whether a given value is a valid radio value which must be 0 or 1
function validateOnOffRadio(v) {
	if (v != "0" && v != "1") {
		onBadRequest();
	}
}

// validate IP address
// Parameters:
//  ip: input IP address of list of IP address seperated by listDelimiter
//  listDelimiter:
//  extraQuant: Quantification of extra IP addresses (i.e after the first one) in the list.
//   e.g {1} to match X.X.X.X,Y.Y.Y.Y
function validateIpAddress(ip, listDelimiter, extraQuant) {
	var validIpNumber = "([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])";
	var validIpAddress = "(" + validIpNumber + "\\.){3}" + validIpNumber;
	var validIpAddressesPattern;
	if (listDelimiter != undefined && extraQuant != undefined && listDelimiter != "" && extraQuant != "") {
		validIpAddressesPattern = "^" + validIpAddress + "(" + listDelimiter + validIpAddress + ")" + extraQuant + "$";
	}
	else {
		validIpAddressesPattern = "^" + validIpAddress + "$";
	}
	if (string_regex_match(ip, validIpAddressesPattern) == false) {
		onBadRequest();
	}
}

// validate netmask
function validateNetmask(netmask) {
	var leadingOnes="(0|1(28|92)|2(24|40|48|52|54))";
	var allOnes="(255\\.)";
	var validSubnetMask = "^(" + allOnes + "{3}255)|(" + allOnes + "{3}" + leadingOnes + ")|" + "(" + allOnes + "{2}" + leadingOnes + "\\.0" + ")|" + "(" + allOnes + "{1}" + leadingOnes + "(\\.0){2}" + ")|" + "(" + leadingOnes + "(\\.0){3}" + ")$";
	if (string_regex_match(netmask, validSubnetMask) == false) {
		onBadRequest();
	}
}

// validate MAC address
// It validates following formats:
// - Six groups of two hexadecimal digits seperated by hyphens (-)
// - Six groups of two hexadecimal digits separated by colons (:)
// Parameters:
//  MAC: input MAC address
//  delimiter: delimiter between groups of hexadecimal digits. Default: ":"
function validateMacAddress(mac, delimiter) {
	if (delimiter == undefined) {
		delimiter = ":";
	}
	var validMacAddressPattern;
	if (delimiter == ":" || delimiter == "-") {
		validMacAddressPattern = "^([0-9A-Fa-f]{2}" + delimiter + "){5}([0-9A-Fa-f]{2})$"
	}
	else {
		// unsupported formats
		onBadRequest();
	}
	if (string_regex_match(mac, validMacAddressPattern) == false) {
		onBadRequest();
	}
}

// test whether input string can be convert to number
// it ignores if string is undefined, "", or "N/A"
function tryConvertStringNumber(s) {
	if (s != undefined && s != "" && s != "N/A") {
		var n = string_to_number(s);
	}
}

// try to validate on/off radio
// it ignores if input is undefined, "", or "N/A"
function tryValidateOnOffRadio(v) {
	if (v != undefined && v != "" && v != "N/A") {
		validateOnOffRadio(v);
	}
}

// try to validate whether a input value is a decimal number
// it ignores if input is undefined, "", or "N/A"
function tryValidateDecimalNumber(v) {
	if (v != undefined && v != "" && v != "N/A") {
		if (string_regex_match(v, "^[0-9]+$") == false) {
			onBadRequest();
		}
	}
}
// whether an input value is empty
function isEmpty(v){
  return  (v == undefined || v == "" || v == "N/A") ;
}

// validate time in HHMM format
function validateTimeHHMM(v) {
	if (string_regex_match(v, "^[0-2][0-9][0-5][0-9]$") == false) {
		onBadRequest();
	} else {
		var n=string_to_number(v);
		if ((n/100) > 23) {
			onBadRequest();
		}
	}
}

// validate bounded integer
function validateBoundedInteger(v, lower, upper) {
	if (string_regex_match(v, "^[0-9]+$") == false) {
		onBadRequest();
	} else {
		var n=string_to_number(v);
		if (n < lower || n > upper) {
			onBadRequest();
		}
	}
}
%>

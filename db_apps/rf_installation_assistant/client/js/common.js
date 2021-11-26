//
// This file contains support javascript functions used by
// Titan installation assistant.
//

// globals
g_cid = [ null, null, null ];
g_pass = [ null, null, null ];
g_is_html5_supported = true;

// Work out if HTML5 (specifically the meter element) is supported
// If this is a Apple produced browser, return false.
// This will catch Safari running on iPhone, iPad or iPod touch.
// Also IE doesn't support it.
// Will catch Macs as well, but this is ok.
function is_html5_supported()
{

    if (navigator.vendor.match(/Apple/i)) {
        return false;
    }

    // Detect IE - any version. Until version 10 inclusive, MSIE string was present with version number.
    // Starting from version 11, the version is no longer present (hence no MSIE) but IE can be detected
    // by detecting "Trident" string which is the version of MSHTML (Trident)
    var is_ie = navigator.userAgent.indexOf('Trident') != -1 || navigator.userAgent.indexOf('MSIE') != -1;
    if (is_ie) {
        return false;
    }

    return true;
}

// Send HTTP Get request using ajax and asynchronously call a callback function with decoded data
// from response when response arrives
function ajaxGetRequest(url, doJsonParse, successCallback, failCallback) {
    var xmlHttp = new XMLHttpRequest();

    xmlHttp.onreadystatechange = function() {
        if (xmlHttp.readyState == 4) {
            if (xmlHttp.status == 200) {
                console.log("xml request is successful");
                //console.log(xmlHttp.responseText);
                var result;
                if (doJsonParse) {
                    if (xmlHttp.responseText.length > 0) {
                        result = JSON.parse(xmlHttp.responseText);
                    }
                } else {
                    result = xmlHttp.responseText;
                }
                successCallback(result);
            } else {
                if (typeof failCallback === "function") {
                    failCallback(xmlHttp.status, xmlHttp.responseText);
                }
            }
        }
    };

    xmlHttp.open("GET", url, true);
    xmlHttp.send();
}

// Send HTTP POST request using ajax
function ajaxPostRequest(url, params, successCallback, failCallback) {
    var xmlHttp = new XMLHttpRequest();

    xmlHttp.onreadystatechange = function() {
        if(xmlHttp.readyState == 4) {
            if (xmlHttp.status == 200) {
                console.log("post request is successful");
                if (typeof successCallback === "function") {
                    successCallback(xmlHttp.responseText);
                }
            } else {
                if (typeof failCallback === "function") {
                    failCallback(xmlHttp.status, xmlHttp.responseText);
                }
            }
        }
    };

    xmlHttp.open("POST", url, true);
    xmlHttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xmlHttp.send(params);
}

// Send HTTP POST request using ajax
function ajaxPostRequestJson(url, params, successCallback, failCallback) {
    var xmlHttp = new XMLHttpRequest();

    xmlHttp.onreadystatechange = function() {
        if(xmlHttp.readyState == 4) {
            if (xmlHttp.status == 200) {
                console.log("post request is successful");
                if (typeof successCallback === "function") {
                    successCallback(xmlHttp.responseText);
                }
            } else {
                if (typeof failCallback === "function") {
                    failCallback(xmlHttp.status, xmlHttp.responseText);
                }
            }

        }
    };

    xmlHttp.open("POST", url, true);
    xmlHttp.setRequestHeader("Content-type", "application/json");
    xmlHttp.send(JSON.stringify(params));
}

// Send HTTP POST form-data request using ajax
// params should be a FormData instance
function ajaxFormDataPostRequest(url, params, successCallback, failCallback) {
    var xmlHttp = new XMLHttpRequest();

    xmlHttp.onreadystatechange = function() {
        if(xmlHttp.readyState == 4) {
            if (xmlHttp.status == 200) {
                console.log("form-data post request is successful");
                if (typeof successCallback === "function") {
                    successCallback(xmlHttp.responseText);
                }
            } else {
                if (typeof failCallback === "function") {
                    failCallback(xmlHttp.status, xmlHttp.responseText);
                }
            }
        }
    };

    xmlHttp.open("POST", url, true);
    xmlHttp.send(params);
}

// Just send an HTTP Get request and ignore any responses
// Used to implement control functions (such as reset stats)
function ajaxDeleteRequest(url) {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open("DELETE", url, true);
    xmlHttp.send();
}

// Supports saving the time when the test started, and
// then using this timestamp in other pages.
function recordTimestamp(item) {
    var date = new Date();
    localStorage.setItem(item, date);
}

// Return saved timestamp
function retrieveTimestamp(item) {
    var date = localStorage.getItem(item);
    if (date == null) {
        return "N/A";
    }
    return date;
}

// Return the str if exists, otherwise return the alternative.
// If the alternative doesn't exists either, return "N/A"
function displayAltStringIfEmpty(str, alternative) {
    if (str === undefined || str === null || str.length === 0) {
        if (alternative) {
            return alternative;
        }
        return "N/A";
    }
    return str;
}

// Set Websocket report filter mask. Default no filter enabled
// Filter mask
const BITMASK_BATTERY = 0x0001
const BITMASK_ORIENTATION = 0x0002
const BITMASK_RF_STATISTICS = 0x0004
const BITMASK_RF_CURR_READING = 0x0008
const BITMASK_THROUGHPUT_TEST = 0x0010
const BITMASK_SYSTEM_STATUS = 0x0020
const BITMASK_ANTENNA_STATUS = 0x0040
const BITMASK_REGISTRATION_STATUS = 0x0080
const BITMASK_UPGRADING_FW_STATUS = 0x0100
const BITMASK_FACTORY_RESET_STATUS = 0x0200
function setWebsocketReportFilter(mask) {
    ajaxGetRequest("/set_ws_filter?mask="+mask+"&", true, function(data) {
        //console.log(data);
    });
}

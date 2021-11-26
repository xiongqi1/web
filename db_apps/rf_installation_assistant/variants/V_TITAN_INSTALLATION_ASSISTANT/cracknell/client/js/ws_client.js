//
// This file contains javascript functions used by Titan installation assistant.
// Functions in this file handle websocket client side communications
//

function writeToScreen(message) {
    // debug stuff
    // Can add this to html to see debug messages from web socket.
    // <div id="debug_string"></div>
    // and then uncomment the next line
    // document.getElementById("debug_string").innerHTML = message;
}

function initWebSocket(URL) {

    // based on URL, work out if this should be HTTP or HTTPs and hence
    // secure web socket or not.
    if (URL.startsWith("https")) {
        var wsUri = "wss://";
    } else {
        var wsUri = "ws://";
    }
    wsUri = wsUri + document.location.host + "/ws";

    // the second argument (version) has to be supported by the server (see ia_web_server.lua)
    websocket = new WebSocket(wsUri, "v1.installation_assistant");

    websocket.onopen = function(evt) { onOpen(evt); };
    websocket.onclose = function(evt) { onClose(evt); };
    websocket.onmessage = function(evt) { onMessage(evt); };
    websocket.onerror = function(evt) { onError(evt); };
    websocket.onping = function(evt) { writeToScreen("Ping message :" + evt.data); };
}

function onOpen(evt) {
    // @TODO any special handling should go here
}

function onClose(evt) {
    // @TODO any special handling should go here
}

function onError(evt) {
    // @TODO any special handling should go here
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
}

// At the moment this is not used since the client doesn't send any data into web socket
function doSend(message) {
    writeToScreen("SENT: " + message);
    websocket.send(message);
}

// runs when server sends data. This may be one of the following things at the moment
// 1) Current RF readings
// 2) RF Stats
// 3) Battery data
// 4) Throughput test result
// 5) When no RF is available, report type is "Invalid". In this case, pages are de-populated
// Data is in Json format, and report_type field tells us which data type it is.
// Server decided how often data is sent (unlike GET/ajax method that is also supported)
// The buildXXX functions are exactly the same as in GET/ajax method.
// 6) Magnetometer Calibration Status
//
function onMessage(evt) {

    // these elements are emptied when Invalid report type is sent
    var emptyElementIds = ["statsResult"];

    var i;
    var res; // Inner html content
    var elem = null; // the element that will receive html content
    var elem2 = null; // helper variable
    var message_data = JSON.parse(evt.data);

    if (message_data.report_type == "Invalid") {
        res = buildInvalidResult(message_data);
        // display a relevant (e.g. "no data available") message in scanResult field
        elem = document.getElementById("scanResult");

        // empty elements that make no sense in invalid report (e.g. no rf data)
        for (i = 0; i < emptyElementIds.length; i++) {
            elem2 = document.getElementById(emptyElementIds[i]);
            if (elem2 != null) {
                elem2.innerHTML = "";
            }
        }
    } else {
        if (message_data.report_type == "RfStats") {
            res = buildStatsResultTable(message_data);
            elem = document.getElementById("statsResult");

        } else if (message_data.report_type == "RfCurrent") {
            var res = buildScanResultTable(message_data);
            elem = document.getElementById("scanResult");

#ifdef V_TITAN_INSTALLATION_ASSISTANT_CONN_nrb200
        } else if (message_data.report_type == "Batt") {
            res = buildBatteryResult(message_data);
            elem = document.getElementById("battResult");

#endif
#ifdef V_ORIENTATION_y
        } else if (message_data.report_type == "MagCal") {
            res = buildMagCalStatus(message_data);
            elem = document.getElementById("magCalResult");
#endif
        } else if (message_data.report_type == "Mode2") {
            res = buildMode2Info(message_data);
            elem = document.getElementById("mode2Info");
        }
    }
    // set the inner HTML
    if (elem != null) {
        elem.innerHTML = res;
    }
    writeToScreen('<span style="color: blue;">RESPONSE: ' + evt.data+'</span>');
}

// This will initialize web socket when the content is loaded
var ws_listener = function(event) {
    console.log("document is ready");
    initWebSocket(document.URL);
};

document.addEventListener('DOMContentLoaded', ws_listener, false);

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

function initWebSocket() {

    // URL determined dynamically
    // @TODO - should be wss:// for HTTPs. Tested, works, need to decide how to make it configurable. Also need to buy a proper server certificate
    var wsUri = "ws://" + document.location.host + "/ws";

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
// 3) System Status data
// 4) Orientation data
//
function onMessage(evt) {

    var res; // Inner html content
    var elem = null; // the element that will receive html content
    var message_data = JSON.parse(evt.data);

    if (message_data.report_type == "Invalid") {
        res = buildInvalidResult(message_data);
    } else {
        if (message_data.report_type == "RfStats") {
            res = buildStatsResultTable(message_data);
            elem = document.getElementById("statsResult");
        } else if (message_data.report_type == "RfCurrent") {
            res = buildScanResultTable(message_data);
            elem = document.getElementById("scanResult");
        } else if (message_data.report_type == "SysStatus") {
            res = buildSystemStatusContent(message_data);
            elem = document.getElementById("systemStatusContent");
#ifdef V_ORIENTATION_y
        } else if (message_data.report_type == "Orientation") {
            res = buildOrientationResult(message_data);
            elem = document.getElementById("orientationResult");
#endif
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
    initWebSocket();
};

document.addEventListener('DOMContentLoaded', ws_listener, false);

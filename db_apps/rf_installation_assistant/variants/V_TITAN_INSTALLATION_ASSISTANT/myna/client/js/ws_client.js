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
// 3) Battery data
// 4) Throughput test result
// 5) When no RF is available, report type is "Invalid". In this case, pages are de-populated
// Data is in Json format, and report_type field tells us which data type it is.
// Server decided how often data is sent (unlike GET/ajax method that is also supported)
// The buildXXX functions are exactly the same as in GET/ajax method.
// 6) Orientation data
//
function onMessage(evt) {

    // these elements are disabled when Invalid report type is sent
    var disabledElementIds = ["ReRunButtonId","ShowAllCellButtonId","ResetStatsButtonId"];

    // these elements are emptied when Invalid report type is sent
    var emptyElementIds = ["ttResult","statsResult"];

    // determine the page we are on.
    // On advanced scan and advanced operations page, we display a lot less stuff
    // On advanced operations page, we display Save button
    var advanced_scan_page = (window.location.pathname.search("adv_scan.html") != -1);
    var advanced_oper_page = (window.location.pathname.search("advanced_operations.html") != -1);

    var i;
    var res; // Inner html content
    var elem = null; // the element that will receive html content
    var elem2 = null; // helper variable
    var message_data = JSON.parse(evt.data);

    if (message_data.report_type == "Invalid") {
        res = buildInvalidResult(message_data);
        // display a relevant (e.g. "no data available") message in scanResult field
        elem = document.getElementById("scanResult");

        // disable elements that need to be disabled
        for (i = 0; i < disabledElementIds.length; i++) {
            elem2 = document.getElementById(disabledElementIds[i]);
            if (elem2 != null) {
                elem2.disabled = true;
            }
        }

        // do this separately as all disabled elements are temporarily enabled in the else {} block
        // which may cause flicker
        var id_speed_test = document.getElementById("SpeedTestButtonId");
        if (id_speed_test != null) {
            id_speed_test.disabled = true;
        }

        // empty elements that make no sense in invalid report (e.g. no rf data)
        for (i = 0; i < emptyElementIds.length; i++) {
            elem2 = document.getElementById(emptyElementIds[i]);
            if (elem2 != null) {
                elem2.innerHTML = "";
            }
        }
    } else {
        // re-enable elements that may have been disabled if rf disappears and then appears again
        // start from 1 as we do not want to momentarily enable ReRun button element.
        for (i = 1; i < disabledElementIds.length; i++) {
            elem2 = document.getElementById(disabledElementIds[i]);
            if (elem2 != null) {
                elem2.disabled = false;
            }
        }

        if (message_data.report_type == "RfCurrent") {
            if (typeof RfScanProcessor !== "undefined" && typeof rfScanProcessor !== "undefined"
                && rfScanProcessor instanceof RfScanProcessor) {
                rfScanProcessor.processScanResult(message_data);
            }
            if (!advanced_oper_page) {
                var ret = buildScanResultTable(message_data, !advanced_scan_page);
                res = ret[0];
                elem = document.getElementById("scanResult");
            }
        } else if (message_data.report_type == "TTest") {
            if (typeof processSpeedTestResult === "function") {
                processSpeedTestResult(message_data);
            }
        } else if (message_data.report_type == "Batt") {
            if (typeof BatteryHandler !== "undefined" && typeof batteryHandler !== "undefined"
                    && batteryHandler instanceof BatteryHandler) {
                batteryHandler.handleData(message_data);
            }
            res = buildBatteryResult(message_data);
            elem = document.getElementById("battResult");
        } else if (message_data.report_type == "AntennaStatus") {
            if (typeof OrientationGpsDataHandler !== "undefined" && typeof orientationGpsDataHandler !== "undefined"
                && orientationGpsDataHandler instanceof OrientationGpsDataHandler) {
                orientationGpsDataHandler.handleData(message_data);
            }
        } else if (message_data.report_type == "RegistrationStatus" && typeof getRegistrationStatusAndValidate === 'function') {
            getRegistrationStatusAndValidate(message_data);
        }
        else if (message_data.report_type == "OrientationData") {
            if (typeof OrientationDataHandler !== "undefined" && typeof orientationDataHandler !== "undefined"
                && orientationDataHandler instanceof OrientationDataHandler) {
                orientationDataHandler.handleData(message_data);
            }
        }
        else if (message_data.report_type == "UpgradingFwStatus") {
            if (typeof handleFwUpgradeStatusWsReport === "function") {
                handleFwUpgradeStatusWsReport(message_data);
            }
        }
        else if (message_data.report_type == "FactoryResetStatus") {
            if (typeof handleFactoryResetStatusWsReport === "function") {
                handleFactoryResetStatusWsReport(message_data);
            }
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

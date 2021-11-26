//----------------------------------------------------------------------
// !!!!!!!!!!!!! THIS FILE IS NOT USED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Delete it when we are 100% sure we are using Websocket and not Ajax for most pages
//----------------------------------------------------------------------
// This file contains javascript functions used by Titan installation assistant.
// Functions in this file handle Ajax (GET method-based) page updates
//
// Currently, they are not used (since WebSocket method is used for efficiency)
// If no technical reason exists to avoid using WebSocket, this file and other
// Ajax-related code can be deleted.

var updateScanResultCurrRf = function() {

    ajaxGetRequest("/rf_current/rf_curr_landing", true, function(scanResult) {
        //console.log(scanResult);
        var res = buildScanResultTable(scanResult);
        document.getElementById("scanResult").innerHTML = res;
    });
    setTimeout(updateScanResultCurrRf, 1000);
}

var updateScanResultStats = function() {

    ajaxGetRequest("/rf_stats/stats_landing", true, function(statsResult) {
        //console.log(statsResult);
        var res = buildStatsResultTable(statsResult)
        document.getElementById("statsResult").innerHTML = res;
    });
    setTimeout(updateScanResultStats, 10000);
}

var updateScanResultBatt = function() {

    ajaxGetRequest("/battery/batt_landing", true, function(battResult) {
        //console.log(battResult);
        var res = buildBatteryResult(battResult)
        document.getElementById("battResult").innerHTML = res;
    });
    setTimeout(updateScanResultBatt, 10000);
}

document.addEventListener('DOMContentLoaded', function() {
    console.log("document is ready");
    updateScanResultCurrRf();
    updateScanResultStats();
    updateScanResultBatt();
});

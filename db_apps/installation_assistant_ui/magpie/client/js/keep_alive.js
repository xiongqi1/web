/*
 * Check and display waiting-for-OWA indicator if necessary
 *
 * Copyright (C) 2019 Casa Systems
 */

const OWA_SYNC_UNKNOWN = 0;
const OWA_DISCONNECTED = 1;
const OWA_NO_SYNC = 2;
const OWA_SYNCHRONIZED = 3;

var owaSyncStatus = OWA_SYNC_UNKNOWN;
var keepAliveInterval;

/*
 * Send keep-alive message to get current OWA sync status
 * This function update internal status and display indicator if necessary
 * @param callOnFirstSyncFunc function to call if first keep-alive indicates sync status
 * @param callOnFirstNonSyncFunc function to call if first keep-alive does not indicate sync status
 * @param callOnEachFunc function to call on each keep alive request is responded
 */
function getKeepAlive(callOnFirstSyncFunc, callOnFirstNonSyncFunc, callOnEachFunc) {
    ajaxGetRequest("/keep_alive", true, function(data) {
        if(callOnEachFunc){
            callOnEachFunc(data);
        }

        if (data.owa_sync_status == "synchronised") {
            if (owaSyncStatus == OWA_SYNC_UNKNOWN) {
                owaSyncStatus = OWA_SYNCHRONIZED;
                if (typeof callOnFirstSyncFunc === "function") {
                    callOnFirstSyncFunc();
                }
            } else if (owaSyncStatus != OWA_SYNCHRONIZED) {
                // it was no sync, now sync is detected, forward to Data Entry
                if (typeof keepAliveInterval !== "undefined") {
                    clearInterval(keepAliveInterval);
                }
                owaSyncStatus = OWA_SYNCHRONIZED;
                // defer redirecting for users to see the message
                displayWaitingOwaPopupBox();
                setInterval(function(){ window.location = "/data_entry.html";}, 2000);
            }
        } else {
            if (owaSyncStatus == OWA_SYNC_UNKNOWN && typeof callOnFirstNonSyncFunc === "function") {
                callOnFirstNonSyncFunc();
            }
            owaSyncStatus = OWA_NO_SYNC;
            displayWaitingOwaPopupBox();
        }
    }, function() {
        // failure means OWA is disconnected
        owaSyncStatus = OWA_DISCONNECTED;
        displayWaitingOwaPopupBox();
    });
}

/*
 * Setup get keep-alive periodically
 */
function setupPeriodicKeepAlive(callOnEachFunc) {
    keepAliveInterval = setInterval(()=>{
        getKeepAlive(undefined, undefined, callOnEachFunc);
    }, 1000);
}

/*
 * Display waiting-for-OWA popup-box
 */
function displayWaitingOwaPopupBox() {
    var popupBox = document.getElementById("keepalive-popup-box");
    if (!popupBox) {
        return;
    }

    var popupTitle = document.getElementById("popup-title");
    var popupMessage = document.getElementById("popup-message");

    var title = "Waiting for OWA synchronization";
    var msg = "";
    switch (owaSyncStatus) {
        case OWA_DISCONNECTED:
            msg = "Waiting for connection with OWA";
            break;
        case OWA_NO_SYNC:
            msg = "Waiting for data synchronization with OWA";
            break;
        case OWA_SYNCHRONIZED:
            title = "Synchronized with OWA";
            msg = "Redirecting to Data Entry";
            break;
    }
    if (popupTitle.textContent != title) {
        popupTitle.textContent = title;
    }
    if (popupMessage.textContent != msg) {
        popupMessage.textContent = msg;
    }

    if (popupBox.getAttribute("style") == "display:none") {
        popupBox.setAttribute("style", "display:block");
    }
}


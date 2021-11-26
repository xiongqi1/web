/*
 * SW upgrade for Saturn/Neptune
 *
 * Copyright (C) 2020 Casa Systems
 */

// This is where the uploaded file will be stored in device
var targetPath = "/cache/upload/upgrade.star";

var upgradeBtn = buttonAction("flash", "Upgrade", "postFlashRequest",
                              "", {buttonStyle: "submitButton"});

var expectedFwVersion;
var upgradeResult;
var upgradeExtras;

function upgradeJsonpParser(resp) {
    upgradeResult = resp.result;
    upgradeExtras = resp.extras;
}

var fwUploader = fileUploader(
    "firmware",
    "Select firmware to upload",
    targetPath,
    [".star"],
    null,
    "Wrong file type. Files must have .star extension. Please try again.",
    "File upload failed. Please check the connection to the device and try again.",
    true,
    {onPosted: (resp) => {
         if (resp.message == "firmwareUploaded" && resp.result == "0") {
             upgradeBtn.setEnable(true);
             expectedFwVersion = resp.extras; // save for verification after reboot
         }
     },
     onLengthyOperation: (respObj) => {
         var timer
         var pollPeriod = 5000;
         var pollTimeout = 1000;
         var pollCnt = 0;
         const downPolls = 6; // number of polls to confirm device is down
         const upPolls = 3; // number of polls to confirm device is up
         set_progress_message(_("Waiting for device to reboot"));
         timer = pollUrl(respObj.ping_url, pollTimeout, pollPeriod,
                         null, onLostConnection);
         function onLostConnection() {
             pollCnt++;
             if (pollCnt < downPolls) {
                 return;
             }
             pollCnt = 0;
             clearInterval(timer);
             set_progress_message(_("Waiting for device to get back online"));
             timer = pollUrl(respObj.ping_url, pollTimeout, pollPeriod,
                             onReturnedOnline);
         }
         function onReturnedOnline() {
             pollCnt++;
             if (pollCnt < upPolls) {
                 return;
             }
             pollCnt = 0;
             clearInterval(timer);
             set_progress_message(_("Checking firmware version"));
             // Version check is based on sw.version RDB variable which
             // is set by identity service that starts much later than
             // turbontc service. So enough timeout value is needed for
             // waiting sw.version set.
             timer = pollUrl("/upload/upgrade", pollTimeout*30, pollPeriod,
                             onGotVersion, onFailedVersion);
         }
         function onGotVersion(resp) {
            if (upgradeResult == "0") {
                var versions = upgradeExtras.split(",");
                if (versions[0] == expectedFwVersion) {
                     clearInterval(timer);
                     blockUI_wait_confirm(_("Firmware upgrade succeeded"),
                                          _("CSok"),
                                          () => {window.location.href = respObj.ping_url;});
                 }
             }
         }
         function onFailedVersion() {
             set_progress_message("Failed to get firmware version. Retrying");
         }
     }
    }
);

var fwUpgrade = PageObj("fwUpgrade", "Upgrade firmware",
{
    members: [
        fwUploader,
        upgradeBtn,
    ],
});

var pageData : PageDef = {
    title: "Firmware upgrade",
    pageObjects: [fwUpgrade],
    menuPos: ["System", "FwUpgrade"],
    onReady: () => {
        upgradeBtn.setEnable(false);
    },
};

function postFlashRequest() {
    fwUploader.onUploaderFileChange("/" + relUrlOfPage, "", true);
    upgradeBtn.setEnable(false);
}

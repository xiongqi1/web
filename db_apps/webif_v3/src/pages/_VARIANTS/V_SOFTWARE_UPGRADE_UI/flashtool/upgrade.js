/*
 * Default SW upgrade page for Cassini platform
 *
 * Copyright (C) 2020 Casa Systems
 */

// This is where the uploaded file will be stored in device
var targetPath = "/usrdata/cache/upgrade.star";

var upgradeBtn = buttonAction("flash", "Upgrade", "postFlashRequest",
                              "", {buttonStyle: "submitButton"});

var resetConfig = toggleVariable("resetConf", "Reset to default config", "");

var expectedFwVersion;

var factoryResetRequired;
var defLanIpAddr;
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
        var completed = 0;

        // number of polls to confirm device is down
        const downPolls = 6;

        // number of polls to confirm device is up
        // give 60 seconds more if factory reset is reserved
        const upPolls = (factoryResetRequired != "0")? 15:3;

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
            // If factory reset is reserved, ping to default LAN IP address.
            // Ping to web_server_status to avoids CORS issue.
            var ping_url = (factoryResetRequired != "0")? (defLanIpAddr + respObj.ping_url):respObj.ping_url;
            timer = pollUrl(ping_url, pollTimeout, pollPeriod,
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
            var ping_url;
            if (factoryResetRequired != "0") {
                ping_url = defLanIpAddr + "/upload/upgrade";
            } else {
                ping_url = "/upload/upgrade";
            }
            ping_url = ping_url + "?callback=upgradeJsonpParser";
            timer = pollUrl(ping_url, pollTimeout*30, pollPeriod,
                            onGotVersion, onFailedVersion);
        }
        function onGotVersion(resp) {
            // return if already completed upgrading to prevent
            // pending events pop up windows.
            if (completed) {
                return;
            }
            if (upgradeResult == "0") {
                var versions = upgradeExtras.split(",");
                if (versions[0] == expectedFwVersion) {
                    var redirectUrl = (factoryResetRequired != "0")? (defLanIpAddr):"/";
                    clearInterval(timer);
                    completed = 1;
                    blockUI_wait_confirm(_("Firmware upgrade succeeded"),
                                        _("CSok"),
                                        () => {window.location.href = redirectUrl;});
                }
                // TODO: check inactive partition version for factory mode
            }
        }
        function onFailedVersion(resp) {
            set_progress_message("Trying to get firmware version. Please wait...");
        }
    }
    }
);

var fwUpgrade = PageObj("fwUpgrade", "Upgrade firmware",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
            arr.push("o.defLanIpAddr = getDefConfVal('/etc/cdcs/conf/default.conf', 'link.profile.0.address')");
            return arr;
        },
        set: function(arr) {
            arr.push("if o.resetConf == '1' then");
            arr.push("  executeCommand('environment -w FACTORY_RESET NEEDED')");
            arr.push("end");
            return arr;
        },
    },
#endif
    encodeRdb: function(obj) {
        factoryResetRequired = (obj.resetConf == "")? "0":obj.resetConf;
        defLanIpAddr = "http://" + obj.defLanIpAddr;
        return obj;
    },
    members: [
        fwUploader,
        resetConfig,
        upgradeBtn,
        hiddenVariable("defLanIpAddr", ""),
    ],
});

var pageData : PageDef = {
    title: "Firmware upgrade",
    pageObjects: [fwUpgrade],
    suppressAlertTxt: true,
    menuPos: ["System", "FwUpgrade"],
    onReady: () => {
        upgradeBtn.setEnable(false);
    },
};

function startUpgrading() {
    sendObjects();
    fwUploader.onUploaderFileChange("/" + relUrlOfPage, "", true);
    upgradeBtn.setEnable(false);
}
function postFlashRequest() {
    if (resetConfig.getVal() == "1") {
        if (window.location.protocol == "https:") {
            var newUrl = window.location.href.replace("https:", "http:");
            blockUI_alert_l("Redirecting to HTTP connection due to factory reset option.", function(){
                window.location.href = newUrl;
            });
            return;
        }
        blockUI_alert("Reset to default option is enabled. It may take 1-2 minutes longer to complete upgrading firmware", function() {
            startUpgrading();
        });
    } else {
        startUpgrading();
    }
}

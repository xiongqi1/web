var changePinReq = false;
var pinEnable = toggleVariable("pinEnable", "PIN protection", "");
var curSimPin = editablePasswordVariable("curSimPin", "Current PIN").setRequired(true)
                .setValidate(function (val) { return isValidPin(val); }, "pin warningMsg1");
var newSimPin = editablePasswordVariable("newSimPin", "New PIN").setRequired(true)
                .setValidate(function (val) { return isValidPin(val); }, "pin warningMsg1");
var confirmNewSimPin = editablePasswordVariable("confirmNewSimPin", "Confirm new PIN").setRequired(true)
                .setValidate(function (val) { return val === newSimPin.getVal(); }, "pin warningMsg2");
var simPuk = editablePasswordVariable("simPuk", "Current PUK")
                .setValidate(function (val) { return isValidPin(val); }, "puk warningMsg1");

function isValidPin(val) {
    const len = val.length;
    if (len < 4 || len > 8)
        return false;
    for (let i = 0; i < len; i++)
    {
        if (val.charAt(i) < '0' || val.charAt(i) > '9')
            return false;
    }
    return true;
}

function onClickChangePin() {
    if (pinEnable.getVal() != "1") {
        blockUI_alert_l("Please enable PIN protection first!");
        return;
    }
    changePinReq = true;
    setDivVisible(true, "newSimPin");
    setDivVisible(true, "confirmNewSimPin");
}

function checkSIMStatus() {
    switch (SimSecurityMgmt.obj.simStatus) {
    case "PIN locked":
        ["pinEnable", "changePin", "newSimPin", "confirmNewSimPin", "simPuk", "pukRetries"]
            .forEach(function(div) { setDivVisible(false, div); });
        break;
    case "PUK locked":
        ["pinEnable", "changePin", "curSimPin", "pinRetries"]
            .forEach(function(div) { setDivVisible(false, div); });
        break;
    case "SIM OK":
        ["newSimPin", "confirmNewSimPin", "simPuk", "pukRetries"]
            .forEach(function(div) { setDivVisible(false, div); });
        if (pinEnable.getVal() != "1")
            setDivVisible(false, "changePin");
        break;
    default:
        ["pinEnable", "changePin", "curSimPin", "newSimPin", "confirmNewSimPin", "simPuk", "pinRetries", "pukRetries", "savePin"]
            .forEach(function(div) { setDivVisible(false, div); });
        break;
    }
}

var SimSecurityMgmt = PageObj("SimSecurityMgmt", "Sim security management",
{
    members: [
        staticTextVariable("simStatus", "Status").setRdb("wwan.0.sim.status.status"),
        staticTextVariable("pinRetries","PIN retries remaining").setRdb("wwan.0.sim.status.retries_remaining"),
        staticTextVariable("pukRetries","PUK retries remaining").setRdb("wwan.0.sim.status.retries_puk_remaining"),
        pinEnable,
        buttonAction("changePin", "Change PIN", "onClickChangePin"),
        curSimPin,
        simPuk,
        newSimPin,
        confirmNewSimPin,
        buttonAction("savePin", "Save", "onClickSaveSimSecurity"),
        hiddenVariable("pinProtection ", 'wwan.0.sim.status.pin_enabled')
    ],
    decodeRdb: function(obj) {
        if (obj.pinProtection == "Enabled") {
            obj.pinEnable = "1";
        } else {
            obj.pinEnable = "0";
        }
        if ((obj.simStatus.indexOf("SIM locked") != -1) || (obj.simStatus.indexOf("incorrect SIM") != -1) || (obj.simStatus.indexOf("SIM PIN") != -1)) {
            obj.simStatus = "PIN locked";
        } else if (obj.simStatus.indexOf("PUK") != -1) {
            obj.simStatus = "PUK locked";
        }
        return obj;
    }
});

function onClickSaveSimSecurity() {
    let cmd = "";
    let params = {};
    let successMsg = "";
    switch (SimSecurityMgmt.obj.simStatus) {
    case "PIN locked":
        cmd = "verifypin";
        params = {"pin":curSimPin.getVal()};
        successMsg = "SIM unlocked successfully.";
        break;
    case "PUK locked":
        cmd = "verifypuk";
        params = {"puk":simPuk.getVal(), "newpin":newSimPin.getVal()};
        successMsg = "SIM unlocked successfully.";
        break;
    case "SIM OK":
        if (pinEnable.getVal() == "1") {
            if (changePinReq) {
                if (newSimPin.getVal() != confirmNewSimPin.getVal()) {
                    blockUI_alert("New PINs do not match!", ()=>[]);
                    return;
                }
                cmd = "changepin";
                params = {"pin":curSimPin.getVal(), "newpin":newSimPin.getVal()};
                successMsg = "PIN changed successfully.";
            } else {
                cmd = "enable";
                params = {"pin":curSimPin.getVal()};
                successMsg = "PIN protection enabled successfully.";
            }
        } else {
            cmd = "disable";
            params = {"pin":curSimPin.getVal()};
            successMsg = "PIN protection disabled successfully.";
        }
        break;
    }
    sendRdbCommand("wwan.0.sim", cmd, params, 30, csrfToken,
        function(param) {
            sendRdbCommand("wwan.0.sim", "check", {}, 30, csrfToken,
                function(param) {
                    blockUI_alert(successMsg, function() {
                        requestSinglePageObject(SimSecurityMgmt);
                        location.reload();
                    });
                }
            );
        }
    );
}

var pageData : PageDef = {
    title: "Sim security",
    menuPos: ["Networking", "WirelessWAN", "SimSecurity"],
    pageObjects: [SimSecurityMgmt],
    onReady: function () {
        checkSIMStatus();
    }
};

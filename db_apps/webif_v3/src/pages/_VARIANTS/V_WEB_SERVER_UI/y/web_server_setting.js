//-------------------------------------------------------------------------------
// Web server setting
//-------------------------------------------------------------------------------
var httpsEn;
var httpPort;
var httpsPort;
var httpsPortMenu = editablePortNumber("httpsPort", "HTTPS server port")
                    .setRdb("service.webserver.https_port")
                    .setIsVisible(function() {
                        return toBool(webServerSetting.obj.httpsEn == "1");})                    ;
var httpPortMenu = editablePortNumber("httpPort", "HTTP server port")
                    .setRdb("service.webserver.http_port");
function onClickHttpsEn(toggleName, v) {
    setToggle(toggleName, v);
    httpsPortMenu.setVisible(v);
}

var webServerSetting = PageObj("webServerSetting", "web server setting",
{
    members: [
        toggleVariable("httpsEn", "HTTPS", "onClickHttpsEn").setRdb("service.webserver.https_enabled"),
        httpsPortMenu,
        httpPortMenu,
        buttonAction("webServerSettingSaveButton", "Save",
                    "onClickSaveBtnWebServerSetting();", "",
                    {buttonStyle: "submitButton"})
        .setIsVisible(function() {return true;}),
        hiddenVariable("trigger", "service.webserver.trigger")
    ],
    decodeRdb: function(obj) {
        httpsEn = obj.httpsEn;
        httpPort = obj.httpPort;
        httpsPort = obj.httpsPort;
        obj.trigger = 1;
        return obj;
    }
});

function onClickSaveBtnWebServerSetting(){
    var hostname = window.location.hostname;
    var obj = webServerSetting.packageObj();
    // Check if certificate exists
    if (obj.httpsEn == "1" && certiInfoValid == 0) {
        blockUI_alert_l("Valid web server certificate does not exist!", function() {
            location.reload();
        });
        return;
    }
    // Check if need redirection
    var redirect = false;
    if ((httpsEn != obj.httpsEn) ||
        (httpsEn == "1" && httpsPort != obj.httpsPort) ||
        (httpsEn == "0" && httpPort != obj.httpPort)) {
        redirect = true;
    }
    pageData.suppressAlertTxt = false;
    // if no changes then do not trigger webserver template
    if (!redirect) {
        delete webServerSetting.obj.trigger;
    }
    sendSingleObject(webServerSetting);

    // Redirect to index page on new server protocol & port if changed
    if (redirect) {
        var newAddr;
        console.log("redirect to index.html")
        if (obj.httpsEn == "1") {
            newAddr = "https://" + hostname + ":" + obj.httpsPort + "/index.html";
        } else {
            newAddr = "http://" + hostname + ":" + obj.httpPort + "/index.html";
        }
        blockUI_alert_l("Redirect to "+newAddr, function() {
            window.location.href = newAddr;
        });
    }
}

//-------------------------------------------------------------------------------
// Generate server certificate
//-------------------------------------------------------------------------------
var certiInfoValid = 0;

function updateCertiInfo(res) {
    genServerCertiBtn.setEnable(true);
    if (res.result != "0") {
        return;
    }
    if (res.server_certificate == null || res.server_certificate == "" || res.server_certificate == ",,,,,/,/") {
        certiInfoValid = 0;
        return;
    }
    var certProps = res.server_certificate.substring(0, res.server_certificate.indexOf("/")).split(",");
    genServerCerti.obj.country = certProps[0];
    genServerCerti.obj.state = certProps[1];
    genServerCerti.obj.city = certProps[2];
    genServerCerti.obj.org = certProps[3];
    genServerCerti.obj.email = certProps[5];
    var expDates = res.server_certificate.substring(res.server_certificate.indexOf("/")+1, res.server_certificate.length-1).split(",");
    genServerCerti.obj.serialNo = res.server_certificate_serial_no;
    genServerCerti.obj.notBefore = expDates[0] == ""? "N/A":expDates[0];
    genServerCerti.obj.notAfter = expDates[1] == ""? "N/A":expDates[1];
    genServerCerti.obj.status = _("certi generated");
    genServerCerti.populate();
    certiInfoValid = 1;
}

function requestServerCertiInfo() {
    getServerCertiInfo(updateCertiInfo);
}

function caError() {
    blockUI_alert(_("ca fail"), requestServerCertiInfo);
}

function certiGenFunc() {
    pageData.suppressAlertTxt = true;
    sendSingleObject(genServerCerti);
    generateServerCa(updateCertiInfo, caError);
}

function certiCancelFunc() {
    genServerCertiBtn.setEnable(true);
}

function genServerCertiFunc() {
    var obj = genServerCerti.packageObj();
    // validity check
    if (obj.country == "" || obj.state == "" || obj.city == "" ||
        obj.org == "" || obj.email == "") {
        validate_alert("", _("empty field"));
        return;
    }
    var msg = _("ca delete confirm");
    genServerCertiBtn.setEnable(false);
    if (certiInfoValid) {
        blockUI_confirm(msg, certiGenFunc, certiCancelFunc);
    } else {
        certiGenFunc();
    }
}

#ifdef COMPILE_WEBUI
var customLuaGenServerCerti = {
  lockRdb : false,
  helpers: [
    "function isValidCountryName(val)",
    " if val and #val >= 1 and #val <= 2 "
        + "and val:match('[A-Z][A-Z]') then",
    "  return true",
    " end",
    " return false",
    "end"
  ]
};
#endif

var keySizeSel = selectVariable("keySize", "Server key size", function(o){
      return [["2048","2048"], ["4096","4096"],]})
      .setRdb("service.webserver.https_keysize");
var genServerCertiBtn = buttonAction("genServerCertiBtn", "Generate",
                                     "genServerCertiFunc", "Server certificate");
var countryVar = editableTextVariable("country", "Country")
                    .setMaxLength(2)
                    .setMinLength(2)
                    .setValidate(
                      function (val) { if (val.length < 2) return false; return true;},
                                    "Country name is too short",
                      function (field) {return nameFilter(field);},
                                    "isValidCountryName(v)")
                    .setRdb("service.webserver.https_country", true);
var stateVar = editableTextVariable("state", "State")
                    .setMaxLength(64)
                    .setRdb("service.webserver.https_state", true);
var cityVar = editableTextVariable("city", "City")
                    .setMaxLength(64)
                    .setRdb("service.webserver.https_city", true);
var orgVar = editableTextVariable("org", "Organization")
                    .setMaxLength(64)
                    .setRdb("service.webserver.https_org", true);
var emailVar = editableTextVariable("email", "Email")
                    .setMaxLength(64)
                    .setRdb("service.webserver.https_email", true);
var genServerCerti = PageObj("genServerCerti", "gen server certi",
{
#ifdef COMPILE_WEBUI
    customLua: customLuaGenServerCerti,
#endif
    members: [
        staticTextVariable("serialNo", "Certificate serial number"),
        staticTextVariable("notBefore", "Not before"),
        staticTextVariable("notAfter", "Not after"),
        keySizeSel,
        countryVar,
        stateVar,
        cityVar,
        orgVar,
        emailVar,
        genServerCertiBtn,
    ],
});
//-------------------------------------------------------------------------------


var pageData : PageDef = {
    title: "Web server setting",
    menuPos: ["System", "SystemConfig", "WebServerSetting"],
    pageObjects: [webServerSetting, genServerCerti],
    onReady: function (){
        onClickHttpsEn("httpsEn", httpsEn);
        requestServerCertiInfo();
        genServerCertiBtn.setEnable(false);
    }
}

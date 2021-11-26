function onClickRemoteHttpEnable(toggleName, v) {
    setToggle(toggleName, v);
    setDivVisible(v, "httpPort");
}

function onClickRemoteHttpsEnable(toggleName, v) {
    setToggle(toggleName, v);
    setDivVisible(v, "httpsPort");
}

function onClickRemoteSshEnable(toggleName, v) {
    setToggle(toggleName, v);
    setDivVisible(v, "sshPort");
}


var remoteHttpEnable = toggleVariable("remoteHttpEnable", "HTTP enable", "onClickRemoteHttpEnable")
                        .setRdb("admin.remote.webenable");
var remoteHttpsEnable = toggleVariable("remoteHttpsEnable", "HTTPS enable", "onClickRemoteHttpsEnable")
                        .setRdb("admin.remote_https.enable");
var remoteSshEnable = toggleVariable("remoteSshEnable", "SSH enable", "onClickRemoteSshEnable")
                        .setRdb("admin.remote.sshd_enable");
var localHttpsEnable = toggleVariable("localHttpsEnable", "HTTPS enable").setRdb("admin.local.enable_https");


var remoteAccessControl = PageObj("remoteAccessControl", "Remote Access Control",
{
    members: [
        remoteHttpEnable,
        editablePortNumber("httpPort", "HTTP port")
            .setRdb("admin.remote.port")
            .setIsVisible(function() {
                return toBool(remoteHttpEnable.getVal());}),
        remoteHttpsEnable,
        editablePortNumber("httpsPort", "HTTPS port")
            .setRdb("admin.remote_https.port")
            .setIsVisible(function() {
                return toBool(remoteHttpsEnable.getVal());}),
        remoteSshEnable,
        editablePortNumber("sshPort", "SSH port")
            .setRdb("admin.remote.sshd_port")
            .setIsVisible(function() {
                return toBool(remoteSshEnable.getVal());}),
        toggleVariable("remotePingEnable", "Ping enable")
            .setRdb("admin.remote.pingenable")
    ],
});

var localAccessControl = PageObj("localAccessControl", "Local Access Control",
{
    members: [
        toggleVariable("localHttpEnable", "HTTP enable").setRdb("admin.local.enable_http"),
        localHttpsEnable,
        toggleVariable("localSshEnable", "SSH enable").setRdb("admin.local.ssh_enable")
    ],
});

#ifdef COMPILE_WEBUI
var customLuaCheckServerCerti = {
    lockRdb: false,
    get: function(arr) {
        arr.push("o = {}")
        arr.push("local key_file = luardb.get('service.webserver.https_key') or '/usr/local/cdcs//webserver/httpswebserver.key'")
        arr.push("local cert_file = luardb.get('service.webserver.https_cert') or '/usr/local/cdcs//webserver/httpswebserver.crt'")
        arr.push("o.isCertiValid = file_exists(key_file) and file_exists(cert_file)")
        arr.push("o.httpsServerEn = luardb.get('service.webserver.https_enabled')")
        arr.push("o.httpServerPort = luardb.get('service.webserver.http_port')")
        return arr
    },
};
#endif

var checkHttpsServer = PageObj("checkHttpsServer", "",
{
    readOnly: true,
#ifdef COMPILE_WEBUI
    customLua: customLuaCheckServerCerti,
#endif
    members: [
        hiddenVariable("isCertiValid", ""),
        hiddenVariable("httpsServerEn", ""),
        hiddenVariable("httpServerPort", "")
    ],
    invisible: true,
});

function onClickSave() {
    sendObjects();
    if (localHttpsEnable.getVal() == "1" || remoteHttpsEnable.getVal() == "1") {
        var newAddr = "http://" + window.location.hostname + ":" + checkHttpsServer.obj.httpServerPort + "/web_server_setting.html";
        if (!checkHttpsServer.obj.isCertiValid) {
            blockUI_alert(_("HTTPS access control is enabled but there is no valid server certificate that is essential to this service. Redirecting to HTTPS server setting page."), function() {
                window.location.href = newAddr;
            });
            return;
        }
        if (checkHttpsServer.obj.httpsServerEn == "0") {
            blockUI_alert(_("HTTPS access control is enabled but HTTPS server is disabled. Redirecting to HTTPS server setting page."), function() {
                window.location.href = newAddr;
            });
            return;
        }
    }
}

var pageData : PageDef = {
    title: "Access control",
    menuPos: ["System", "AccessControl"],
    pageObjects: [remoteAccessControl, localAccessControl, checkHttpsServer],
    onReady: function () {
        appendButtons({"save":"CSsave"});
        setButtonEvent('save', 'click', onClickSave);
    }
};

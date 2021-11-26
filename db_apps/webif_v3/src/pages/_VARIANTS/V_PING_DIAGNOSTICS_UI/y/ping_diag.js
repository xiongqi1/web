// Ping diagnostics page - depends on TR-069 Device.IP.Diagnostics.IPPing

var pingDiagRdbPrefix = "tr069.diagnostics.ipping.";

var pingDiagReady = true; // are we ready to receive request

function onPingDiagCancel()
{
    PingDiagCfg.obj["pingState"] = "Canceled";
    sendObjects();
}

function onPingDiagRequest()
{
    clear_alert();
    // clear all elements from result
    PingDiagResult.clear();
    PingDiagCfg.obj["pingTriggerSrc"] = "webui";
    PingDiagCfg.obj["pingState"] = "Requested";
    blockUI_cancel("Ping in progress. Please wait ...", onPingDiagCancel);
    sendObjects(
        // start ping result polling only when the objects are sucessfully sent
        function() {
            PingDiagResult.pollPeriod = 1000;
            startPagePolls();
        }
    );
}

var PingDiagCfg = PageObj("PingDiagCfg", "Ping diagnostics configuration",
{
#ifdef COMPILE_WEBUI
    customLua: {
        get: function(arr) {
            arr.push("o.ifaces = getNetIfaces(true)");
            arr.push("local pidfile = '/var/run/tr143_ping.pid'");
            arr.push("local program = '/usr/lib/tr-069/scripts/tr143_diagd.lua'");
            arr.push("if not isDaemonRunning(pidfile) then startDaemon(program, pidfile, '-p') end");
            return arr;
        },
        validate: function(arr) {
            arr.push("v=o.pingInterface if not isValid.Enum(v, getNetIfaces(true)) and v ~= '' then return false,'oops! '..'pingInterface' end");
            return arr;
        },
    },
#endif
    rdbPrefix: pingDiagRdbPrefix,
    members: [
        editableHostnameOrIpAddress("pingHost", "Host").setRdb("host"),
        editableBoundedInteger("pingRepeats", "Number of repetitions", 1, 4294967295, "Number of repetitions must have a value between 1 and 4294967295. Please try again", {helperText: "(1-4294967295)"}).setRdb("reps"),
        editableBoundedInteger("pingTimeout", "Timeout", 1, 4294967295, "Timeout must have a value between 1 and 4294967295. Please try again", {helperText: "(1-4294967295) ms"}).setRdb("timeout"),
        editableBoundedInteger("pingBlockSize", "Data block size", 1, 65535, "Data block size must have a value between 1 and 65535. Please try again", {helperText: "(1-65535)"}).setRdb("blksz"),
        editableBoundedInteger("pingDscp", "DSCP", 0, 63, "DSCP must have a value between 0 and 63. Please try again", {helperText: "(0-63)"}).setRdb("dscp"),
        selectVariable("pingInterface", "Interface", function(o) {
            var ifaces = [ "" ];
            if (isDefined(o) && Array.isArray(o.ifaces)) {
                o.ifaces.forEach(function(iface) {
                    ifaces.push(iface);
                })
            }
            return ifaces;
        }, null, true).setRdb("interface_iface"),
        selectVariable("pingProtocol", "Protocol version", function(o) {
            return [ "Any", "IPv4", "IPv6" ]; }).setRdb("proto"),
        hiddenVariable("pingTriggerSrc", "trigger_src", true),
        hiddenVariable("pingState", "state", true), // this must be the last
        buttonAction("pingRequest", "Request", "onPingDiagRequest"),
    ],
});

var PingDiagResult = PageObj("PingDiagResult", "Ping diagnostics result",
{
    rdbPrefix: pingDiagRdbPrefix,
    readOnly: true,
    members: [
        staticTextVariable("diagState", "Diagnostics state").setRdb("state"),
        staticTextVariable("successCount", "Success count").setRdb("success_count"),
        staticTextVariable("failureCount", "Failure count").setRdb("failure_count"),
        staticTextVariable("minRespTime", "Minimum response time").setRdb("min_rsp_time"),
        staticTextVariable("maxRespTime", "Maximum response time").setRdb("max_rsp_time"),
        staticTextVariable("avgRespTime", "Average response time").setRdb("avg_rsp_time"),
    ],
    decodeRdb: function(obj) {
        if (obj["minRespTime"]) {
            obj["minRespTime"] = obj["minRespTime"] + _(" ms");
        }
        if (obj["maxRespTime"]) {
            obj["maxRespTime"] = obj["maxRespTime"] + _(" ms");
        }
        if (obj["avgRespTime"]) {
            obj["avgRespTime"] = obj["avgRespTime"] + _(" ms");
        }
        if (!obj["diagState"]) {
            obj["diagState"] = "None";
        }
        var state = obj["diagState"];
        var newReady = (state != "Canceled" && state != "Requested");
        if (!pingDiagReady && newReady) {
            // diag result is available, success or failure
            stopPagePolls(); // no need to poll any more
        }

        pingDiagReady = newReady;

        if (pingDiagReady) {
            $.unblockUI();
        }
        return obj;
    },
});

var pageData : PageDef = {
    title: "Ping diagnostics",
    menuPos: ["System", "PingDiagnostics"],
    pageObjects: [PingDiagCfg, PingDiagResult],
    suppressAlertTxt: true,
};

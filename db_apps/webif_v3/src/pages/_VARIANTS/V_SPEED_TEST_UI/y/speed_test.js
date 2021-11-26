var speedTestRdbPrefix = "service.speedtest.";

var SpeedTest = PageObj("SpeedTest", "Network speed test status",
{
    rdbPrefix: speedTestRdbPrefix,
    pollPeriod: 1000,
    encodeRdb: function (obj) {
        if (obj.trig == "1") {
            obj.status = "";
        } else {
            obj.status = "stopped";
        }
        return obj;
    },
    decodeRdb: function (obj) {
        obj.latency += (obj.latency != "")? " ms":"";
        obj.dlBw += (obj.dlBw != "")? " Mbps":"";
        obj.ulBw += (obj.ulBw != "")? " Mbps":"";
        if (obj.status == "inprogress") {
            $("#spinner").show();
            setEnable("startButton", false);
            setEnable("stopButton", true);
        } else {
            $("#spinner").hide();
            setEnable("startButton", true);
            setEnable("stopButton", false);
        }
        obj.status = obj.status.replace("inprogress","in progress").replace("notstarted","not started");
        obj.dlRl = (obj.dlRl != "NotRated")? obj.dlRl:"";
        obj.ulRl = (obj.ulRl != "NotRated")? obj.ulRl:"";
        return obj;
    },
    members: [
        staticTextVariable("status", "Status")
            .setRdb("current_state"),
        staticTextVariable("resultId", "Result ID")
            .setReadOnly(true).setRdb("result_id"),
        staticTextVariable("country", "Country")
            .setReadOnly(true).setRdb("country"),
        staticTextVariable("serverName", "Server name")
            .setReadOnly(true).setRdb("name"),
        staticTextVariable("serverLocation", "Server location")
            .setReadOnly(true).setRdb("location"),
        staticTextVariable("isp", "Service provider")
            .setReadOnly(true).setRdb("isp"),
        staticTextVariable("clientIp", "Client IP address")
            .setReadOnly(true).setRdb("client_ip"),
        staticTextVariableWait("latency", "Ping latency")
            .setReadOnly(true).setRdb("ping_latency"),
        staticTextVariable("dlBw", "Download")
            .setReadOnly(true).setRdb("download_bandwidthMbps"),
        staticTextVariable("ulBw", "Upload")
            .setReadOnly(true).setRdb("upload_bandwidthMbps"),
        hiddenVariable("trig", "trigger"),
    ],
});

function onClickStartBtnSpeedTest(){
    SpeedTest.obj.trig = "1";
    sendObjects();
}

function onClickStopBtnSpeedTest(){
    SpeedTest.obj.trig = "0";
    sendObjects();
}

var pageData : PageDef = {
    title: "Network speed test",
    menuPos: ["Networking", "SpeedTest"],
    pageObjects: [SpeedTest],
    suppressAlertTxt: true,
    onReady: function () {
        appendButtons({"start":"start", "stop":"stop"});
        setButtonEvent('start', 'click', onClickStartBtnSpeedTest);
        setButtonEvent('stop', 'click', onClickStopBtnSpeedTest);
        $("#spinner").hide();
        setEnable("startButton", true);
        setEnable("stopButton", false);
    }
};

// ------------------------------------------------------------------------------------------------
// Network speed test
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var downloadPrefix = "service.ttest.ftp.0.";
var uploadPrefix = "service.ttest.ftp.1.";

var customLuaSpeedTest= {
  lockRdb : false,
  set: function() {
    var luaScript = `
    local luardb = require("luardb")
    luardb.set("service.ttest.ftp.0.repeats", "1")
    luardb.set("service.ttest.ftp.1.repeats", "1")
    luardb.set("service.ttest.ftp.0.is_dload", "1")
    luardb.set("service.ttest.ftp.1.is_dload", "0")
    luardb.set("service.ttest.ftp.0.speed_expected", "100")
    luardb.set("service.ttest.ftp.1.speed_expected", "100")
    luardb.set("service.ttest.ftp.0.res.0.result", "")
    luardb.set("service.ttest.ftp.1.res.0.result", "")
    luardb.set("service.ttest.ftp.0.res.0.status", "")
    luardb.set("service.ttest.ftp.1.res.0.status", "")
    luardb.set("service.ttest.ftp.command", "start")
    return 0
`;
    return luaScript.split("\n");
  }
};

// return an array of page object the need to be saved
function getSingleObjectToSend(objName) {
  var objs=[];
  var obj = objName.packageObj();
  if (obj) objs[objs.length] = obj;
  return objs;
}

function onUpdateTestResult() {
  SpeedTest.obj.download_result = "";
  SpeedTest.obj.download_status = "";
  SpeedTest.obj.upload_result = "";
  SpeedTest.obj.upload_status = "";
  pageData.suppressAlertTxt = true;
  $("button").prop("disabled", true);
  blockUI_wait(_("GUI pleaseWait"));
  sendTheseObjects(getSingleObjectToSend(SpeedTest), null, null);
  SpeedTest.pollPeriod = 2000;
  startPagePolls();     // start polling
  var numPolls = 30;    // 60 secs
  var intv = setInterval(function() {

    function stopWithError(err? : string) {
      clearInterval(intv);
      $.unblockUI();
      pageData.suppressAlertTxt = false;
      $("button").prop("disabled", false);
      if (isDefined(err)) {
        validate_alert( "", _(err));
      }
      SpeedTest.pollPeriod = 0;
      startPagePolls(); // stop polling
    }

    if ((isDefined(SpeedTest.obj.download_result) && SpeedTest.obj.download_result != "" ) &&
        (isDefined(SpeedTest.obj.download_status) && SpeedTest.obj.download_status == "completed" ) &&
        (isDefined(SpeedTest.obj.upload_result) && SpeedTest.obj.upload_result != "" ) &&
        (isDefined(SpeedTest.obj.upload_status) && SpeedTest.obj.upload_status == "completed" )) {
      if (SpeedTest.obj.download_result == "fail" || SpeedTest.obj.upload_result == "fail") {
        return stopWithError("speed test failed");
      }
      stopWithError();
      setTimeout("window.location.href='/speed_test_result.html'", 100);
    }
    numPolls--;
    if (numPolls <= 0) {
      return stopWithError("speed test timeout");
    }
  }, 2000); // 2 secs
}

var SpeedTest = PageObj("SpeedTest", "run speed test",
{
  labelText: "Run speed test",
  customLua: customLuaSpeedTest,
  members: [
    buttonAction("startButton", "run speed test", "onUpdateTestResult();"),
    hiddenVariable("download_result", "service.ttest.ftp.0.res.0.result"),
    hiddenVariable("download_status", "service.ttest.ftp.0.res.0.status"),
    hiddenVariable("upload_result", "service.ttest.ftp.1.res.0.result"),
    hiddenVariable("upload_status", "service.ttest.ftp.1.res.0.status")
  ]
});

var FtpDownloadCfg = PageObj("FtpDownloadCfg", "ftp download server",
{
  labelText: "FTP download server details",
  rdbPrefix: downloadPrefix,
  members: [
    editableURL("ftpDownServer", "server ip address").setRdb("server"),
    editableUsername("ftpDownServerUser", "pppoe user").setRdb("user"),
    editablePasswordVariable("ftpDownServerPass", "pppoe passwd").setRdb("password"),
    editableTextVariable("ftpDownRemoteFile", "remote file name").setRdb("remote_file_name"),
    editableTextVariable("ftpDownLocalFile", "local file name").setRdb("local_file_name"),
  ]
});

var FtpUploadCfg = PageObj("FtpUploadCfg", "ftp upload server",
{
  labelText: "FTP upload server details",
  rdbPrefix: uploadPrefix,
  members: [
    editableURL("ftpUpServer", "server ip address").setRdb("server"),
    editableUsername("ftpUpServerUser", "pppoe user").setRdb("user"),
    editablePasswordVariable("ftpUpServerPass", "pppoe passwd").setRdb("password"),
    editableTextVariable("ftpUpLocalFile", "local file name").setRdb("local_file_name"),
    editableTextVariable("ftpUpRemoteFile", "remote file name").setRdb("remote_file_name"),
  ]
});

function speedTestSendObjects() {
    sendTheseObjects(getSingleObjectToSend(FtpDownloadCfg), null, null);
    sendTheseObjects(getSingleObjectToSend(FtpUploadCfg), null, null);
}

// This RDB variable is set by the OWA depending on NIT connection status
var nit_connected = toBool("<%get_single_direct('nit.connected');%>");

var pageData : PageDef = {
  title: "Speed Test",

  // If NIT is connected then this whole page is disabled and the user is
  // prompted to open browser page at the connected NIT.
  disabled: nit_connected,
  alertHeading: "speed test disable",
  alertText: "use connected nit",
  menuPos: ["Services", "SpeedTest"],
  pageObjects: [SpeedTest, FtpDownloadCfg, FtpUploadCfg],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  suppressAlertTxt: false,
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', speedTestSendObjects);
  }
}

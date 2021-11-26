// ------------------------------------------------------------------------------------------------
// Display speed test result
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var downloadPrefix = "service.ttest.ftp.0.";
var uploadPrefix = "service.ttest.ftp.1.";

var DownloadTestResult = PageObj("DownloadTestResult", "download test result",
{
  labelText: "Download test result",
  rdbPrefix: downloadPrefix,
  readOnly: true,
  members: [
    staticTextVariable("downloadStartTime", "start time").setRdb("res.0.start_time"),
    staticTextVariable("downloadFileSize", "file size").setRdb("res.0.file_size_stored"),
    staticTextVariable("downloadDuration", "duration").setRdb("res.0.real_time_all"),
    staticTextVariable("downloadSpeed", "speed").setRdb("res.0.speed"),
    staticTextVariable("downloadResult", "result").setRdb("res.0.result")
  ],
  decodeRdb: function (obj) {
    obj.downloadSpeed = (obj.downloadSpeed/125000).toFixed(2) + " Mbps";
    return obj;
  }
});

var UploadTestResult = PageObj("UploadTestResult", "upload test result",
{
  labelText: "Upload test result",
  rdbPrefix: uploadPrefix,
  readOnly: true,
  members: [
    staticTextVariable("uploadStartTime", "start time").setRdb("res.0.start_time"),
    staticTextVariable("uploadFileSize", "file size").setRdb("res.0.file_size_stored"),
    staticTextVariable("uploadDuration", "duration").setRdb("res.0.real_time_all"),
    staticTextVariable("uploadSpeed", "speed").setRdb("res.0.speed"),
    staticTextVariable("uploadResult", "result").setRdb("res.0.result")
  ],
  decodeRdb: function (obj) {
    obj.uploadSpeed = (obj.uploadSpeed/125000).toFixed(2) + " Mbps";
    return obj;
  }
});

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
  pageObjects: [DownloadTestResult, UploadTestResult],
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(false, false, false));
  }
}

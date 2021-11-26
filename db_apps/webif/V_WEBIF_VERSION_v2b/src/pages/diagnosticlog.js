// ------------------------------------------------------------------------------------------------
// Diagnostic log user interface
// allow user to download diagnostic logs and setup periodic log collection
// Copyright (C) 2018 NetComm Wireless Limited.
// ------------------------------------------------------------------------------------------------
var diagnosticPrefix = "system.diagnostic.log.";

// clear all diagnostic logs after confirm with user
function onClickClearLog() {
  function clearlog_func() {
    $.get('/cgi-bin/cleardiaglog.cgi?csrfTokenGet=' + csrfToken, function(){location.reload()})
  }
  blockUI_confirm(_("confirm cleanLog"), clearlog_func);
}

// download diagnostic log
function onClickGetLog() {
  if (DiagnosticLogInfo.obj.count > 0 || DiagnosticLogInfo.obj.panicData > 0){
    location.href = "/cgi-bin/logfile.cgi?csrfTokenGet=" + csrfToken + "&action=downloadDiagnosticlog";
  } else {
    blockUI_alert(_("on_download_emptylog"));
  }
}

var DiagnosticLogCfg = PageObj("DiagnosticLogCfg", "DiagnosticLogConfig",
{
  customLua: {
    set: function(setArray) {
      return setArray.concat(["setRdb('"+diagnosticPrefix+"trigger', '1')"]);
    }
  },
  rdbPrefix: diagnosticPrefix,
  members: [
    toggleVariable("enable", "periodic_log_collection").setRdb("enable"),
    selectVariable("timer", "log_capture_interval", function(o){
      return [
        ["0",_("one-off")],
        ["360000" ,"1 " +_("hour")],      // backend value is in centisecond
        ["720000" ,"2 " +_("hours")],
        ["1440000","4 " +_("hours")],
        ["2160000","6 " +_("hours")],
        ["4320000","12 "+_("hours")]
      ];}).setRdb("timer"),
    editableBoundedInteger("maxPeriodicLog", "max_periodic_log", 100, 10000, "field100and10000",
       {helperText: _("100-10000")}).setRdb("max_periodic_log_size"),
    editableBoundedInteger("maxPanicData", "max_panic_data", 1, 1000, "field1and1000",
       {helperText: _("1-1000")}).setRdb("max_panic_data_size")
  ]
});

var DiagnosticLogInfo = PageObj("DiagnosticLogInfo", "captured_info",
{
  rdbPrefix: diagnosticPrefix,
  members: [
    staticTextVariable("count","periodic_logs_captured").setRdb("count"),
    staticTextVariable("panicData","captured_panic_data_size").setRdb("panic_data_size"),
    buttonAction("clearLog", "clear", "onClickClearLog", "clear_diag_logs"),
    buttonAction("getLog", "download", "onClickGetLog", "DownloadDiagnosticLog")
  ]
});

var pageData : PageDef = {
#ifndef V_DIAGNOSTIC_LOG
  onDevice : false,
#endif
  title: "Diagnostic Log",
  menuPos: ["System", "DIAGNOSTICLOG"],
  pageObjects: [DiagnosticLogCfg, DiagnosticLogInfo],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#htmlGoesHere").after(genSaveRefreshCancel(true, true, false));
    $("#saveButton").on('click', sendObjects);
    $("#refreshButton").on('click', function(){location.reload()});
  }
}

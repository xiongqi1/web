var logfile = PageObj("logfile", "", {
  members: [
    hiddenVariable("captureLevel", "service.syslog.option.capturelevel"),
  ]
});

function onClickLogDownloadBtn() {
  requestSinglePageObject(logfile);
  downloadUrl("log_file", "logmsg.txt");
}

function onClickLogClearBtn() {
  clearLogFiles(csrfToken);
  requestSinglePageObject(logfile);
}

var pageData : PageDef = {
    title: "System Logs",
    authenticatedOnly: true,
    menuPos: ["System", "Log", "LogFile"],
    pageObjects: [ logfile ],
    onReady: function (){
      appendLogDownloadClearButton();
      setButtonEvent('download', 'click', onClickLogDownloadBtn);
      setButtonEvent('clear', 'click', onClickLogClearBtn);
    },
    showImmediate: true
}

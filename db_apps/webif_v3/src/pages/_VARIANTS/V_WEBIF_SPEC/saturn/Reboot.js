var rebootButton = buttonAction("reboot", "Reboot", "onClickRebootButton", "", {buttonStyle: "submitButton"});

var rebootWarningHeader = "Clicking 'Reboot' will cause your device to power cycle.";
var rebootWarningParagraph = _("rebootWarning");

function onUpdateRebootResult() {
  // Saturn/Neptune booting time is roughly 60 seconds at this time.
  // Additional time for power down & rebooting is 150 seconds.
  var estRebootingSecs = parseInt(TrigReboot.obj.lastBootDuration) + 150;
  pageData.suppressAlertTxt = true;
  rebootButton.setEnable(false);
  sendSingleObject(TrigReboot);
  blockUI_RebootWaiting();
  waitUntilRebootComplete(estRebootingSecs);
}

function onClickRebootButton() {
  blockUI_confirm(_("rebootConfirm"), function(obj){
      onUpdateRebootResult();
      });
}

var TrigReboot = PageObj("TrigReboot", "",
{
#ifdef COMPILE_WEBUI
  customLua: {
    set: function(arr) {
      arr.push('luardb.set("service.system.reset_reason", "Webui Request")');
      arr.push('luardb.set("service.system.reset", "1")');
      return arr;
    }
  },
#endif
  pageWarning: [rebootWarningHeader, rebootWarningParagraph],
  members: [
    rebootButton,
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ]
});

var pageData : PageDef = {
  title: "Reboot",
  overrideTitle : "ATTENTION!!!!",
  menuPos: ["System", "Reboot"],
  pageObjects: [TrigReboot],
  suppressAlertTxt: true,
  style: {
    headerCssClass: "h2-logfile-large",
    titleCssClass: "body-box-logfile title",
    bodyCssClass: "body-box reboot"
  },
  onReady: function (){
    appendButtons({});
  }
}

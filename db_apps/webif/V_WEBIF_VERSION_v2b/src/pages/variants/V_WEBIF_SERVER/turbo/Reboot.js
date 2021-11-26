// return an array of page object the need to be saved
function getSingleObjectToSendInReboot(objName) {
  var objs=[];
  var obj = objName.packageObj();
  if (obj) objs[objs.length] = obj;
  return objs;
}

function onUpdateRebootResult() {
  pageData.suppressAlertTxt = true;
  $("button").prop("disabled", true);
  $("#objouterwrapperTrigReboot").hide();
  $("#objouterwrapperWaitRebooting").show();
  sendTheseObjects(getSingleObjectToSendInReboot(TrigReboot), null, null);
  WaitRebooting.pollPeriod = 500;
  startPagePolls();     // start polling
  // wait rebooting for the period of last booting duration plus 20 seconds
  var counter = 0;
  var estTime;
  var obj = TrigReboot.packageObj();
  if (obj.lastBootDuration == "") {
    estTime = 60;
  } else {
    // Need to add power-down processing time
    estTime = parseInt(obj.lastBootDuration) + 50;
  }
  var numPolls = estTime*1000/WaitRebooting.pollPeriod;
  var intv = setInterval(function() {
    function stopPolling() {
      clearInterval(intv);
      $.unblockUI();
      $("button").prop("disabled", false);
      WaitRebooting.pollPeriod = 0;
      startPagePolls(); // stop polling
    }
    numPolls--;
    counter++;
    var percentage = Math.floor(50*counter/estTime);
    if (numPolls <= 0) {
      stopPolling();
      // "The reboot seems to be taking too long, you may need to
      // manually power cycle the device"
      $("#rebootMsg").text(_("setman warningMsg6"));
      $("#rebootCt").text("");
      $("#spinner").hide();
      return;
    } else {
      $("#rebootCt").text(percentage + " %");
    }
    var obj = WaitRebooting.packageObj();
    if (percentage > 50 && obj.turbontcStatus == "started") {
      stopPolling();
      // "Reboot is successful, now redirecting to the Status page..."
      $("#rebootMsg").text(_("setman warningMsg7"));
      $("#rebootCt").text("100 %");
      setTimeout("window.location='/index.html'", 5000);
      return;
    }
  }, 500); // 500 ms
}

function onClickRebootButton() {
  blockUI_confirm(_("rebootConfirm"), function(obj){
      onUpdateRebootResult();
      });
}

class rebootWarning extends ShownVariable {
  genHtml() {
    var hText = '<h2><span>' + _("rebootRequest") + '</span></h2>' +
                '<p>' + _("rebootTime") + '</p>';
    return hText;
  }
}

var TrigReboot = PageObj("TrigReboot", "",
{
  customLua: {
    lockRdb : false,
    set: function() {
      var luaScript = `
      local luardb = require("luardb")
      o = {}
      luardb.set("service.turbontc.status", "")
      luardb.set("service.system.reset", "1")
      return 0
`;
      return luaScript.split("\n");
    }
  },
  members: [
    new rebootWarning("rebootWarning", ""),
    buttonAction("reboot", "setman reboot", "onClickRebootButton"),
    hiddenVariable("lastBootDuration", "system.startup_duration"),
  ]
});

class rebootWaiting extends ShownVariable {
  genHtml() {
    var hText = '<div id="rebootMsg" align="center" style="font-size:16px;' +
                'font-weight:bold; padding-top:20px;">' + _("GUI rebooting") +
                '</div>' +
                '<div id="spinner" align="center" style="padding-top:10px;">' +
                '<img width="60px" height="60px" src="/img/spinner_250.gif">' +
                '<b id="rebootCt" style="position:relative;top:-25px; left:30px">0 %</b>' +
                '</div>'
    return hText;
  }
}

var WaitRebooting = PageObj("WaitRebooting", "",
{
  members: [
    new rebootWaiting("rebootWaiting", ""),
    hiddenVariable("turbontcStatus", "service.turbontc.status"),
  ]
});

var pageData : PageDef = {
  title: "Reboot",
  menuPos: ["System", "RESET"],
  pageObjects: [TrigReboot, WaitRebooting],
  suppressAlertTxt: true,
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(false, false, false));
    $("#objouterwrapperWaitRebooting").hide();
  }
}

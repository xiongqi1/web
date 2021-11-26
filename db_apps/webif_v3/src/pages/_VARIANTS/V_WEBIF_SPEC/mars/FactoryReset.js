var resetButton = buttonAction("reset", "Restore defaults", "onClickResetButton", "", {buttonStyle: "resetButton"});

// smart redirect logic
// if newLocalHttp & newLocalHttps are 0 that means keep current protocol; http or https
// new page redirected is determined by below logic
//	-----------------------------------------------------------
//	curr_page   default setting             redirect page
//	-----------------------------------------------------------
//	on http     default http 0, https 0     http
//	on http     default http 0, https 1     https *
//	on http     default http 1, https 0     http
//	on http     default http 1, https 1     http
//	-----------------------------------------------------------
//	on https    default http 0, https 0     https
//	on https    default http 0, https 1     https
//	on https    default http 1, https 0     http *
//	on https    default http 1, https 1     https
//	-----------------------------------------------------------
function redirectToHttps()
{
  var newLocalHttp = parseInt(FactoryReset.obj.defHttpEn);
  var newLocalHttps = parseInt(FactoryReset.obj.defHttpsEn);
  var currLocalHttps = (location.protocol == "https:")? 1:0;
  if (currLocalHttps == 0) {
    return (newLocalHttp == 0 && newLocalHttps == 1);
  } else {
    return !(newLocalHttp == 1 && newLocalHttps == 0);
  }
}

function factoryResetOnUpdateRebootResult() {
  // System booting time is roughly 60 seconds at this time.
  // Additional time for power down & rebooting is 200 seconds including modem profile
  // restoring time.
  var estRebootingSecs = parseInt(FactoryReset.obj.lastBootDuration) + 200;
  var defLanIpAddr = '';
  var redirectProtocol = 'http';
  if (FactoryReset.obj.defLanIpAddr != FactoryReset.obj.currLanIpAddr) {
    if (redirectToHttps()) {
      redirectProtocol = 'https'
    }
    defLanIpAddr = redirectProtocol + "://" + FactoryReset.obj.defLanIpAddr;
  }
  pageData.suppressAlertTxt = true;
  resetButton.setEnable(false);
  sendSingleObject(FactoryReset)
  blockUI_RebootWaiting();
  waitUntilRebootComplete(estRebootingSecs, defLanIpAddr);
}

function onClickResetButton() {
  blockUI_confirm(_("rebootConfirm"), function(obj){
      factoryResetOnUpdateRebootResult();
      });
}

var FactoryReset = PageObj("FactoryReset", "",
{
#ifdef COMPILE_WEBUI
  customLua: {
    lockRdb: false,
    get: function(arr) {
      arr.push("o.defLanIpAddr = getDefConfVal('/etc/cdcs/conf/default.conf', 'link.profile.0.address')");
      arr.push("o.defHttpEn = getDefConfVal('/etc/cdcs/conf/default.conf', 'admin.local.enable_http')");
      arr.push("o.defHttpsEn = getDefConfVal('/etc/cdcs/conf/default.conf', 'admin.local.enable_https')");
      return arr;
    },
    set: function(arr) {
      arr.push("executeCommand('rdb_set service.factoryreset.reason webui; rdb_set service.system.factory 1')");
      return arr;
    }
  },
#endif
  members: [
    resetButton,
    hiddenVariable("defLanIpAddr", "").setReadOnly(true),
    hiddenVariable("defHttpEn", "").setReadOnly(true),
    hiddenVariable("defHttpsEn", "").setReadOnly(true),
    hiddenVariable("currLanIpAddr", "link.profile.0.address").setReadOnly(true),
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ]
});

var pageData : PageDef = {
  title: "Restore Factory Defaults",
  menuPos: ["System", "FactoryReset"],
  pageObjects: [FactoryReset],
  suppressAlertTxt: true,
  onReady: function (){
    appendButtons({});
  }
}

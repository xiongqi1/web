var factoryResetBtn = buttonAction("factoryResetBtn", "Factory Reset", "onClickFactoryResetBtn", "", {buttonStyle: "resetButton"});
var carrierResetBtn = buttonAction("carrierResetBtn", "Carrier Reset", "onClickCarrierResetBtn", "", {buttonStyle: "resetButton"});
var installerResetBtn = buttonAction("installerResetBtn", "Installer Reset", "onClickInstallerResetBtn", "", {buttonStyle: "resetButton"});

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
function redirectToHttps(resetObj)
{
  var newLocalHttp = parseInt(resetObj.obj.defHttpEn);
  var newLocalHttps = parseInt(resetObj.obj.defHttpsEn);
  var newLocalHttpsWebServer = parseInt(resetObj.obj.defHttpsWebServerEn);
  var currLocalHttps = (location.protocol == "https:")? 1:0;
  if (currLocalHttps == 0) {
    return (newLocalHttp == 0 && newLocalHttps == 1);
  } else {
    return !(newLocalHttp == 1 && newLocalHttps == 0) && newLocalHttpsWebServer == 1;
  }
}

function resetOnUpdateRebootResult(resetObj) {
  // Saturn/Neptune booting time is roughly 60 seconds at this time.
  // Additional time for power down & rebooting is 150 seconds including modem profile
  // restoring time.
  var estRebootingSecs = parseInt(resetObj.obj.lastBootDuration) + 150;
  var defLanIpAddr = '';
  var redirectProtocol = 'http';
  if (redirectToHttps(resetObj)) {
    redirectProtocol = 'https'
  }
  defLanIpAddr = redirectProtocol + "://" + resetObj.obj.defLanIpAddr;
  var currUrl = window.location.href.replace("/FactoryReset.html", "");
  if (defLanIpAddr == currUrl) {
    defLanIpAddr = '';
  }
  pageData.suppressAlertTxt = true;
  factoryResetBtn.setEnable(false);
  carrierResetBtn.setEnable(false);
  sendSingleObject(resetObj);
  blockUI_RebootWaiting();
  waitUntilRebootComplete(estRebootingSecs, defLanIpAddr);
}

function onClickFactoryResetBtn() {
  blockUI_confirm(_("rebootConfirm"), function(){
    resetOnUpdateRebootResult(FactoryReset);
    });
}

#ifdef V_RUNTIME_CONFIG_y
function onClickCarrierResetBtn() {
  blockUI_confirm(_("rebootConfirm"), function(){
    resetOnUpdateRebootResult(CarrierReset);
    });
}
function onClickInstallerResetBtn() {
  blockUI_confirm(_("rebootConfirm"), function(){
    resetOnUpdateRebootResult(InstallerReset);
    });
}
#endif

var FactoryReset = PageObj("FactoryReset", "",
{
#ifdef COMPILE_WEBUI
  customLua: {
    lockRdb: false,
    get: function(arr) {
      arr.push("o.defLanIpAddr = getDefConfVal('/etc/cdcs/conf/default.conf', 'link.profile.0.address')");
      arr.push("o.defHttpEn = getDefConfVal('/etc/cdcs/conf/default.conf', 'admin.local.enable_http')");
      arr.push("o.defHttpsEn = getDefConfVal('/etc/cdcs/conf/default.conf', 'admin.local.enable_https')");
      arr.push("o.defHttpsWebServerEn = getDefConfVal('/etc/cdcs/conf/default.conf', 'service.webserver.https_enabled')");
      return arr;
    },
    set: arr => [...arr, `
      luardb.set('service.factoryreset.reason', 'webui')
      luardb.set('service.system.factory.level', 'factory')
      luardb.set('service.system.factory', '1')
    `]
  },
#endif
  members: [
    factoryResetBtn,
    hiddenVariable("defLanIpAddr", "").setReadOnly(true),
    hiddenVariable("defHttpEn", "").setReadOnly(true),
    hiddenVariable("defHttpsEn", "").setReadOnly(true),
    hiddenVariable("defHttpsWebServerEn", "").setReadOnly(true),
    hiddenVariable("currLanIpAddr", "link.profile.0.address").setReadOnly(true),
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ]
});

#ifdef V_RUNTIME_CONFIG_y
var CarrierReset = PageObj("CarrierReset", "",
{
#ifdef COMPILE_WEBUI
  customLua: {
    lockRdb: false,
    get: arr => [...arr, `
      o.defLanIpAddr, o.defHttpEn, o.defHttpsEn, o.defHttpsWebServerEn = getDefConfLanRdbs('carrier')
    `],
    set: arr => [...arr, `
      luardb.set('service.factoryreset.reason', 'webui')
      luardb.set('service.system.factory.level', 'carrier')
      luardb.set('service.system.factory', '1')
    `]
  },
#endif
  members: [
    carrierResetBtn,
    hiddenVariable("defLanIpAddr", "").setReadOnly(true),
    hiddenVariable("defHttpEn", "").setReadOnly(true),
    hiddenVariable("defHttpsEn", "").setReadOnly(true),
    hiddenVariable("defHttpsWebServerEn", "").setReadOnly(true),
    hiddenVariable("currLanIpAddr", "link.profile.0.address").setReadOnly(true),
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ]
});

var InstallerReset = PageObj("InstallerReset", "",
{
#ifdef COMPILE_WEBUI
  customLua: {
    lockRdb: false,
    get: arr => [...arr, `
      o.defLanIpAddr, o.defHttpEn, o.defHttpsEn, o.defHttpsWebServerEn = getDefConfLanRdbs('installer')
    `],
    set: arr => [...arr, `
      luardb.set('service.factoryreset.reason', 'webui')
      luardb.set('service.system.factory.level', 'installer')
      luardb.set('service.system.factory', '1')
    `]
  },
#endif
  members: [
    installerResetBtn,
    hiddenVariable("defLanIpAddr", "").setReadOnly(true),
    hiddenVariable("defHttpEn", "").setReadOnly(true),
    hiddenVariable("defHttpsEn", "").setReadOnly(true),
    hiddenVariable("defHttpsWebServerEn", "").setReadOnly(true),
    hiddenVariable("currLanIpAddr", "link.profile.0.address").setReadOnly(true),
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ]
});
#endif

var pageData : PageDef = {
  title: "Restore Factory Defaults",
  menuPos: ["System", "SystemConfig", "FactoryReset"],
#ifdef V_RUNTIME_CONFIG_y
  pageObjects: [FactoryReset, CarrierReset, InstallerReset],
#else
  pageObjects: [FactoryReset],
#endif
  suppressAlertTxt: true
}

var  showHide = (elName, show) => show ? $(elName).show() : $(elName).hide();

function showAuth(show) {
    showHide("#div_authType", show);
    showHide("#div_password", show);
    showHide("#div_password_helper", show);
}
var isAuthVisible = () => RIPCfg.obj.version === '2' && toBool(RIPCfg.obj.authEnable);

function onChangeVersion(_this) {
  RIPCfg.obj.version = _this.value;
  var show = _this.value === '2'; // This->value is "1" or "2"
  showHide("#div_authEnable", show);
  showAuth(isAuthVisible());
}

function onClickAuthEnable(id, val) {
  RIPCfg.obj.authEnable = val;
  $("#" + id).val(val);
  var show = toBool(val);
  showAuth(show);
}

// THe default misses the password helper
function onClickenable(toggleName, v) {
  RIPCfg.setVisible(v);
  showAuth(toBool(v) && isAuthVisible());
}

var RIPCfg = PageObj("RIPCfg", "ripConfiguration",
{
#ifdef V_NON_SECURE_WARNING_y
  setEnabled: function (en) { if (en) blockUI_alert(_("noEncryptionWarning")); },
#endif
  members: [
    objVisibilityVariable("enable", "RIP").setRdb("service.router.rip.enable"),
    selectVariable("version", "version", (obj) => [["1","1"],["2","2"]], "onChangeVersion")
      .setRdb("service.router.rip.version"),
    selectVariable("interface", "interface", (obj) => [["lan","LAN"],["wwan0","WWAN"],["lan,wwan0","both"]])
      .setRdb("service.router.rip.interface"),
    toggleVariable("authEnable", "authentication", "onClickAuthEnable")
      .setIsVisible(() => RIPCfg.obj.version === '2')
      .setRdb("service.router.rip.auth.enable"),
    selectVariable("authType", "authentication type", (obj) => [["md5","md5"],["text","password"]])
      .setIsVisible(isAuthVisible)
      .setRdb("service.router.rip.auth.type"),
    editableStrongPasswordVariable("password", "password")
      .setIsVisible(isAuthVisible)
      .setMaxLength(16)
      .setRdb("service.router.rip.auth.key_string")
  ]
});

var pageData : PageDef= {
#if defined V_NETWORKING_UI_none
  onDevice : false,
#endif
  title: "RIP",
  menuPos: ["Internet", "RIP"],
  pageObjects: [RIPCfg],
  alertSuccessTxt: "ripSubmitSuccess",
  onReady: function () {
      $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
      $("#saveButton").on('click', sendObjects);
      showAuth(isAuthVisible());
  }
}

disablePageIfPppoeEnabled();

var customLua = {
  helpers: [
        "function isValidServer(v)",
        " local options=luardb.get('service.ddns.serverlist')",
        " local list={}",
        " for word in string.gmatch(options, '([^,]+)') do",
        "   list[#list+1] = word",
        " end",
        " return isValid.Enum(v,list)",
        "end"
    ]
};

var selectServer = selectVariable("ddnsServers", "man ddns",
          function(obj){
            if (obj && obj.ddnsServersOptions)
              return obj.ddnsServersOptions.split(',');
            return [];
          })
      .setRdb("service.ddns.server")
selectServer.validateLua.push("isValidServer(v)");

var DynDnsCfg = PageObj("DynDnsCfg", "ddnsConfiguration",
{
  customLua: customLua,
#ifdef V_NON_SECURE_WARNING_y
  setEnabled: function (enable) { if (enable) blockUI_alert(_("noEncryptionWarning")); },
#endif
  members: [
    hiddenVariable("ddnsServersOptions", "service.ddns.serverlist"),
    objVisibilityVariable("DDnsEnable", "ddnsConfiguration").setRdb("service.ddns.enable"),
    selectServer,
    editableHostname("host", "mesh hostname").setRdb("service.ddns.hostname"),
    editableUsername("user", "user name").setRdb("service.ddns.user"),
    editablePasswordVariable("password", "password").setRdb("service.ddns.password")
      .setVerify("verify password")
  ]
});

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_DDNS_WEBUI_none
  onDevice : false,
#endif
  title: "DDNS",
  menuPos: ["Services", "DDNS"],
  pageObjects: [DynDnsCfg],
  alertSuccessTxt: "ddnsSubmitSuccess",
  onReady: function () {
      $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
      $("#saveButton").on('click', sendObjects);
  }
}

disablePageIfPppoeEnabled();

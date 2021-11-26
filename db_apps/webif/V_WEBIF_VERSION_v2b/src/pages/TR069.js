
function onClickPeriodicEnable(toggleName, v) {
	setToggle(toggleName, v);
	setVisible("#div_informPeriod", v);
}

function onClickRandomInformEnable(toggleName, v) {
	setToggle(toggleName, v);
	setVisible("#div_randomInformWindow", v);
}

var TR069Cfg = PageObj("TR069Cfg", "tr069 configuration",
  {
    members: [
      objVisibilityVariable("TR069CfgEnable", "enableTR069Service").setRdb("service.tr069.enable"),
      editableURL("acsURL", "ACS URL").setRdb("tr069.server.url"),
      editableTextVariable("acsUsername", "acsUsername")
        .setRdb("tr069.server.username")
        .setRequired(false)
        .setValidate(function (val) { return true;}, "", function (field) {return hostNameFilter(field);}, "isValid.Hostname(v)"),
      editablePasswordVariable("acsPassword", "acsPassword")
        .setRdb("tr069.server.password")
        .setRequired(false)
        .setValidate(function (val) { return true;}, ""),
      editableHostname("connectionRequestUsername", "connectionRequestUsername")
        .setRdb("tr069.request.username")
        .setRequired(false)
        .setValidate(function (val) { return true;}, "", function (field) {return hostNameFilter(field);}, "isValid.Hostname(v)"),
      editablePasswordVariable("connectionRequestPassword", "connectionRequestPassword")
        .setRdb("tr069.request.password")
        .setRequired(false)
        .setValidate(function (val) { return true;}, ""),
      toggleVariable("periodicEnable", "enableperiodicACSinforms", "onClickPeriodicEnable").setRdb("tr069.server.periodic.enable"),
      editableBoundedInteger("informPeriod", "informPeriod", 30, 2592000, "Msg120", {helperText: "(30-2592000) secs"})
        .setRdb("tr069.server.periodic.interval")
        .setIsVisible( function () {return toBool(TR069Cfg.obj.periodicEnable);})
        .setSmall(),
      toggleVariable("enableRandomInform", "enableRandomInform", "onClickRandomInformEnable").setRdb("tr069.server.random_inform.enable"),
      editableBoundedInteger("randomInformWindow", "randomInformWindow", 0, 3600, "field0and3600", {helperText: "(0-3600) secs"})
        .setRdb("tr069.server.random_inform.window")
        .setIsVisible( function () {return toBool(TR069Cfg.obj.enableRandomInform);})
        .setSmall()
    ]
  });

var TR069InformStatus = PageObj("tr069InformStatus","last inform status",
  {
    readOnly: true,
    pollPeriod: 2000,
    members: [
      staticTextVariable("startat","start at").setRdb("tr069.informStartAt"),
      staticTextVariable("endat", "end at").setRdb("tr069.informEndAt")
    ]
  });

var TR069DeviceInfo = PageObj("tr069DeviceInfo", "tr069 deviceInfo",
  {
    readOnly: true,
    members: [
      staticText("manufacturer", "tr069 manufacturer", "NetComm Wireless Limited"),
      staticTextVariable("manufacturerOui","tr069 manufacturerOUI").setRdb("systeminfo.oui"),
      staticTextVariable("modelname","tr069 modelname").setRdb("system.product.model"),
      staticTextVariable("description","tr069 description").setRdb("system.product.title"),
  #ifdef V_CUSTOM_FEATURE_PACK_fastmile
      staticText("productClass", "tr069 productclass", "ODU"),
  #else
      staticTextVariable("productClass","tr069 productclass").setRdb("system.product.class"),
  #endif
      staticTextVariable("serialNumber", "tr069 serialnumber")
  #ifdef V_PRODUCT_ntc_8000c
        .setRdb("uboot.sn")
  #elif defined V_CUSTOM_FEATURE_PACK_fastmile
        .setRdb("systeminfo.udid")
  #else
        .setRdb("systeminfo.serialnumber")
  #endif
    ]
  });

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_TR069_none
  onDevice : false,
#endif
  title: "TR-069",
  menuPos: ["Services", "TR"],
  pageObjects: [TR069Cfg, TR069InformStatus, TR069DeviceInfo],
  alertSuccessTxt: "tr069SubmitSuccess",
  onReady: function (){
    $("#objouterwrapperTR069Cfg").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

disablePageIfPppoeEnabled();

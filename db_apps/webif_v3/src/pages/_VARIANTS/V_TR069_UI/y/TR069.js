var origProfileNo;

function onClickPeriodicEnable(toggleName, v) {
    setToggle(toggleName, v);
    setDivVisible(v, "informPeriod");
}

function onClickRandomInformEnable(toggleName, v) {
    setToggle(toggleName, v);
    setDivVisible(v, "randomInformWindow");
}

var TR069Enable = objVisibilityVariable("TR069CfgEnable", "enableTR069Service")
                    .setRdb("service.tr069.enable");
var TR069Cfg = PageObj("TR069Cfg", "tr069 configuration",
  {
    members: [
      TR069Enable,
      editableURL("acsURL", "ACS URL").setRdb("tr069.server.url"),
      editableTextVariable("acsUsername", "acsUsername")
      .setRdb("tr069.server.username")
      .setRequired(false)
      .setValidate(function (val) { return true;}, "", function (field) {return hostNameFilter(field);}, "isValid.Hostname(v)"),
      editablePasswordVariable("acsPassword", "acsPassword")
        .setRdb("tr069.server.password")
        .setVerify("verifyACSpassword")
        .setRequired(false)
        .setValidate(function (val) { return true;}, ""),
      editableHostname("connectionRequestUsername", "connectionRequestUsername")
        .setRdb("tr069.request.username")
        .setRequired(false)
        .setValidate(function (val) { return true;}, ""),
      editablePasswordVariable("connectionRequestPassword", "connectionRequestPassword")
        .setRdb("tr069.request.password")
        .setVerify("verify connection request password")
        .setRequired(false)
        .setValidate(function (val) { return true;}, ""),
      editablePortNumber("connectionRequestPort", "connectionRequestPort")
        .setRdb("tr069.request.port"),
#ifdef V_TR069_MANAGEMENT_APN_y
      selectVariable("managementWwanProfileNo", "managementWwanProfileNo",
          () => {
              const NumProfiles = 6;
              const idxList: OptionsType[] = [["", "Default route"]];
              for (let i = 1; i <= NumProfiles; i++){
                  idxList.push([i, "Profile no." + i]);
              }
              return idxList;
          },
          undefined,
          true
      ).setRdb("tr069.management_apn.lp_idx"),
      hiddenVariable("managementApnTrigger", "tr069.management_apn.trigger", true),
#endif
      toggleVariable("periodicEnable", "enableperiodicACSinforms", "onClickPeriodicEnable")
        .setRdb("tr069.server.periodic.enable"),
      editableBoundedInteger("informPeriod", "informPeriod", 30, 2592000, "Msg120", {helperText: "(30-2592000) secs"})
        .setRdb("tr069.server.periodic.interval")
        .setIsVisible( function () {
          return toBool(TR069Enable.getVal()) && toBool(TR069Cfg.obj.periodicEnable);})
        .setInputSizeCssName("sml"),
      toggleVariable("enableRandomInform", "enableRandomInform", "onClickRandomInformEnable")
        .setRdb("tr069.server.random_inform.enable"),
      editableBoundedInteger("randomInformWindow", "randomInformWindow", 0, 3600, "field0and3600", {helperText: "(0-3600) secs"})
        .setRdb("tr069.server.random_inform.window")
        .setIsVisible( function () {
          return toBool(TR069Enable.getVal()) && toBool(TR069Cfg.obj.enableRandomInform);})
        .setInputSizeCssName("sml")
    ],
    encodeRdb: function(obj) {
#ifdef V_TR069_MANAGEMENT_APN_y
        if (obj.managementWwanProfileNo == origProfileNo) {
            delete obj.managementApnTrigger;
        } else {
            obj.managementApnTrigger = 1;
        }
#endif
        return obj;
    },
    decodeRdb: function(obj) {
#ifdef V_TR069_MANAGEMENT_APN_y
        origProfileNo = obj.managementWwanProfileNo;
#endif
        return obj;
    }
  });

var TR069InformStatus = PageObj("tr069InformStatus","last inform status",
  {
    readOnly: true,
    //pollPeriod: 2000,
    members: [
      staticTextVariable("startat","start at").setRdb("tr069.informStartAt"),
      staticTextVariable("endat", "end at").setRdb("tr069.informEndAt")
    ]
  });

var TR069DeviceInfo = PageObj("tr069DeviceInfo", "tr069 deviceInfo",
  {
    readOnly: true,
    members: [
      staticText("manufacturer", "tr069 manufacturer", "Casa Systems"),
      staticTextVariable("manufacturerOui","tr069 manufacturerOUI").setRdb("systeminfo.oui"),
      staticTextVariable("modelname","tr069 modelname").setRdb("system.product.model"),
      staticTextVariable("description","tr069 description").setRdb("system.product.title"),
      staticTextVariable("productClass","tr069 productclass").setRdb("system.product.class"),
      staticTextVariable("serialNumber", "tr069 serialnumber").setRdb("systeminfo.serialnumber")
    ]
  });

var pageData : PageDef = {
  title: "TR-069",
  menuPos: ["Services", "TR-069"],
  pageObjects: [TR069Cfg, TR069InformStatus, TR069DeviceInfo],
  alertSuccessTxt: "tr069SubmitSuccess",
  onReady: function (){
    appendButtons({"save":"CSsave"});
    setButtonEvent('save', 'click', sendObjects);
  }
}

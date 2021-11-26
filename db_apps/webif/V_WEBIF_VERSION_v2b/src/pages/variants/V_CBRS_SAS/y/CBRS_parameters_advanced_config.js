// ------------------------------------------------------------------------------------------------
// CBRS install parameters setup
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var sasPrefix = "sas.";

var CbrsRegStatusAdvConfig= PageObj("CbrsRegStatusAdvConfig", "cbrs reg status",
{
  labelText: "CBRS registration status",
  rdbPrefix: sasPrefix,
  pollPeriod: 1000,
  readOnly: true,
  members: [
    staticTextVariable("regStatus", "reg status").setRdb("registration.state"),
    staticTextVariable("regRespMsg", "reg resp msg")
        .setRdb("registration.response_message")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.regStatus != "Registered");}),
    staticTextVariable("regReqErrCode", "reg req err code")
        .setRdb("registration.request_error_code")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.regStatus != "Registered");}),
    staticTextVariable("regRespCode", "reg resp code")
        .setRdb("registration.response_code")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.regStatus != "Registered");}),
    staticTextVariable("regRespData", "reg resp data")
        .setRdb("registration.response_data")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.regStatus != "Registered");}),
    staticTextVariable("grantStatus", "grant status").setRdb("grant.0.state"),
    staticTextVariable("grantRespMsg", "grant resp msg")
        .setRdb("grant.0.response_message")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.grantStatus != "AUTHORIZED");}),
    staticTextVariable("grantRespReason", "grant resp reason")
        .setRdb("grant.0.reason")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.grantStatus != "AUTHORIZED");}),
    staticTextVariable("grantRespData", "grant resp data")
        .setRdb("grant.0.response_data")
        .setIsVisible( function () {return toBool(CbrsRegStatusAdvConfig.obj.grantStatus != "AUTHORIZED");}),
  ]
});

var customLuaCbrsParametersAdvConfig = {
  lockRdb : false,
  set: function() {
    var luaScript = `
    if luardb.get("service.sas_client.enabled_by_ia") ~= o.clientEnable then
      luardb.set("service.sas_client.enabled_by_ia", o.clientEnable)
    end
    luardb.set('sas.antenna.latitude', o.latitude)
    luardb.set('sas.antenna.longitude', o.longitude)
    luardb.set('sas.antenna.height', o.height)
    luardb.set('sas.antenna.azimuth', o.azimuth)
    luardb.set('sas.antenna.downtilt', o.downtilt)
    luardb.set('sas.config.cpiId', o.cpiId)
    luardb.set('sas.config.cpiName', o.cpiName)
    luardb.set('sas.config.cpiKeyFile', o.cpiKeyFile)
    luardb.set('cpi passcode', o.cpiPasscode)
    luardb.set('sas.config.callSign', o.callSign)
    luardb.set('sas.config.userId', o.userId)
    if luardb.get("sas.config.url") ~= o.sasServer then
      luardb.set("sas.config.url", o.sasServer)
      if o.clientEnable == "1" then
        os.execute("/etc/init.d/rc.d/sas_client restart")
      end
    end
    if o.regOption == "generate" then
      os.execute("/bin/sasCpiSignature.sh register")
    elseif o.regOption == "deregister" then
      luardb.set("sas.registration.cmd", "force_deregister")
    elseif o.regOption == "register" then
      luardb.set("sas.registration.cmd", "register")
    end
`;
    return luaScript.split("\n");
  }
};

var CbrsParametersAdvConfig = PageObj("CbrsParametersAdvConfig", "CBRS parameters",
{
  labelText: "CBRS parameters",
  customLua: customLuaCbrsParametersAdvConfig,
  members: [
    objVisibilityVariable("clientEnable", "sas client enable").setRdb("service.sas_client.enable"),
    editableURL("sasServer", "sas server address").setRdb("sas.config.url"),
    editableBoundedFloat("latitude", "antenna latitude", -90, 90, "warningforDDLati", {helperText: "(-90~90)"})
        .setRdb("sas.antenna.latitude")
        .setMaxLength(16),
    editableBoundedFloat("longitude", "antenna longitude", -180, 180, "warningforDDLong", {helperText: "(-180~180)"})
        .setRdb("sas.antenna.longitude")
        .setMaxLength(16),
    editableBoundedFloat("height", "antenna height", 0, 10000, "fieldzand10000", {helperText: "(0~10000)"})
        .setRdb("sas.antenna.height")
        .setMaxLength(16),
    editableBoundedFloat("azimuth", "antenna azimuth", 0, 359, "field0and359", {helperText: "(0~359)"})
        .setRdb("sas.antenna.azimuth")
        .setMaxLength(16),
    editableBoundedFloat("downtilt", "antenna downtilt", -90, 90, "fieldm90and90", {helperText: "(-90~90)"})
        .setRdb("sas.antenna.downtilt")
        .setMaxLength(16),
    editableTextVariable("cpiId", "cpi id")
        .setRdb("sas.config.cpiId")
        .setMaxLength(256),
    editableTextVariable("cpiName", "cpi name")
        .setRdb("sas.config.cpiName")
        .setMaxLength(256),
    editableTextVariable("cpiKeyFile", "cpi key file").setRdb("sas.config.cpiKeyFile"),
    editableTextVariable("cpiPasscode", "cpi passcode", {"isClearText": false, "encode": true, "required": false})
        .setMaxLength(256),
    editableTextVariable("callSign", "call sign")
        .setRdb("sas.config.callSign")
        .setMaxLength(12),
    editableTextVariable("userId", "user id")
        .setRdb("sas.config.userId")
        .setMaxLength(63),
    selectVariable("regOption", "Registration action", function(o){
      return [
        ["", "do nothing"],
        ["deregister", "deregister"],
        ["register", "register with current cpiSignatureData"],
        ["generate", "generate cpiSignatureData then register"]
      ];})
    .setRdb("sas.reg.attempt"),
    new InstallCertTimeVariable ("instCertTime", "inst cert time")
  ]
});

// This RDB variable is set by the OWA depending on NIT connection status
var nit_connected = toBool("<%get_single_direct('nit.connected');%>");

var pageData : PageDef = {
  title: "CBRS Install Parameters",

  // If NIT is connected then this whole page is disabled and the user is
  // prompted to open browser page at the connected NIT.
  disabled: nit_connected,
  alertHeading: "cbrs setup disable",
  alertText: "use connected nit",

  menuPos: ["Services", "CbrsParametersAdvConfig"],
  pageObjects: [CbrsRegStatusAdvConfig, CbrsParametersAdvConfig],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

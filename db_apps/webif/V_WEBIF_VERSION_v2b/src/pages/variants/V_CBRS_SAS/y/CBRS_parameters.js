// ------------------------------------------------------------------------------------------------
// CBRS install parameters setup
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var sasPrefix = "sas.";

var CbrsRegStatus = PageObj("CbrsRegStatus", "cbrs reg status",
{
  labelText: "CBRS registration status",
  rdbPrefix: sasPrefix,
  pollPeriod: 1000,
  readOnly: true,
  members: [
    staticTextVariable("regStatus", "reg status").setRdb("registration.state"),
    staticTextVariable("regRespMsg", "reg resp msg")
        .setRdb("registration.response_message")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.regStatus != "Registered");}),
    staticTextVariable("regReqErrCode", "reg req err code")
        .setRdb("registration.request_error_code")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.regStatus != "Registered");}),
    staticTextVariable("regRespCode", "reg resp code")
        .setRdb("registration.response_code")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.regStatus != "Registered");}),
    staticTextVariable("regRespData", "reg resp data")
        .setRdb("registration.response_data")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.regStatus != "Registered");}),
    staticTextVariable("grantStatus", "grant status").setRdb("grant.0.state"),
    staticTextVariable("grantRespMsg", "grant resp msg")
        .setRdb("grant.0.response_message")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.grantStatus != "AUTHORIZED");}),
    staticTextVariable("grantRespReason", "grant resp reason")
        .setRdb("grant.0.reason")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.grantStatus != "AUTHORIZED");}),
    staticTextVariable("grantRespData", "grant resp data")
        .setRdb("grant.0.response_data")
        .setIsVisible( function () {return toBool(CbrsRegStatus.obj.grantStatus != "AUTHORIZED");}),
  ]
});

var customLuaCbrsParameters = {
  lockRdb : false,
  set: function() {
    var luaScript = `
    if luardb.get("service.sas_client.enabled_by_ia") ~= o.clientEnable then
      luardb.set("service.sas_client.enabled_by_ia", o.clientEnable)
    end
    if luardb.get("sas.config.url") ~= o.sasServer then
      luardb.set("sas.config.url", o.sasServer)
      if o.clientEnable == "1" then
        os.execute("/etc/init.d/rc.d/sas_client restart")
      end
    end
`;
    return luaScript.split("\n");
  }
};

var CbrsParameters = PageObj("CbrsParameters", "CBRS parameters",
{
  labelText: "CBRS parameters",
  customLua: customLuaCbrsParameters,
  members: [
    hiddenVariable("clientEnable", "sas client enable").setRdb("service.sas_client.enable"),
    editableURL("sasServer", "sas server address").setRdb("sas.config.url"),
    staticTextVariable("latitude", "antenna latitude").setRdb("sas.antenna.latitude"),
    staticTextVariable("longitude", "antenna longitude").setRdb("sas.antenna.longitude"),
    staticTextVariable("height", "antenna height").setRdb("sas.antenna.height"),
    staticTextVariable("azimuth", "antenna azimuth").setRdb("sas.antenna.azimuth"),
    staticTextVariable("downtilt", "antenna downtilt").setRdb("sas.antenna.downtilt"),
    staticTextVariable("cpiId", "cpi id").setRdb("sas.config.cpiId"),
    staticTextVariable("cpiName", "cpi name").setRdb("sas.config.cpiName"),
    hiddenVariable("cpiPasscode", "cpi passcode"),
    staticTextVariable("callSign", "call sign").setRdb("sas.config.callSign"),
    staticTextVariable("userId", "user id").setRdb("sas.config.userId"),
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

  menuPos: ["Services", "CbrsParameters"],
  pageObjects: [CbrsRegStatus, CbrsParameters],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

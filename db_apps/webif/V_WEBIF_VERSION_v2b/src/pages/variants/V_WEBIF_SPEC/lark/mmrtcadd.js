//Copyright (C) 2020 Casa Systems.

var rtcFile = "/tmp/upload/runtime_conf.star";
var rtcFileUploader = new FileUploader("rtconfig", "Select configuration to upload", rtcFile, [".star"], null, "MsgWrongSTar", "Msg77", true);
rtcFileUploader.helperText = "&nbsp";

var po = PageObj("addRTConfig", "Add customized configuration",
{
  members: [
    rtcFileUploader,
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Add",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["OWA", "Add configuration"]
};

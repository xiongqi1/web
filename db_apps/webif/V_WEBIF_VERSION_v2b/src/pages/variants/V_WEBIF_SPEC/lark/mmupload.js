//Copyright (C) 2019 NetComm Wireless Limited.

var file = "/mnt/emmc/upload/magpie.star";
var fileUploader = new FileUploader("firmware", "Select firmware to upload", file, [".star"], null, "MsgWrongSTar", "Msg77", true);
fileUploader.helperText = "&nbsp";

var po = PageObj("uploadFirmware", "Upload OWA firmware",
{
  members: [
    fileUploader,
    staticTextVariable("ui_model", "UI model").setRdb("installation.ui_model"),
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Upload",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["OWA", "Upload firmware"],
  onReady: function() {
    if("magpie" == $("#inp_ui_model").text()) {
      $("a[href$='mmrtconf.html']").parent().remove();
      $("a[href$='mmrtcadd.html']").parent().remove();
    }
    $("#div_ui_model").remove();
  }
};

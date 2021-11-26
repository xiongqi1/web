//Copyright (C) 2019 NetComm Wireless Limited.

var file1 = "/tmp/upload/upgrade.star";
var file2 = "/tmp/upload/uboot.star"
var file3 = "/tmp/upload/gridvar.cof";

var fileUploader1 = new FileUploader("firmware", "Select firmware to upload", file1, [".star", ".st"], null, "MsgWrongSTar", "Msg77", true);
var fileUploader2 = new FileUploader("uboot", "Select image to upload", file2, [".star", ".st"], null, "MsgWrongSTar", "Msg77", true);

var po1 = PageObj("upgradeFirmware", "Upgrade firmware",
{
  members: [
    fileUploader1,
    buttonAction("flash", "Upgrade", "postFlashRequest"),
  ]
});

var po2 = PageObj("upgradeUboot", "Upgrade boot loader",
{
  members: [
    fileUploader2,
    buttonAction("write", "Upgrade", "postWriteRequest"),
  ]
});

var fileUploader3 = new FileUploader("gridvar", "Select data to upload", file3, [".cof", ".COF"], null, "MsgWrongCOF", "Msg77", false);
fileUploader3.helperText = "&nbsp;";

var po3 = PageObj("upgradeGridvar", "Upgrade World Magnetic Model data",
{
  members: [
    fileUploader3,
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Upgrade",
  authenticatedOnly: true,
  pageObjects: [po1, po2, po3],
  menuPos: ["NIT", "Upgrade"],
  onReady: function() {
    $("#inp_flash").prop('disabled', true);
    $("#inp_write").prop('disabled', true);
    if("1" != getUrlParameter("uboot")) {
      $("#objouterwrapperupgradeUboot").hide();
    }
  },
};

function getUrlParameter(sParam) {
  var sPageURL = window.location.search.substring(1),
    sURLVariables = sPageURL.split('&'),
    sParameterName,
    i;

  for (i = 0; i < sURLVariables.length; i++) {
    sParameterName = sURLVariables[i].split('=');

    if (sParameterName[0] === sParam) {
      return sParameterName[1] === undefined ? true : decodeURIComponent(sParameterName[1]);
    }
  }
}

function postFlashRequest() {
  fileUploader1.onUploaderFileChange("/" + relUrlOfPage, "", true);
  $("#inp_flash").blur();
}

function postWriteRequest() {
  fileUploader2.onUploaderFileChange("/" + relUrlOfPage, "", true);
  $("#inp_write").blur();
}

function onFirmwarePosted(respObj) {
  if(respObj.message == "firmwareUploaded" && respObj.result == "0") {
    $("#inp_flash").prop('disabled', false);
  }
  else {
    $("#inp_flash").prop('disabled', true);
  }
}

function onUbootPosted(respObj) {
  if(respObj.message == "bootLoaderUploaded" && respObj.result == "0") {
    $("#inp_write").prop('disabled', false);
  }
  else {
    $("#inp_write").prop('disabled', true);
  }
}

fileUploader1.onPosted = onFirmwarePosted;
fileUploader2.onPosted = onUbootPosted;

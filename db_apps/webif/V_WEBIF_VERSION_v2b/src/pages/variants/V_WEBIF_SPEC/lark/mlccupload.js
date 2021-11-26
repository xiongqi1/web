//Copyright (C) 2019 NetComm Wireless Limited.
//This file defines the webpage that allows technical support or development
//personnel to upload client certificate and key for activating debug functions on OWA.

var customLuaDebugMode = {
  lockRdb : false,
  get: function() {
    var luaScript = `
    require "luardb"
    o = {}
    o.debug_mode = luardb.get("owa.debug.enable")
`;
    return luaScript.split("\n");
  },

  set: function() {
    var luaScript = `
    require "luardb"

    local function enableService(name, on)
      local timeout_secs = 3
      local result_len = 64

      return 0 == luardb.invoke("service.authenticate", "activate", timeout_secs,
                                 result_len, "service", name, "enable", on)
    end

    local mode = o["debug_mode"]
    local name = ({"none", "support", "debug"})[mode+1];
    local ret = true

    enableService("support", 0)
    enableService("debug", 0)

    if '0' ~= mode then
      ret = enableService(name, 1)
    end

    if ret then
      luardb.set("owa.debug.enable", mode)
      return 0
    end

    return 1
`;
    return luaScript.split("\n");
  }
};

var file1 = "/tmp/upload/ccrt.pem";
var file2 = "/tmp/upload/ckey.pem";

var clientCertUploader = new FileUploader("certificate", "Select certificate to upload", file1, [".pem",  ".crt"], null, "Msg74", "Msg77");
var privateKeyUploader = new FileUploader("privateKey", "Select private key to upload", file2, [".pem", ".key"], null, "Msg74", "Msg77");

var po1 = PageObj("uploadClientCertificate", "Upload client certificate",
{
  members: [
    clientCertUploader,
  ]
});

var po2 = PageObj("uploadPrivateKey", "Upload private key",
{
  members: [
    privateKeyUploader,
  ]
});

var po3 = PageObj("enableOwaDebug", "Enable OWA debug functions",
{
  customLua: customLuaDebugMode,
  members: [
    selectVariable("debug_mode", "Debug mode", (obj)=>
      [["0", "Off"], ["1", "Support"], ["2", "Development"]], ""),
    buttonAction("apply", "Apply", "sendObjects", "")
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Certificate",
  authenticatedOnly: true,
  pageObjects: [po1, po2, po3],
  menuPos: ["NIT", "Certificate"]
};

function onPrivateKeyUploaded(respObj) {
  if(respObj.message == "encryptedPrivateKeyUploaded" && respObj.result == "0") {
    document.cookie = "redirect_url=/mlccupload.html";
    $(location).attr("href", "/mldecrypt.html");
  }
}

clientCertUploader.helperText = "&nbsp;";
privateKeyUploader.helperText = "&nbsp;";
privateKeyUploader.onPosted = onPrivateKeyUploaded;

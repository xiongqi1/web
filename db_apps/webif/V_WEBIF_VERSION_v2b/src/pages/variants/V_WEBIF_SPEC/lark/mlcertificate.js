//Copyright (C) 2021 Casa Systems.
//This file defines the webpage that allows installers to update Lark's certificate.

var customLuaCertInfo = {
  lockRdb : false,
  get: function() {
    var luaScript = `
    require "luardb"
    o = {}
    o.cert_info = "Certificate not installed."

    local cert_path = luardb.get("service.authenticate.clientcrtfile")
    local cert_exists = cert_path and 0 == os.execute("test -f " .. cert_path)

    if cert_exists then
        local cmd = "openssl x509 -text -in " .. cert_path
        local file = io.popen(cmd, 'r')
        local info = file:read('*a')
        file:close()
        if info and #info > 0 then
            o.cert_info = info
        else
            o.cert_info = "Cannot get certificate information."
        end
    end
`;
    return luaScript.split("\n");
  }
};

var po1 = PageObj("larkCertInfo", "Certificate information",
{
  customLua: customLuaCertInfo,
  members: [
    constTextArea("cert_info", "", {cols: 80, rows: 40}),
  ]
});

var file = "/tmp/upload/cert.p12";
var larkCertUploader = new FileUploader("certificate", "Select certificate to upload", file, [".p12",  ".pfx"], null, "Msg150", "Msg77");
var  po2 = PageObj("updateDeviceCertificate", "Update device certificate",
{
  members: [
    larkCertUploader,
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Certificate",
  authenticatedOnly: true,
  pageObjects: [po1, po2],
  menuPos: ["NIT", "Certificate"]
};

function onLarkCertUploaded(respObj) {
  if(respObj.message == "deviceCertsUpdated" && respObj.result == "0") {
    setTimeout(()=>$(location).attr("href", "/mlcertificate.html"), 2000);
  }
}

larkCertUploader.helperText = "&nbsp;";
larkCertUploader.onPosted = onLarkCertUploaded;

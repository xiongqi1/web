//Copyright (C) 2019 NetComm Wireless Limited.
//This file defines the webpage that decrypts an encrypted authentication key

var customLuaDebugMode = {
  lockRdb : false,
  get: function() {
    var luaScript = `
    local luardb = require("luardb")
    o = {}

    local key_file = luardb.get("service.authenticate.clientkeyfile")
    local key_exists = key_file and 0 == os.execute("test -f " .. key_file)

    o.decryption_required = key_exists and 0 ~= os.execute("openssl rsa -noout -passin pass: -in " ..
      key_file .. " &> /dev/null")
    o.key_password = ""
`;
    return luaScript.split("\n");
  },

  set: function() {
    var luaScript = `
    local luardb = require("luardb")
    local escape = require("turbo.escape")

    local client_key_file = luardb.get("service.authenticate.clientkeyfile")
    local decrypted_key_file = "/tmp/decrypted.key"

    local passphrase = o["key_password"]
    passphrase = escape.base64_decode(passphrase)
    passphrase = escape.json_encode(passphrase)

    local command = 'openssl rsa -in ' .. client_key_file .. ' -out ' .. decrypted_key_file ..
      ' -passin pass:' .. passphrase ..  ' &> /dev/null && mv ' ..
      decrypted_key_file .. ' ' .. client_key_file

    local ret = os.execute(command)
    return ret
`;
    return luaScript.split("\n");
  }
};

var po = PageObj("decrypt", "Decrypt authentication key",
{
  customLua: customLuaDebugMode,
  members: [
    editablePasswordVariable("key_password","privateKeyPassphrase"),
    buttonAction("decrypt", "decrypt", "sendObjects", ""),
    staticTextVariable("notice", "authKeyDecrypted")
  ],
  decodeRdb: onDecodeRdb
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "decryptPrivateKey",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["NIT", "DecryptKey"],
  onDataUpdate: onDataUpdate,
  onReady: onReady,
  onSubmit: onSubmit
}

function getCookieValue(name, alter) {
  var match = document.cookie.match("(^|[^;]+)\\s*" + name + "\\s*=\\s*([^;]+)");
  return match ? match.pop() : alter;
}

function delCookie(name) {
  document.cookie = name + "=; expires=Thu, 01-Jan-70 00:00:01 GMT;";
}

function redirectPage() {
  var page = getCookieValue("redirect_url", "");
  if(page == "") {
    return;
  }

  $(location).attr("href", page);
  delCookie("redirect_url");
}

function onDecodeRdb(obj) {
  if(!obj.decryption_required) {
    redirectPage();
  }
 return obj;
}

function onDataUpdate(resp) {
  if(resp.result) {
    validate_alert(_("invalidPassphrase"), _("pleaseTryAgain"));
  }
  else {
    $("#div_key_password").hide();
    $("#div_decrypt").hide();
    $("#div_notice").show();

    $(".note").hide();
  }
}

function onReady(resp) {
  if(resp.decrypt.decryption_required) {
    $("#div_notice").hide();
  }
  else {
    $("#div_key_password").hide();
    $("#div_decrypt").hide();
  }
}

function onSubmit(event) {
  sendObjects();

  $("#inp_key_password").blur();
  event.preventDefault();

  return false;
}
// Copyright (C) 2020  Casa Systems Inc.

var debugLogPrefix = "service.debuginfo.";
var customLuaDebugInfo={
  lockRdb : false,
  get: function(){
  var luaScript = `
    local luardb = require("luardb")
    local ui_model = luardb.get('installation.ui_model')
    local prefix = (ui_model == 'myna' or ui_model == 'magpie') and 'sas' or 'service'
    o = {}
    o.status=luardb.get(prefix..'.debuginfo.status') or ''
    o.url=luardb.get(prefix..'.debuginfo.url') or ''
    if prefix == 'sas' then luardb.set('service.debuginfo.status',o.status) end
    o.lstatus=luardb.get('service.debuginfo.lstatus') or ''
    if o.url and o.url ~= '' then
      if not o.lstatus:match('transferring') and not o.lstatus:match('ready') then
        o.lstatus='transferring OWA debug info to local storage'
        luardb.set('service.debuginfo.lstatus',o.lstatus)
        local postcmd=';rdb_set service.debuginfo.lstatus "OWA debug info is ready for download"'
        local cmd='(mkdir /tmp/download; cd /tmp/download; wget '..o.url..postcmd..')&'
        os.execute(cmd)
      end
    elseif not o.lstatus:match('waiting for') then
      luardb.set('service.debuginfo.lstatus','')
    end
`;
    return luaScript.split("\n");
  },
  set: function(){
  var luaScript = `
    local luardb = require("luardb")
    local ui_model = luardb.get('installation.ui_model')
    local prefix = (ui_model == 'myna' or ui_model == 'magpie') and 'sas' or 'service'
    luardb.set(prefix..'.debuginfo.trigger', '1')
    luardb.set(prefix..'.debuginfo.url','')
    luardb.set('service.debuginfo.lstatus','debug info order sent to OWA, waiting for result...')
`;
    return luaScript.split("\n");
  },
}

// set a timer to refresh page once per 10s to update status text
var timer
function setRefreshTimer(){
  timer=setTimeout(function() { location.reload();}, 10000);
  setVisible("#div_download", po.obj.lstatus.match(/ready/g) == "ready");
}
function sendobjects_setRefreshTimer() { sendObjects(); setRefreshTimer(); }

// download log
function onDownloadDebugInfo() {
  clearTimeout(timer);
  var res = po.obj.lstatus.match(/ready/g);
  if (res == "ready") {
    var url=document.createElement("a");
    url.href=po.obj.url;
    var element = document.createElement('a');
    element.setAttribute('href', '/download/'+url.pathname);
    element.setAttribute('download', url.pathname.replace("/",""));
    element.style.display = 'none';
    document.body.appendChild(element);
    element.click();
    document.body.removeChild(element);
  } else {
    blockUI_alert(_("Wait for the current log generation to finish or generate new order"));
  }
}

// generate new debug log
function onGenerateDebugInfo() {
  clearTimeout(timer);
  if (po.obj.lstatus.match(/ready/g) == "ready") {
    blockUI_confirm("Confirm to discard current log and generate new one",
      sendobjects_setRefreshTimer, setRefreshTimer);
    return;
  }
  if (po.obj.lstatus.match(/waiting for result/g) == "waiting for result") {
    blockUI_confirm("Confirm to ignore current order and generate new one", sendobjects_setRefreshTimer, setRefreshTimer);
    return;
  }
  sendobjects_setRefreshTimer();
}

var po = PageObj("debugInfo", "OWA system debug info",
{
  customLua: customLuaDebugInfo,
  rdbPrefix: debugLogPrefix,
  members: [
    staticTextVariable("lstatus","local status").setRdb("lstatus"),
    staticTextVariable("status","OWA status").setRdb("status"),
    buttonAction("generate", "generate", "onGenerateDebugInfo", "Generate new debug info"),
    buttonAction("download", "download", "onDownloadDebugInfo", "Download debug info"),
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "DebugInfo",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["OWA", "DebugInfo"],
  alertSuccessTxt: "debug info ordered, refresh the page and check the status text",
  onReady: function (){
    $("#htmlGoesHere").after(genSaveRefreshCancel(false, true, false));
    setRefreshTimer();
    $("#refreshButton").on('click', function(){location.reload()});
  }
};

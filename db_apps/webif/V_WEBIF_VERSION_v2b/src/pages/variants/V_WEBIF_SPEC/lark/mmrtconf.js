//Copyright (C) 2019 NetComm Wireless Limited.

var customLuaListRuntimeConfig = {
  lockRdb : false,
  get: function() {
    var luaScript = `
  require("lfs")
  local luardb = require("luardb")
  o = {}

  local rtc_path = "/mnt/emmc/rtconfig/"

  local odu_mcc = ""
  local odu_mnc = ""

  if "1" == luardb.get("owa.connected") then
    local model = luardb.get("owa.system.product")
    if model and not model:find("eng") then
      local efad = luardb.get("wwan.0.sim.raw_data.ad")
      local imsi = luardb.get("wwan.0.imsi.msin")
      local mnc_len = efad:sub(-1)
      odu_mcc = imsi:sub(1, 3)
      odu_mnc = imsi:sub(4, 3 + mnc_len)
    end
  end

  local function read_cfg_info(index)
    local filename = rtc_path .. index .. ".txt"

    local fobj = io.open(filename, "r")
    if nil == fobj then
      return nil
    end

    local info = {}
    info.index = index
    info.mcc = fobj:read("*line")
    info.mnc = fobj:read("*line")
    info.skin = fobj:read("*line")
    info.variant = fobj:read("*line")
    info.fw_ver = fobj:read("*line")
    info.cfg_ver = fobj:read("*line")
    info.build = fobj:read("*line")

    fobj:close()

    return info
  end

  local function find_cfg_names()

    local indexes= {}
    for file in lfs.dir(rtc_path) do
      local match = string.match(file, "([0-9]+).txt")
      if nil ~= match then
        table.insert(indexes, tonumber(match))
      end
    end

    table.sort(indexes)
    return indexes
  end

  if not os.rename(rtc_path, rtc_path) then
    lfs.mkdir(rtc_path)
  end

  local indexes  = find_cfg_names()
  for _, index in ipairs(indexes) do
    local info = read_cfg_info(index)
    if nil ~= info then
      if "" == odu_mcc or "" == odu_mnc or
        odu_mcc == info.mcc and odu_mnc == info.mnc then
        info.id = table.getn(o)
        table.insert(o, info)
      end
    end
  end
`;
    return luaScript.split("\n");
  },

  set: function() {
    var luaScript = `
  local rtc_path = "/mnt/emmc/rtconfig/"

  local function remove_cfg(keep)
    local files = {}
    for file in lfs.dir(rtc_path) do
      local match = string.match(file, "([0-9]+).txt")
      if nil ~= match and nil == keep[match] then
        os.remove(rtc_path .. match .. ".txt")
        os.remove(rtc_path .. match .. ".star")
      end
    end
  end

  local records = o["objs"]
  local keep = {}

  for i, record in ipairs(records) do
    keep[record.index .. ""] = record
  end

  remove_cfg(keep)
  return 0
`;
    return luaScript.split("\n");
  }
};

function onConfigPageReady() {
    $("#listRuntimeConfig tr").each(function() {
      var col_val = $(this).find("td:eq(7)").text();
      if (col_val == _("engineering")) {
        $(this).find("td:eq(7)").html("<span style='background-color:yellow'>" + col_val + "</span>");
      }
    });
}

var po = PageTableObj("listRuntimeConfig", "Configuration",
{
  customLua: customLuaListRuntimeConfig,
  readOnly: false,
  editRemovalOnly: true,
  colgroup: ["90px","60px","60px","60px","160px","160px","60px","auto"],
  tableAttribs: {class:"above-5-column"},
  members: [
    staticTextVariable("index", "Configuration"),
    staticTextVariable("mcc", "MCC"),
    staticTextVariable("mnc", "MNC"),
    staticTextVariable("skin", "Customer"),
    staticTextVariable("variant", "Compatible hardware"),
    staticI18NVariable("fw_ver", "Compatible firmware"),
    staticI18NVariable("cfg_ver", "Version"),
    staticI18NVariable("build", "Type"),
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Customized configuration",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["OWA", "Configuration"],
  onReady: onConfigPageReady,
  onDataUpdate: onConfigPageReady
};

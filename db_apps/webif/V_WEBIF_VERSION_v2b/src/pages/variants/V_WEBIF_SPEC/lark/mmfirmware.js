//Copyright (C) 2019 NetComm Wireless Limited.

var customLuaListFirmware = {
  lockRdb : false,
  get: function() {
    var luaScript = `
  require("lfs")
  o = {}

  local firmware_path = "/mnt/emmc/firmware/"

  local function read_fw_info(index)
    local filename = firmware_path .. index .. ".txt"

    local fobj = io.open(filename, "r")
    if nil == fobj then
      return nil
    end

    local info = {}
    info.index = index
    info.version = fobj:read("*line")
    info.date = fobj:read("*line")
    info.size = fobj:read("*line")
    info.compatible = fobj:read("*line"):gsub(",", ", ")
    info.build = fobj:read("*line")

    fobj:close()

    return info
  end

  local function find_fw_names()

    local indexes= {}
    for file in lfs.dir(firmware_path) do
      local match = string.match(file, "([0-9]+).txt")
      if nil ~= match then
        table.insert(indexes, tonumber(match))
      end
    end

    table.sort(indexes)
    return indexes
  end

  local indexes  = find_fw_names()
  for _, index in ipairs(indexes) do
    local info = read_fw_info(index)
    if nil ~= info then
      info.id = table.getn(o)
      table.insert(o, info)
    end
  end
`;
    return luaScript.split("\n");
  },

  set: function() {
    var luaScript = `
  local firmware_path = "/mnt/emmc/firmware/"

  local function remove_fw(keep)
    local files = {}
    for file in lfs.dir(firmware_path) do
      local match = string.match(file, "([0-9]+).txt")
      if nil ~= match and nil == keep[match] then
        os.remove(firmware_path .. match .. ".txt")
        os.remove(firmware_path .. match .. ".star")
      end
    end
  end

  local records = o["objs"]
  local keep = {}

  for i, record in ipairs(records) do
    keep[record.index .. ""] = record
  end

  remove_fw(keep)
  return 0
`;
    return luaScript.split("\n");
  }
};

function onPageReady() {
    var model = $("#inp_ui_model").text();
    // Hide the 'Build type' column in the Web-UI for Myna, Myna-Lite and Sparrow products.
    if ("myna" == model || "myna_lite" == model || "sparrow" == model) {
      $('td:nth-child(6),th:nth-child(6)').hide();
    }
    else {
      $("#listFirmware tr").each(function() {
        var col_val = $(this).find("td:eq(5)").text();
        if (col_val == _("engineering")) {
          $(this).find("td:eq(5)").html("<span style='background-color:yellow'>" + col_val + "</span>");
        }
      });

      if("magpie" == model) {
        $("a[href$='mmrtconf.html']").parent().remove();
        $("a[href$='mmrtcadd.html']").parent().remove();
      }
    }

    $("#div_ui_model").remove();
    $("h2:contains('ODU Info')").remove();
}

var po = PageTableObj("listFirmware", "Firmware list",
{
  customLua: customLuaListFirmware,
  readOnly: false,
  editRemovalOnly: true,
  colgroup: ["90px","130px","130px","120px","180px","120px","auto"],
  tableAttribs: {class:"above-5-column"},
  members: [
    staticTextVariable("index", "Firmware"),
    staticTextVariable("version", "Revision"),
    staticTextVariable("date", "Release date"),
    staticTextVariable("size", "Size"),
    staticTextVariable("compatible", "Compatible hardware"),
    staticI18NVariable("build", "Build type"),
  ]
});

var poOduInfo = PageObj("oduInfo", "ODU Info",
{
  readOnly: true,
  members: [
    staticTextVariable("ui_model", "UI model").setRdb("installation.ui_model"),
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Firmware",
  authenticatedOnly: true,
  pageObjects: [po, poOduInfo],
  menuPos: ["OWA", "Firmware"],
  onReady: onPageReady,
  onDataUpdate: onPageReady
};

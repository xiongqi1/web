//Copyright (C) 2019 Casa Systems.

var customLuaPowerdown= {
  lockRdb : false,
  get: function() {
    var luaScript = `
    local luardb = require("luardb")
    o = {}

    o.inactivity_duration = luardb.get("service.system.poweroff.duration")
    o.powerdown_left = math.floor(luardb.get("service.system.poweroff.remaining")/100)
`;
    return luaScript.split("\n");
  },

  set: function() {
    var luaScript = `
    local luardb = require("luardb")
    local duration = o["inactivity_duration"]

    luardb.set("service.system.poweroff.duration", duration)
    os.execute("inactivity_control.sh reset")

    return 0
`;
    return luaScript.split("\n");
  }
};

var po1 = PageObj("autoPowerdownSettings", "Powerdown settings",
{
  customLua: customLuaPowerdown,
  members: [
    selectVariable("inactivity_duration", "Inactivity Duration", ()=>
      [["0", "disabled"],
      ["300", "5 minutes"],
      ["600", "10 minutes"],
      ["900", "15 minutes"],
      ["1200", "20 minutes"],
      ["1500", "25 minutes"],
      ["1800", "30 minutes"],
      ["2100", "35 minutes"],
      ["2400", "40 minutes"],
      ["2700", "45 minutes"],
      ["3000", "50 minutes"],
      ["3300", "55 minutes"],
      ["3600", "60 minutes"]]),
    buttonAction("save", "Save", "sendObjects();powerdownCheck();"),
  ]
});

var pageData : PageDef = {
#ifndef V_AUTO_POWERDOWN_y
  onDevice : false,
#endif
  title: "Auto powerdown",
  authenticatedOnly: true,
  pageObjects: [po1],
  menuPos: ["NIT", "Auto powerdown"],
};

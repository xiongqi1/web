// ------------------------------------------------------------------------------------------------
// CBRS Config & Test
// Copyright (C) 2020 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

var customLuaCbrsTxPowerDisplay = {
  get: function(arr) {
    var luaScript = `
    o.maxTxPower = tonumber(luardb.get("sas.antenna.last_eirp")) or ""
    if o.maxTxPower ~= "" then
        local antenna_gain = tonumber(luardb.get("sas.antenna.gain")) or 19
        o.maxTxPower = o.maxTxPower + antenna_gain
    end
`;
    return arr.concat(luaScript.split("\n"));
  }
};

var CbrsTxPowerDisplay = PageObj("CbrsTxPowerDisplay", "CBRS tx power display",
{
  labelText: "CBRS tx power display",
  pollPeriod: 1000,
  customLua: customLuaCbrsTxPowerDisplay,
  members: [
    staticTextVariable("maxTxPower", "max tx power"),
  ]
});

var customLuaCbrsTxPowerCfg = {
  set: function(arr) {
    var luaScript = `
    local antenna_gain = tonumber(luardb.get("sas.antenna.gain")) or 19
    local currTxPower = tonumber(luardb.get("sas.antenna.last_eirp")) or 0
    -- Create the variable if it does not exist
    if currTxPower == 0 then
        luardb.set("sas.antenna.last_eirp","","p")
    end
    -- Retrieve the 'new max. Tx power' value from UI
    local new_max_tx_power = o.newmaxTxPower
    luardb.set("service.luaqmi.command", string.format("setTxPower, 48, %d, 0",(new_max_tx_power - antenna_gain)))
`;
    return arr.concat(luaScript.split("\n"));
  }
};

var CbrsTxPowerCfg = PageObj("CbrsTxPowerCfg", "CBRS tx power cfg",
{
  labelText: "CBRS tx power cfg",
  customLua: customLuaCbrsTxPowerCfg,
  members: [
    editableBoundedInteger("newmaxTxPower", "new max tx power", 23, 41, "Please enter a valid value", {helperText: "(Range:23-41 dBm/10MHz)"}),
  ]
});
var pageData : PageDef = {
  title: "CBRS Parameters Config and Test",
  disabled: false,
  menuPos: ["Services", "CbrsTxPowerCfg"],
  pageObjects: [CbrsTxPowerDisplay,CbrsTxPowerCfg],
  validateOnload: false,
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

function populate_eth_stat(obj) {
    var eth_profiles=obj.wanInfo;
    /* sort */
    eth_profiles.sort((a,b) => a.metrics-b.metrics);
    /* build failover status table */
    var div_ethwan_fo = [];
    eth_profiles.forEach(function(o){
        /* apply default */
        if(o.monitor_type == "") {
            o.monitor_type = "phy";
        }
        if(div_ethwan_fo.length == 0) {
            div_ethwan_fo.push("<dl>" + "<dt>" + _("priority") + "</dt>" + "</dl>");
        }
        div_ethwan_fo.push(
        "<dl>"+
            "<dd><span style='display:inline-block;width:140px'>" + (o.dev_type == "wwan" ? "wwan." + o.pf: o.dev) +
            " ["+_(o.monitor_type)+"]</span>" +
            "<i class=" + (o.fo_status=="up" ? "success-sml": "warning-sml") +
            "></i></dd>" +
        "</dl>"
        );
    });

    $("#StsWanInfo_0_0_div").html(div_ethwan_fo.length == 0? "<dl><dd>"+_("no wan interface is configured")+"</dd></dl>": div_ethwan_fo.join(""));
}

// TODO
// vlan_cfg.sh is part of V_MULTIPLE_LANWAN so not exist in Cassini
// at the moment. To use this, need to copy the file from Bovine or Serpent
// TODO
// Should be changed same as WWanInfo-Saturn.js considering link.policy variables
var customLuaGetWanInfo = {
    lockRdb : false,
    get: function() {
        var luaScript = `
        require("stringutil")
        wanInfo = {}
        o = {}
        local stdout = io.popen("vlan_cfg.sh get_active_profiles", "r")
        for pf in stdout:lines() do
            local info = {}
            local dev = luardb.get("link.profile."..pf..".dev")
            info.pf = pf
            info.name = luardb.get("link.profile."..pf..".name")
            info.dev = dev
            info.dev_type = dev:gsub(".[0-6]","")
            info.metrics = luardb.get("link.profile."..pf..".defaultroutemetric")
            info.status = luardb.get("link.profile."..pf..".status")
            network_if = luardb.get("link.profile."..pf..".interface")
            info.conntype = luardb.get("link.profile."..pf..".conntype")
            info.ip = luardb.get("link.profile."..pf..".iplocal")
            info.gw = luardb.get("link.profile."..pf..".ipremote")
            info.mask = luardb.get("link.profile."..pf..".mask")
            info.dns1 = luardb.get("link.profile."..pf..".dns1")
            info.dns2 = luardb.get("link.profile."..pf..".dns2")
            info.fo_status = luardb.get("service.failover."..pf..".status") or ""
            info.monitor_type = luardb.get("service.failover."..pf..".monitor_type") or ""
            table.insert(wanInfo, info)
        end
        o.wanInfo = wanInfo
`;
    return luaScript.split("\n");
}};

var WanInfo = PageObj("StsWanInfo", "wan",
{
    customLua: customLuaGetWanInfo,
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    pollPeriod: 1000,
    columns : [{
        members:[
        {hdg: "IP"}
        ]
    }],

    populate: function () {
        populate_eth_stat(this.obj);
        return _populateCols(this.columns, this.obj);
    },

    members: [
        hiddenVariable("wanInfo", '')
    ]
});

stsPageObjects.push(WanInfo);

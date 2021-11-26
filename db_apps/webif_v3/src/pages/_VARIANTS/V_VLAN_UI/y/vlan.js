var vlanRebootWarningHeader = "Configuring VLAN rules may cause your device to reboot.";
var vlanRebootWarningParagraph = "Device will reboot when going from zero to one or more enabled VLANs,"
  + " and the reverse. The reboot will take a few minutes, during which you won't be able to access your device.";

var VlanCfg = PageObj("VlanCfg", "VLAN configuration",
{
  pageWarning: [vlanRebootWarningHeader, vlanRebootWarningParagraph],
  members: [
    toggleVariable("vlanEnable", "Enable").setRdb("vlan.enable"),
    hiddenVariable("enableTrigger", "vlan.trigger"),
    buttonAction("vlanCfgSaveButton", "Save", "sendSingleObject(VlanCfg);", "", {buttonStyle: "submitButton"}),
  ],
  decodeRdb: function(obj) {
    if (obj.enable == "") {
      obj.enable = "0";
    }
    obj.enableTrigger = "1";
    return obj;
  },
});

#ifdef COMPILE_WEBUI
var customLuaVlan = {
  lockRdb : false,
  get: (arr)=>[...arr, `
        local rawobj=getRdbArray(authenticated,'vlan',0,49,false,{'rule_name','enable','id','address','netmask','dhcp.range','dhcp.lease','dhcp.enable','admin_access_allowed','interface','name'})
        -- vlan.num is the last index of vlan rules
        local lastIndex = tonumber(luardb.get('vlan.num'))
        for idx, obj in ipairs(rawobj) do
          if idx > lastIndex then break end
          if rawobj[idx]['id'] ~= '' then
            if rawobj[idx]['address'] and rawobj[idx]['address'] ~= '' then
              rawobj[idx]['dhcp_lease'] = rawobj[idx]['dhcp.lease']
              rawobj[idx]['dhcp_range'] = rawobj[idx]['dhcp.range']
              rawobj[idx]['dhcp_enable'] = rawobj[idx]['dhcp.enable']
            else
              rawobj[idx]['dhcp_lease'] = ''
              rawobj[idx]['dhcp_range'] = ''
              rawobj[idx]['dhcp_enable'] = ''
            end
            table.insert(o, rawobj[idx])
          end
        end
    `],
  set: (arr)=>[...arr, `
      local lastIndex = -1
      for idx, obj in ipairs(o.objs) do
        o.objs[idx]['dhcp.lease'] = o.objs[idx]['dhcp_lease']
        o.objs[idx]['dhcp.range'] = o.objs[idx]['dhcp_range']
        o.objs[idx]['dhcp.enable'] = o.objs[idx]['dhcp_enable']
        local ifname = o.objs[idx].interface or ''
        local vlan_id = o.objs[idx].id or ''
        if ifname ~='' and vlan_id ~= '' then
          o.objs[idx].name = ifname .. '.' .. vlan_id
        end
        if not o.objs[idx].__deleted and o.objs[idx].__id > lastIndex then
          lastIndex = o.objs[idx].__id
        end
      end
      local rc = setRdbArray(authenticated,'vlan',{'rule_name','enable','id','address','netmask','dhcp.range','dhcp.lease','dhcp.enable','admin_access_allowed','interface','name'},o)
      luardb.set('vlan.num',lastIndex + 1)
      luardb.set('vlan.trigger','1')
      return rc
    `],
  helpers: [
    "function isValidName(val)",
    " if val and #val >= 1 and #val <= 32 "
        + "and val:match('[!()*/0-9;?A-Z_a-z-]+') then",
    "  return true",
    " end",
    " return false",
    "end"
  ]
};
#endif

var VlanRules = PageTableObj("VlanRules", "VLAN rules",
{
#ifdef COMPILE_WEBUI
  customLua: customLuaVlan,
#endif
  sendThisObjOnly: true,
  editLabelText: "VLAN Settings",
  arrayEditAllowed: true,
  indexHoleAllowed: true,
  extraAttr: {
    tableAttr: {class:"above-5-column"},
    thAttr: [
      {class:"customTh field1"},
      {class:"customTh field2"},
      {class:"customTh field1"},
      {class:"customTh field1"},
      {class:"customTh field3"},
      {class:"customTh field2"},
      {class:"customTh field2"},
      {class:"customTh field2"},
      {class:"customTh field2"},
    ],
  },

  initObj:  function() {
      var obj: any = {};
      obj.rule_name = "";
      obj.id = "";
      obj.address = "";
      obj.netmask = "";
      obj.dhcp_range =  "";
      obj.startRange = "";
      obj.endRange = "";
      obj.dhcp_lease = "";
      obj.dhcp_enable = "0"
      obj.admin_access_allowed = "0";
      obj.interface = "eth0";
      obj.name = "";
      obj.enable = "0";
      return obj;
  },

  decodeRdb: function(objs: any[]) {
    objs.forEach(function(obj){
      var addrAr = obj.dhcp_range.split(",");
      obj.startRange = (addrAr.length > 0)? addrAr[0]:"";
      obj.endRange = (addrAr.length > 1)? addrAr[1]:"";
    });
    return objs;
  },

  onEdit: function (obj) {
    setEditButtonRowVisible(true);
    setPageObjVisible(false, "VlanCfg");
  },

  offEdit: function () {
    setEditButtonRowVisible(false);
    setPageObjVisible(true, "VlanCfg");
  },

  members: [
    staticTextVariable("rule_name", "Name"),
    staticTextVariable("id", "ID"),
    staticTextVariable("address", "Address"),
    staticTextVariable("netmask", "Subnet mask"),
    staticTextVariable("dhcp_range", "DHCP range"),
    staticTextVariable("dhcp_lease", "Lease time"),
    staticTextVariable("dhcp_enable", "DHCP enable"),
    staticTextVariable("admin_access_allowed", "Allow admin access"),
    staticTextVariable("enable", "Enable"),
  ],

  saveObj: function(obj) {
    obj.dhcp_range = obj.startRange + "," + obj.endRange;
    return obj;
  },

//  visibilityVar: "dhcp_enable",

  setVisible: function (v) {
    var pgeObj = this;
    var visVar = pgeObj.visibilityVar;
    if ((typeof v !== "undefined")) {
      if ((typeof pgeObj.setEnabled === "function")) {
        pgeObj.setEnabled(v == "1");
      }
    }
    else {
      v = this.obj[visVar];
    }

    var visibleMembers = [];
    pgeObj.editMembers.forEach(function(mem){
        if (mem.memberName == "startRange" ||
            mem.memberName == "endRange" ||
            mem.memberName == "dhcp_lease") {
            visibleMembers.push(mem)
        }
    })

    setToggle("inp_" + visVar, v);
    setMemberVisibility(pgeObj, toBool(v), visibleMembers);
  },

  postPageObj : function () {
    var obj = this;
    obj.editMembers.forEach(function(mem){
      if (mem.visibilityVar){
        obj.visibilityVar = mem.memberName;
        mem.onClick = "onClick" + obj.visibilityVar;
      }
    });
  },

  editMembers: [
    editableTextVariable("rule_name", "Rule name")
      .setMaxLength(32)
      .setValidate(
        function (val) { if (val.length == 0) return false; return true;}, "Rule name is invalid",
        function (field) {return nameFilter(field);},
        "isValidName(v)"
      ),
    editableBoundedInteger("id", "VLAN ID", 0, 4094, "Please specify a value between 0 and 4094", {helperText: _("0-4094")}),
#ifdef COMPILE_WEBUI
    editableVlanIpAddressVariable("address", "IP address"),
    editableIpMaskVariable("netmask", "Subnet mask"),
    objVisibilityVariable("dhcp_enable", "DHCP enable"),
    editableIpAddressVariable("startRange", "DHCP start range"),
    editableIpAddressVariable("endRange", "DHCP end range"),
#else
    editableVlanIpAddressVariable("address", "IP address", genHtmlIpBlocks0),
    editableIpMaskVariable("netmask", "Subnet mask", genHtmlMaskBlocksWithoutRequired),
    objVisibilityVariable("dhcp_enable", "DHCP enable"),
    editableIpAddressVariable("startRange", "DHCP start range", genHtmlIpBlocks0),
    editableIpAddressVariable("endRange", "DHCP end range", genHtmlIpBlocks0),
#endif
    editableBoundedInteger("dhcp_lease", "DHCP lease time (seconds)", 120, 99999, "Minimum lease time is 120 seconds", {helperText: "(>=120)"}).setMaxLength(5),
    toggleVariable("admin_access_allowed", "Allow admin access"),
    toggleVariable("enable", "Enable"),
  ]
});

var pageData : PageDef = {
  title: "VLAN",
  menuPos: ["Networking", "LAN", "VLAN"],
  pageObjects: [VlanCfg, VlanRules],
  onReady: function (){
    appendButtons({"save":"CSsave", "cancel":"cancel"});
    setButtonEvent('save', 'click', sendObjects);
    setEditButtonRowVisible(false);
  }
}

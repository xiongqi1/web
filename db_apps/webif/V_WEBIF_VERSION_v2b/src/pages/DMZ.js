var customLuaDMZ = {
  // This custom validation is after the standard validation and checks that the IP address
  // given is also with the LAN subnet (or VLANs)
  validate: function(defVal) {
    return defVal.concat( [
      "if isWithinLAN(o.dmzIp)==false then response.errorText='dmz warningMsg02' return false, 'notWithinLAN' end"
    ]);
  },
  helpers: [
    "function isWithinLAN(ip)",
#if defined(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y)
    "  local cnt= tonumber(getRdb('vlan.count'))",
    "  if cnt then",
    "    response.debugcnt = cnt",
    "    local vlans=getRdbArray(true,'vlan',0,cnt,true,{'address','netmask'})",
    "    response.debug=vlans",
    "    cnt = #vlans",
    "    for i=1,cnt do",
    "      if isValid.IpWithinRange(ip,vlans[i].address,vlans[i].netmask) then return true end",
    "    end",
    "  end",
#endif
#if defined(V_CUSTOM_FEATURE_PACK_Santos) || !defined(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y)
    "  return isValid.IpWithinRange(ip,getRdb('link.profile.0.address'),getRdb('link.profile.0.netmask'))",
#else
    "  return false",
#endif
    "end"
  ]
};

var DmzCfg = PageObj("DmzCfg", "dmz configuration",
{
  customLua: customLuaDMZ,
  members: [
    objVisibilityVariable("dmzEnable", "treeapp dmz").setRdb("service.firewall.dmz.enable"),
    editableIpAddressVariable("dmzIp", "dmz ipaddr").setRdb("service.firewall.dmz.address")
    //,editableIpv6AddressVariable("dmzIpv6", "dmz ipaddrv6").setRdb("service.firewall.dmz.addressv6")
  ]
});

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none
  onDevice : false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["Internet", "DMZ"],
  pageObjects: [DmzCfg],
  alertSuccessTxt: "dmzSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

disablePageIfPppoeEnabled();

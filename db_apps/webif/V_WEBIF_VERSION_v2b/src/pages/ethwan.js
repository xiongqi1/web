// ------------------------------------------------------------------------------------------------
// Ethernet WAN configuration
// Copyright (C) 2018 NetComm Wireless Limited.
// ------------------------------------------------------------------------------------------------
var customLuaGetEthWANLinkProfile = {
  lockRdb: false,
  get: function() {
    return [
      "=getWanProfiles(authenticated)"
    ];
  },
  set: function() {
    return [
      "--put the single profile obj into array so we can use setRdbArray to save it.",
      "local array={objs={o}}",
      "return setRdbArray(authenticated, rdbPrefix, rdbMembers, array)"
    ];
  },
  helpers:[
    "local rdbPrefix = 'link.profile'",
    "local rdbMembers = ",
    "  {'dev', 'conntype', 'snat', 'iplocal', 'mask', 'gw', 'dns1', 'dns2', 'defaultroutemetric'}",
    "function getWanProfiles(auth)",
    "  -- read upto 16 ethernet network interfaces",
    "  local interfaces = getRdbArray(auth, 'network.interface.eth', 0, 16, false, {'mode'})",
    "  -- read upto 16 ethernet wan profiles, which starts at index 7",
    "  local allProfiles = getRdbArray(auth, rdbPrefix, 7, 23, false, rdbMembers)",
    "  -- build a list of Ethernet WAN interface name",
    "  local wanList={}",
    "  for i=1, #interfaces do",
    "    if interfaces[i].mode == 'wan' then",
    "      table.insert(wanList, 'eth.' .. interfaces[i].id)",
    "    end",
    "  end",
    "  -- select profile that matches interface name in WAN list",
    "  local wanProfiles = {}",
    "  for i=1, #allProfiles do",
    "    if allProfiles[i].dev ~= '' then",
    "      for j=1, #wanList do",
    "        if wanList[j] == allProfiles[i].dev then",
    "          table.insert(wanProfiles, allProfiles[i])",
    "          break",
    "        end",
    "      end",
    "    end",
    "  end",
    "  -- ensure un-used ipv4 members has valid default value",
    "  for i=1, #wanProfiles do",
    "    if wanProfiles[i].conntype == 'dhcp' then",
    "      local ipvars={'iplocal', 'mask', 'gw', 'dns1', 'dns2'}",
    "      for j=1, #ipvars do",
    "        if wanProfiles[i][ipvars[j]] == '' then",
    "          local val = ipvars[j]=='mask' and '255.255.255.255' or '0.0.0.0'",
    "          wanProfiles[i][ipvars[j]] = val",
    "        end",
    "      end",
    "    end",
    "  end",
    "  return wanProfiles",
    "end"
   ]
};

// list of configuratble items for WAN interface
var wanProfiles=[];  // link profile for each WAN interface, fill by decodeRdb
var selectedWan=0;   // the WAN interface to be displayed, default show first one

// populate/refresh the UI data for selected WAN interface
function setEthWanCfgObj(){
  EthWanCfg.obj=wanProfiles[selectedWan];
  EthWanCfg.populate();
  setVisibility(wanProfiles[selectedWan].conntype == 'static');
  var el: any = document.getElementById("sel_"+wanSelect.memberName);
  el.value=selectedWan;
}

// hide/show items that are only applicable to selected option
function setVisibility(isVisible) {
  ["iplocal", "mask", "gw", "dns1", "dns2"].forEach(function(m){setVisible("#div_"+m, isVisible);});
}

// on connection type changes, hide or show relevant items
function onChangeConnType(o) { setVisibility(o.value == "static"); }

function onClickNatMasq(btn, v) {setToggle(btn, v);}

// build list options from all available WAN interfaces
function wanSelectOptions(obj) {
  var options=[];
  if (isDefined(wanProfiles)){
    for (var i = 0; i < wanProfiles.length; i++) {
      options.push([i, wanProfiles[i].dev]);
    }
  }
  return options;
}

// check if current display configuration has been modified
function isModified() {
  for (var i=0; i<EthWanCfg.members.length; i++) {
    var name=EthWanCfg.members[i].memberName;
    if (isDefined(EthWanCfg.obj[name])) {
      if (EthWanCfg.members[i].getVal() != EthWanCfg.obj[name]) {
        return true;
      }
    }
  }
  return false;
}

// refresh gui data when different WAN interface is selected
// if data has been modified, confirm with the user first
function onChangeWanIntf(_this)
{
  if (isModified()) {
    blockUI_confirm(_("discard confirm"),
      function(){selectedWan=_this.value; setEthWanCfgObj()},
      function(){
        var el: any = document.getElementById("sel_"+wanSelect.memberName);
        el.value=selectedWan
      }
    );
  } else {
    selectedWan=_this.value;
    setEthWanCfgObj();
  }
}

// variable to select which WAN interface configuration to be displayed
var wanSelect=selectVariable("wanSelect", "wan ethernet", wanSelectOptions, "onChangeWanIntf");

// main page object that handles ethernet WAN configuration
var EthWanCfg = PageObj("EthWanCfg", "wan configuration",
{
  customLua: customLuaGetEthWANLinkProfile,
  decodeRdb: function(objs) {
    wanProfiles = objs; // keep all profiles
    var selected=[];    // populate page object with the selected profile only
    if (wanProfiles.length > 0) {
      selectedWan=0;
      selected=wanProfiles[selectedWan];
      setEthWanCfgObj();
    }
    return selected;
  },
  encodeRdb: function(o) {
    // return current display profile in same format that was received at decodeRdb
    // merge value with gui data first
    for (var i=0; i<EthWanCfg.members.length; i++) {
      var name=EthWanCfg.members[i].memberName;
      if (isDefined(EthWanCfg.obj[name])) {
        if (EthWanCfg.members[i].getVal() !== wanProfiles[selectedWan][name]) {
          wanProfiles[selectedWan][name] = EthWanCfg.members[i].getVal();
        }
      }
    }
    return wanProfiles[selectedWan];
  },
  members: [
    wanSelect,
    selectVariable("conntype", "connection type",
      function(o){return [["static","static"],["dhcp","DHCP"]];}, "onChangeConnType"),
    toggleVariable("snat", "NatMasq", "onClickNatMasq"),
    editableIpAddressVariable("iplocal","ip address"),
    editableIpMaskVariable("mask","subnet mask"),
    editableIpAddressVariable("gw","gateway"),
    editableIpAddressVariable("dns1","dns1ip"),
    editableIpAddressVariable("dns2","dns2ip"),
    editableBoundedInteger("defaultroutemetric", "metric", 0, 65535, "Msg48", {helperText: ""}),
  ]
});

var pageData : PageDef = {
#ifndef V_MULTIPLE_LANWAN_UI
  onDevice : false,
#endif
  title: "LAN/WAN Switch",
  menuPos: ["Internet", "ETHWAN"],
  pageObjects: [EthWanCfg],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    // display an alert when no Ethernet WAN available.
    if (wanProfiles.length == 0) {
      displayAlert(_("wan configuration"), _("no wan interface is configured"));
      return;
    }
    $("#objouterwrapperEthWanCfg").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

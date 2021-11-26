/*
 * Wireless WAN link profile list for Saturn/Neptune
 *
 * Copyright (C) 2020 Casa Systems
 */

var NumProfiles = 6;

#ifdef COMPILE_WEBUI
var customLuaLinkProfile = {
  // there is outer rdb lock/unlock surrounding this block.
  // so disable locking for each of the following getRdbArray calls
  get: arr => [...arr, `
    o=getRdbArray(authenticated,'link.profile',1,${NumProfiles},false,{'name','apn','ip_handover.enable', 'vlan_index'},false)
    combineRdbArray(o, getRdbArray(authenticated,'link.policy',1,${NumProfiles},false,{'enable'},false))
    local vlans = getVlanList(true)
    for _, obj in ipairs(o) do
      obj['vlans'] = vlans
    end
  `],
  validate: arr => [...arr, `
    for _, obj in ipairs(o) do
      if not isValid.Key(obj['vlan_index'], getVlanList(true)) and obj['vlan_index'] ~= '' then
        return false, 'oops! ' .. 'vlan_index'
      end
    end
  `],
  set: function(arr) {
    // Set RDB variables only when changed its value
    // Set link.policy.x.trigger_connect to invoke interface up/down immediately
    arr.push("for _, obj in ipairs(o.objs) do");
    arr.push("  if obj.changed == '1' then");
    arr.push("    rdbName = 'link.profile.' .. obj.__id .. '.'");
    arr.push("    setRdb(rdbName .. 'ip_handover.enable', obj['ho_enable'])");
    arr.push("    setRdb(rdbName .. 'vlan_index', obj['vlan_index'])");
    arr.push("    local rdbName = 'link.policy.' .. obj.__id .. '.'");
    arr.push("    setRdb(rdbName .. 'enable', obj['enable'])");
    arr.push("    setRdb(rdbName .. 'trigger_connect', obj['enable'])");
    arr.push("  end");
    arr.push("end");
    return arr;
  }
};
#endif

function onClickEnable(togName, v) {
  var profId = getRowIndex(togName) + 1;
  var prof;

  LinkProfile.obj.some(function(obj){
    if (obj.__id === profId) {
      prof = obj;
      return true;
    }
  });
  if (togName.search("ho_enable") > 0) {
    prof.ho_enable = toBool(v) ? "1" : "0";
  } else {
    prof.enable = toBool(v) ? "1" : "0";
  }
}

class ApnTextVariable extends StaticTextVariable {
  setVal(obj: any, apn: string) {
    if (apn == "") {
      apn = htmlTag("span", {style:'font-style:italic'},_("blank"));
    }
    this.setHtml(apn);
  }
}

function vlanMappingSelectVariable(memberName: string, labelText: string, fnOptions: GetOptions, onChange: string) {
  var pe = new VlanMappingSelectVariable(memberName, labelText, fnOptions, onChange);
  pe.setEnable = (en: boolean) => {setEnable(pe.getHtmlId(), en);}
  return pe;
}

class VlanMappingSelectVariable extends SelectVariable {
  rowMemberId?: string;
  setVal(obj: any, val:string) { setOption(this.rowMemberId, this.fnOptions(obj), val);};
  getVal() { return String($("#sel_"+this.rowMemberId).val()); };
  genHtml() {
    this.rowMemberId = this.htmlId;
    this.htmlId = "sel_" + this.rowMemberId;
    var attr : any =  {id: this.htmlId, class:"select defaultWidth", onChange: this.onChange + "(this.id, this.value);"};
    return htmlTag("span",{class:"custom-dropdown"},htmlTag("select", attr, ""));
  };
}

class LinkVariable extends ShownVariable {
  genHtml() {
    var profId = getRowIndex(this.htmlId) + 1;
    return htmlTag("a",
                   {class: "secondary sml edit",
                    href: "/profile_settings.html?id=" + profId,
                    style: "padding:0;border:0;",
                    title: "Edit"},
                   htmlTag("i", {class: "icon edit"}, "")
                  );
  }
}

var profileRebootWarningHeader = "Adding or removing VLAN on profile no.1 will reboot your device.";
var profileRebootWarningParagraph = _("rebootWarning");

var LinkProfile = PageTableObj("LinkProfile", "Wireless WAN profile list",
{
    pageWarning: [profileRebootWarningHeader, profileRebootWarningParagraph],
#ifdef COMPILE_WEBUI
    customLua: customLuaLinkProfile,
#endif
    extraAttr: {
      tableAttr: {class:"above-5-column"},
      thAttr: [
        {class:"customTh field2"},
        {class:"customTh field1"},
        {class:"customTh field5"},
        {class:"customTh field5"},
        {class:"customTh field5"},
        {class:"customTh field5"},
        {class:"customTh field2"},
      ],
    },
    decodeRdb: (obj) => {
        var idx = 1;
        for (var prof of obj) {
            prof.index = idx++;
            prof.prevEnable = prof.enable;
            prof.ho_enable = prof["ip_handover.enable"];
            prof.prevHoEnable = prof.ho_enable;
            prof.prevVlanIdx = prof.vlan_index;
            prof.changed = "0";
        }
        return obj;
    },
    encodeRdb: (obj) => {
        for (var prof of obj) {
            if (prof.enable != "1") {
                prof.enable = "0";
            }
            if (prof.prevEnable != prof.enable) {
              prof.changed = "1";
            }
            if (prof.ho_enable != "1") {
                prof.ho_enable = "0";
            }
            if (prof.prevHoEnable != prof.ho_enable) {
              prof.changed = "1";
            }
            if (prof.prevVlanIdx != prof.vlan_index) {
              prof.changed = "1";
            }
        }
        return obj;
    },
#ifdef V_READONLY_WWAN_PROFILE_y
    readOnly: true,
#endif
    members: [
        staticTextVariable("index", "Profile no."),
        staticTextVariable("name", "Profile name"),
        toggleVariable("enable", "Status", "onClickEnable"),
        new ApnTextVariable("apn", "APN"),
        toggleVariable("ho_enable", "IP handover", "onClickEnable"),
        vlanMappingSelectVariable("vlan_index", "Map to LAN/VLAN",
                    (o) => {
                        let vlans: OptionsType[] = [["", "None"]];
                        if (isDefined(o) && o.vlans.constructor == Object) {
                            for (const ix in o.vlans) {
                                vlans.push([ix, o.vlans[ix]]);
                            }
                        }
                        return vlans;
                    }, "onChangeVlanMapping"),
        new LinkVariable("link", ""),
        hiddenVariable("prevEnable", ""),
        hiddenVariable("prevHoEnable", ""),
        hiddenVariable("prevVlanIdx", ""),
        hiddenVariable("changed", ""),
    ]
});

function onChangeVlanMapping(selHtmlId, selValue) {
  let profId = getRowIndex(selHtmlId) + 1;
  let prof;
  LinkProfile.obj.some(function(obj){
    if (obj.__id === profId) {
      prof = obj;
      return true;
    }
  });
  prof.vlan_index = String(selValue);
}

var pageData : PageDef = {
    title: "Wireless WAN profiles",
    menuPos: ["Networking", "WirelessWAN", "WWAN"],
    pageObjects: [LinkProfile],
    alertSuccessTxt : "You wireless WAN configuration changes have been successfully saved.",
#ifndef V_READONLY_WWAN_PROFILE_y
    onReady: () => {
        appendButtons({"save":"CSsave"});
        setButtonEvent("save", "click", sendObjects);
    }
#endif
};

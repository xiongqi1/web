/*
 * Wireless WAN link profile list for Saturn/Neptune
 *
 * Copyright (C) 2020 Casa Systems
 */

var NumProfiles = 6;

#ifdef COMPILE_WEBUI
var customLuaLinkProfile = {
  get: function(arr) {
    // there is outer rdb lock/unlock surrounding this block.
    // so disable locking for each of the following getRdbArray calls
    arr.push("o=getRdbArray(authenticated,'link.profile',1," + NumProfiles +
             ",false,{'name','enable','apn','user','defaultroute'},false)");
    return arr;
  },
  set: function(arr) {
    arr.push("setRdbArray(authenticated,'link.profile',{'enable','defaultroute','trigger'},o,false)");
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

  prof.enable = toBool(v) ? "1" : "0";
}

function onDefaultRoute(tName) {
  const profId = getRowIndex(tName) + 1;
  for (const prof of LinkProfile.obj) {
    if (prof.__id === profId) {
      prof.defaultroute = "1";
    } else {
      // only one profile can be the default route
      prof.defaultroute = "0";
      // clear all other radio buttons
      setRadioButtonVal("LinkProfile_" + (prof.__id - 1) + "_defaultroute", false);
    }
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

var LinkProfile = PageTableObj("LinkProfile", "Wireless WAN profile list",
{
#ifdef COMPILE_WEBUI
    customLua: customLuaLinkProfile,
#endif
    extraAttr: {
      tableAttr: {class:"above-5-column"},
      thAttr: [
        {class:"customTh field2"},
        {class:"customTh field1"},
        {class:"customTh field5", style:"padding-right:8px;"},
        {class:"customTh field7", style:"text-align:left;padding-left:10px;"},
        {class:"customTh field4", style:"text-align:left;padding-left:10px;"},
        {class:"customTh field2"},
        {class:"customTh field2"},
      ],
    },
    decodeRdb: (obj) => {
        var idx = 1;
        for (var prof of obj) {
            prof.index = idx++;
        }
        return obj;
    },
    encodeRdb: (obj) => {
        for (var prof of obj) {
            if (prof.enable != "1") {
                prof.enable = "0";
            }
            // always set trigger so that ppp scripts can be run
            prof.trigger = "1";
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
        staticTextVariable("user", "Username"),
        radioButtonVariable("defaultroute", "Default route", "onDefaultRoute"),
        new LinkVariable("link", ""),
    ]
});

var pageData : PageDef = {
    title: "Wireless WAN profiles",
    menuPos: ["Networking", "WirelessWAN", "WWAN"],
    pageObjects: [LinkProfile],
    alertSuccessTxt : "You wireless WAN configuration changes have been successfully saved.",
#ifndef V_READONLY_WWAN_PROFILE_y
    onReady: () => {
        appendButtons({"save":"CSsave"});
        setButtonEvent("save", "click", sendObjects);
#ifdef V_LOCK_IMS_SOS_PROFILE_y
      [1,2].forEach(function(line){
        //  2 enable, 5 defaultroute
        setToggleEnable(LinkProfile.objName + "_" + line + "_" + LinkProfile.members[2].memberName, false);
        setRadioButtonEnable(LinkProfile.objName + "_" + line + "_" + LinkProfile.members[5].memberName, false);
      });
#endif
    }
#endif
};

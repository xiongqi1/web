#ifndef V_TRANSPARENT_BRIDGE_PPOE_WEBUI_none
#if !defined(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y) && !defined(V_ROUTER_TERMINATED_PPPOE)
#if !defined(V_PRODUCT_hth_70) && !defined(V_WEBUI_TWEAK_fisher) && !defined(V_CBRS_SAS_y)
#define SHOW_PPPOE
#endif
#endif
#endif

#define SHOW_PROFILES

#ifndef V_ROAMING_SETTINGS_WEBUI_none
#if !defined(V_WEBIF_SPEC_vdf) && !defined(V_CUSTOM_FEATURE_PACK_Verizon_USA) && !defined(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y)
#if !defined(V_PRODUCT_hth_70) && !defined(V_WEBUI_TWEAK_fisher) && !defined(V_CBRS_SAS_y)
#define SHOW_ROAMING
#endif
#endif
#endif

var pageObjects = [];

#ifdef SHOW_PPPOE

// This replaces the auto stub because of the other objects it controls
function onClickPPPoEEnable(toggleName, v) {
  var enabled = toBool(v);

  function setPageVisibility() {
    PPPoE.setVisible(v);
    setVisible("#objouterwrapperLinkProfile", !enabled);
    setVisible("#objouterwrapperRoaming", !enabled);
    setVisible("#objouterwrapperBridgeCfg", enabled);
  }

  if(!isDefined(toggleName)) { // Called from the page onReady and not click
    setPageVisibility();
    return;
  }

  function confirmed() {
    PPPoE.obj.PPPoEEnable = v;
    setPageVisibility();
  }
  function notConfirmed() { setToggle(toggleName, '0');}

  if (enabled) {
     if (cntEnabledProfiles() >= 0) {
       blockUI_confirm(_("pppoeEnableWarningMsg"), confirmed, notConfirmed);
       return;
     }
  }
  confirmed();
}

#ifdef V_PRODUCT_vdf_nwl12
var intro = _("pppoeIntroduction 3gplus");
#else
var intro = _("pppoeIntroduction");
#endif

class IntroVariable extends ShownVariable {
  constructor(name: string, labelText: string, extras?: any) {
    super(name, labelText, extras);
    this.setIsVisible( function () {return toBool(PPPoE.obj.PPPoEEnable);})
  }
  genHtml() {
    return htmlDiv({class:"p-des-full-width"}, htmlTag("p", {}, intro));
  }
}

var PPPoE = PageObj("PPPoE", "dataConnection",
{
  members: [
#if defined(V_PRODUCT_hth_70) || defined(V_WEBUI_TWEAK_fisher) && !defined(V_CBRS_SAS_y)
    hiddenVariable("PPPoEEnable", "service.pppoe.server.0.enable"),
#else
    objVisibilityVariable("PPPoEEnable", "transparentBridgePPPoE").setRdb("service.pppoe.server.0.enable"),
#endif
    new IntroVariable("intro", "")
  ]
});

pageObjects.push(PPPoE);

var BridgeCfg = PageObj("BridgeCfg", "pppoeConfiguration",
{
  members: [
      editableTextVariable("apnName", "apnName", {})
        .setRdb("service.pppoe.server.0.apn").setEncode(true),
      editableTextVariable("serviceName", "serviceName", {})
        .setRdb("service.pppoe.server.0.service").setEncode(true)
  ]
});

pageObjects.push(BridgeCfg);

#endif //SHOW_PPPOE

#ifdef SHOW_PROFILES

#if defined(V_PRODUCT_hth_70)
var NumProfiles = 1
#else
var NumProfiles = 6
#endif

#ifndef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
#define SHOW_DEFAULT_PROFILE
#endif

var customLuaLinkProfile = {
  lockRdb : false,
  get: function() {
    return ["=getRdbArray(authenticated,rdbPrefix,1," + NumProfiles +
              ",false,{'name','defaultroute','enable','apn','user','autoapn'})"]
  },
  set: function() {
    return [
      "local members = {'defaultroute','enable'}",
      "local objs = o.objs",
#ifdef V_DIAL_ON_DEMAND
	    "local dodEn=getRdb('dialondemand.enable')",
	    "local dodProf=getRdb('dialondemand.profile')",
#endif
      "for _, ob in ipairs(objs) do",
#ifdef V_CUSTOM_FEATURE_PACK_Telstra_AU
			" if ob.enable == '1' then setRdb(rdbPrefix  .. '.' ..'ESM_reconnect_delay', '0' ) end",
#endif
#ifdef V_DIAL_ON_DEMAND
		  " if dodEn=='1' then",
			"  if ob.defaultroute=='1' then",
			"   if ob.id ~= dodProf then",
                        "     setRdb('dialondemand.profile',ob.id)",
                        "     setRdb('dialondemand.status','0')",
      "    end",
      "  end",
      " end",
#endif
      " for _, m in ipairs(members) do",
      "  local old = luardb.get(rdbPrefix  .. '.' .. m) or '' ",
      "  if old ~= ob[m] then ob.trigger = '1' break end",
      " end",
      "end",
      "members[#members+1]='trigger'",
      "local rc = setRdbArray(authenticated,rdbPrefix,members,o)",
      "return rc"
    ];
  },
  helpers: [
      "local rdbPrefix = 'link.profile'"
    ]
};

var emulatorEnabled = false;

function getMaxEnabled() {
  var v250_en = "<%if(rdb_exists('confv250.enable')) get_single_direct('confv250.enable');%>";
  var padd_en = "<%if(rdb_exists('confv250.padd_enable')) get_single_direct('confv250.padd_enable');%>";
  emulatorEnabled = toBool(v250_en) && !toBool(padd_en);

#if defined V_CUSTOM_FEATURE_PACK_Telstra_140 || defined V_CUSTOM_FEATURE_PACK_Telstra_145w || defined V_CUSTOM_FEATURE_PACK_Telstra_6200 || defined V_CUSTOM_FEATURE_PACK_Select_Solutions
  var plmn_mcc="get_single_direct('wwan.0.imsi.plmn_mcc');%>";
  var plmn_mnc="get_single_direct('wwan.0.imsi.plmn_mnc');%>";
  if (plmn_mcc == '505' && (plmn_mnc == '01' || plmn_mnc == '1'))
    return 1;
#endif
  var max_enabled_profiles="<%get_single_direct('wwan.0.max_sub_if');%>";
  if(max_enabled_profiles=="" || max_enabled_profiles=="N/A")
	  return 2;
  return Number(max_enabled_profiles);
}

var maxEnabledProfiles = getMaxEnabled();

function isProfileEnabled(obj) { return toBool(obj.enable);}
function isDefaultProfile(obj) { return toBool(obj.defaultroute);}

function cntEnabledProfiles() {
  var cnt = 0;
  LinkProfile.obj.forEach(function(obj){
    if (isProfileEnabled(obj)) {
      cnt++;
    }
  });
  return cnt;
}

function getDefaultProfile() {
  var def;
  LinkProfile.obj.some(function(obj){
    if (isDefaultProfile(obj)) {
      def = obj;
      return true;
    }
  });
  return def;
}

// btn is name of the the input field for the toggle
// the form LinkProfile_2_defaultroute_btn - zero based
function onClickDefault(btn) {
  function disableProfile(fields, obj) {
    fields[1] = (obj.id-1).toString();
    obj.enable = '0';
    setToggle([fields[0],fields[1],"enable"].join("_"),'0');
  }

  var fields = btn.split("_");
  fields.pop(); // Not interested in the last _btn
  var id = parseInt(fields[1]) + 1;
  var cntEnabled = cntEnabledProfiles();
  var def = getDefaultProfile();

  LinkProfile.obj.forEach(function(obj){
    fields[1] = (obj.id-1).toString();
    if (id === obj.id) {
      if (!isProfileEnabled(obj)){
        obj.enable = '1';
        cntEnabled++;
        setToggle([fields[0],fields[1],"enable"].join("_"),'1');
      }
      obj.defaultroute = '1';
    }
    else {
      obj.defaultroute = '0';
      setRadioButtonVal(fields.join("_"),'0');
    }
  });

  if ((cntEnabled > maxEnabledProfiles) || (cntEnabled > 0 && emulatorEnabled)) {

    // Too many enabled.
    // Disable the last default profile
    // or any other enabled profile (except the one we just set as default)
    if (isDefined(def)) {
      disableProfile(fields, def);
    }
    else {
      LinkProfile.obj.some(function(obj){
        if (isProfileEnabled(obj) && obj.id !== id) {
          disableProfile(fields, obj)
          return true;
        }
      });
    }
  }
}

// togName is name of the the input field for the toggle in the form LinkProfile_0_enable
// The toggle visuals are down outside this for normal operation
function onClickEnable(togName, v) {
  var fields = togName.split("_");
  fields[2] = "defaultroute";  // fields will be used to access the default button
  var id = parseInt(fields[1]) + 1;
  var prof;
  var cntEnabled = cntEnabledProfiles();

  LinkProfile.obj.some(function(obj){
    if (obj.id === id) {
      prof = obj;
      return true;
    }
  });

  var enabled = isProfileEnabled(prof);
  if(enabled === toBool(v)) {
    return;
  }
  if(enabled) {
    // We're disabling
    prof.enable = '0';
    if (isDefaultProfile(prof)) {

      fields[1] = (prof.id-1).toString();
      prof.defaultroute = '0';
      setRadioButtonVal(fields.join("_"),'0');

      // find another default
      LinkProfile.obj.some(function(obj){
        if (isProfileEnabled(obj)) {
          fields[1] = (obj.id-1).toString();
          obj.defaultroute = '1';
          setRadioButtonVal(fields.join("_"),'1');
          return true;
        }
      });
    }
  }
  else {
    // We're enabling
    if (cntEnabled > 0 && emulatorEnabled) {
      blockUI_alert(_("enableMultiProfileWarning1"));
      setToggle(togName,'0');
    }
    else if (cntEnabled >= maxEnabledProfiles) {
      blockUI_alert(_("maxEnabledProfilesExceeded"));
      setToggle(togName,'0');
    }
    else {
      prof.enable = '1';
      if (!isDefined(getDefaultProfile()) ) {
        prof.defaultroute = '1';
        setRadioButtonVal(fields.join("_"),'1');
      }
    }
  }
}

class ApnTextVariable extends StaticTextVariable {
  setVal(obj: any, apn: string) {
    var roam_simcard="<%get_single_direct('manualroam.custom_roam_simcard');%>";
    if(!toBool(roam_simcard) && toBool(obj.autoapn)) {
      apn = htmlTag("span", {style:'font-style:italic'},_("band automatic"));
    }
    else if (apn=="") {
      apn = htmlTag("span", {style:'font-style:italic'},_("blank"));
    }
    $("#" + this.htmlId).html(apn);
  }
}

class LinkVariable extends ShownVariable {
  genHtml() {
    var fields = this.htmlId.split("_");
    return htmlTag("a", {class: "secondary sml", href: "/Profile_Settings.html?" + fields[1]
#ifdef V_WEBIF_SPEC_vdf
      }, htmlTag("i", {class: "icon edit"}, _("edit")));
#else
      , style: "padding:0;border:0;", title: "Edit"}, htmlTag("i", {class: "icon edit"}, ""));
#endif
  }
}

var LinkProfile = PageTableObj("LinkProfile", "profileNameList",
{
  customLua: customLuaLinkProfile,
  decodeRdb: function(objs) {
    for (const i in objs) {
      if (objs[i].defaultroute == "1" && objs[i].enable == "0") {
        objs[i].defaultroute = "0";
      }
    }
    return objs;
  },

  colgroup: [
						"100px",
#ifdef SHOW_DEFAULT_PROFILE
						"60px", // This col for "default"
#endif
						"120px", "170px", "auto", "50px"
					],
  thAttr: [
						{},
#ifdef SHOW_DEFAULT_PROFILE
						{},
#endif
						{style:"padding-right:8px;"},
						{style:"text-align:left;padding-left:10px"},
						{style:"text-align:left;padding-left:10px"},
						{}
  ],
  members: [
    staticTextVariable("name", ""),
#ifdef SHOW_DEFAULT_PROFILE
    radioButtonVariable("defaultroute", "default", "onClickDefault"),
#endif
    toggleVariable("enable", "status", "onClickEnable"),
    new ApnTextVariable("apn", "apn"),
    staticTextVariable("user", "user"),
    new LinkVariable("link", ""),
  ]
});

pageObjects.push(LinkProfile);

#endif // SHOW_PROFILES

#ifdef V_MODULE_PRI_BASED_OPERATION_y

declare function rdb_tool(tok: string): void;

function onClickFactoryResetProfile() {

  var msl=DeactivateProfile.members[0].getVal();
  /* check msl code */
  if(msl=="") {
    blockUI_alert(_("msl code not input"));
    return;
  }

  blockUI_confirm(_("omadm deactivate confirm"), function(){
    blockUI_wait(_("GUI pleaseWait"));

    var rdb=new rdb_tool(csrfToken);
    rdb.reset();
    rdb.add("wwan.0.cdma.otasp.stat","");
    rdb.add("wwan.0.cdma.otasp.spc", msl);
    rdb.add("wwan.0.cdma.otasp.cmd","rtn");
    rdb.mset(function(res){
      /* wait until spc done */
      rdb.wait_for_rdb_result("wwan.0.cdma.otasp.stat", 60000, function(res){
        /* redirect to status page if success */
        var succ=res.match(/^\[done\].*/);
        if(succ) {
          setTimeout(function() { location.reload();}, 20000);
          return
        }

        /* unblock and show fail */
        $.unblockUI();
        blockUI_alert(_("omadm factory fail"));
      });
    });
  });
}

function onClickResetProfile() {
  var update_type = ActivateProfile.members[0].getVal();
  blockUI_confirm(_("omadm warning"), function(){
    blockUI_wait(_("GUI pleaseWait"));

    var rdb=new rdb_tool(csrfToken);
    rdb.reset();     /* start factory reset */
    rdb.add("wwan.0.cdma.otasp.stat","");
    rdb.add("wwan.0.cdma.otasp.cmd",update_type);
    rdb.mset(function(res){
      /* wait until spc done */
      rdb.wait_for_rdb_result("wwan.0.cdma.otasp.stat",60000,function(res){
        /* redirect to status page if success */
        var succ=res.match(/^\[done\].*/);
        if(succ) {
          location.reload();
          return;
        }
        /* unblock and show fail */
        $.unblockUI();
        blockUI_alert_l(_("omadm fail"), function() { location.reload();});
      });
    });
  });
}

if ("<%get_single_direct('link.profile.wwan.pri');%>".replace(/\(.*\)/,"")=="SPRINT") {
  var ActivateProfile = PageObj("ActivateProfile", "manual oma-dm",
  {
    readOnly: true,
    members: [
      selectVariable("update_type", "update type",
          function(obj){return [
                ["omadm-prl","omadm device configuration and prl"],
                ["omadm","omadm device configuration"],
                ["prl","omadm prl"]];
          }),
      buttonAction("reset_profile", "oma activate", "onClickResetProfile")
    ]
  });
  pageObjects.push(ActivateProfile);

  var DeactivateProfile = PageObj("DeactivateProfile", "module factory reset",
  {
    readOnly: true,
    members: [
      editableInteger("msl", "msl code").setMaxLength(6),
      buttonAction("freset_profile", "factory reset", "onClickFactoryResetProfile")
    ]
  });
  pageObjects.push(DeactivateProfile);
}

#endif

#ifdef SHOW_ROAMING

// btn is name of the the input field for the toggle
function onClickRoaming(btn,v) {
	setToggle(btn, v);
  if (toBool(v)) {
    Roaming.obj.roamingdatablocked = '';
    blockUI_alert(_("dataRoamAlert"));
  }
}

var Roaming = PageObj("Roaming", "roamingsettings",
{
  members: [
    toggleVariable("enable", "allowdataroaming","onClickRoaming").setRdb("roaming.data.en"),
    hiddenVariable("roamingdatablocked", "roaming.data.blocked"),
  ]
});

pageObjects.push(Roaming);

#endif // SHOW_ROAMING

function getSuccessText() {
#if !defined(V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y) && !defined(V_ROUTER_TERMINATED_PPPOE) && defined (SHOW_PPPOE)
  if (toBool(PPPoE.obj.PPPoEEnable)) {
    return"pppoeSubmitSuccess";
  }
#endif
  return "wwanSubmitSuccess";
}

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none
  onDevice : false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["Internet", "Profile_List"],
  pageObjects: pageObjects,
  alertSuccessTxt : getSuccessText,
  onReady: function (){
#if defined(V_CBRS_SAS_y)
    for (var i=0; i < NumProfiles; i++){
      setRadioButtonEnable(LinkProfile.objName + "_" + i + "_" + LinkProfile.members[1].memberName, false);
      setToggleEnable(LinkProfile.objName + "_" + i + "_" + LinkProfile.members[2].memberName, false);
    }
#else
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
#endif
#ifdef SHOW_PPPOE
    // This needs to be called explicitly because of the other objects it controls
    onClickPPPoEEnable(undefined, PPPoE.obj.PPPoEEnable);
#endif
#ifdef V_LOCK_IMS_SOS_PROFILE_y
    [1,2].forEach(function(line){
      var idx = 1;
#ifdef SHOW_DEFAULT_PROFILE
      setRadioButtonEnable(LinkProfile.objName + "_" + line + "_" + LinkProfile.members[idx].memberName, false);
      idx++;
#endif
      setToggleEnable(LinkProfile.objName + "_" + line + "_" + LinkProfile.members[idx].memberName, false);
    });
#endif
  }
}

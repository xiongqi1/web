var FwCfg = PageObj("FwCfg", "router firewall",
{
#ifdef V_NON_SECURE_WARNING_y
  // display a warning message if firewall gets disabled in the web UI
  setEnabled: function (enable){ if (enable===false) blockUI_alert(_("noFirewallWarning")); },
#endif
  members: [
    objVisibilityVariable("fwEnable", "enable router firewall").setRdb("admin.firewall.enable")
  ]
});

var pageData : PageDef = {
#if defined V_NETWORKING_UI_none
  onDevice : false,
#endif
  rootOnly: true,
  title: "Firewall",
  menuPos: ["Internet", "FIREWALL"],
  pageObjects: [FwCfg],
  alertSuccessTxt: "firewallSubmitSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

disablePageIfPppoeEnabled();

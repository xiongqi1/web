// ------------------------------------------------------------------------------------------------
// WWAN IP handover/pass-through settings
// Copyright (C) 2021 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

function check_range(lower, upper) {
    return _("value_must_be_between") + " " + lower.toString() + " " + _("and") + " " + upper.toString()
}

var IpHandover = PageObj("IpHandover", "IP Handover",
{
#if defined(V_CBRS_SAS_y)
  customLua: {
    set: function(setArray) {
      return setArray.concat(["if o.cfgEnable == '0' then setRdb('service.ip_handover.enable','0') end"]);
    }
  },
#endif
  members:[
#if defined(V_CBRS_SAS_y)
    objVisibilityVariable("cfgEnable", "Enable IP Handover").setRdb("sas.config.ip_handover"),
    staticTextVariable("profileIndex","Profile Index").setRdb("service.ip_handover.profile_index"),
#else
    objVisibilityVariable("cfgEnable", "Enable IP Handover").setRdb("service.ip_handover.enable"),
    editableBoundedInteger("profileIndex", "WWAN profile index", 1, 6, check_range(1, 6))
      .setRdb("service.ip_handover.profile_index")
      .setSmall(),
#endif
    editableIpAddressVariable("fakeIp", "Fake WWAN IP").setRdb("service.ip_handover.fake_wwan_address"),
    editableBoundedInteger("fakeMask", "Fake WWAN Mask", 1, 32, check_range(1, 32))
      .setRdb("service.ip_handover.fake_wwan_mask")
      .setSmall()
  ]
});

var pageData : PageDef = {
#if (!defined V_CUSTOM_FEATURE_PACK_myna_lite) && (!defined V_CUSTOM_FEATURE_PACK_myna)
  onDevice : false,
#endif
  title: "IP Handover",
  menuPos: ["Internet", "IpHandover"],
  pageObjects: [IpHandover],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#objouterwrapperIpHandover").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

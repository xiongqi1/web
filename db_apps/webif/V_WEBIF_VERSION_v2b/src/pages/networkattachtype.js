// ------------------------------------------------------------------------------------------------
// WAN network attach type setting
// Copyright (C) 2021 Casa Systems Inc.
// ------------------------------------------------------------------------------------------------

// network attach type setting
var NetworkAttachType = PageObj("NetworkAttachType", "Network Attach Type",
{
  rdbPrefix: "wmmd.config.",
  members:[
    selectVariable("attachType", "Attach type", function(o){
      return [
        ["CS_PS",_("Combined attach")],
        ["PS_ONLY",_("PS only")]
      ];})
    .setRdb("modem_service_domain")
  ]
});

var pageData : PageDef = {
#if (!defined V_CUSTOM_FEATURE_PACK_myna_lite) && (!defined V_CUSTOM_FEATURE_PACK_myna)
  onDevice : false,
#endif
  title: "Attach Type",
  menuPos: ["Internet", "AttachType"],
  pageObjects: [NetworkAttachType],
  validateOnload: false,
  alertSuccessTxt: "submitSuccess",
  onReady: function (){
    $("#objouterwrapperNetworkAttachType").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

var AutoDialCfg = PageObj("AutoDialCfg", "autodial menu title",
{
  members: [
    objVisibilityVariable("autodialEnable", "autodial enable menu").setRdb("autodial.enable"),
    editableTextVariable("autodialNumber", "autodial number menu").setRdb("autodial.dial_string")
  ]
});

var pageData: PageDef = {
#ifndef V_AUTODIAL
  onDevice : false,
#endif
  title: "Autodial",
  menuPos: ["Services", "AUTODIAL"],
  pageObjects: [AutoDialCfg],
  onReady: function (){
    $("#objwrapperAutoDialCfg").before(htmlTag("p", {}, _("autodial menu desc")));
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}

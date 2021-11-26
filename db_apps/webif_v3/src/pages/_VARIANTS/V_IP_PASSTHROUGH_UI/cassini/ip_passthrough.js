var IpPassthrough = PageObj("IpPassthrough", "IP Passthrough",
{
    members: [
        objVisibilityVariable("enable", "Enable").setRdb("service.ip_handover.enable")
    ],
});

var pageData : PageDef = {
    title: "IP passthrough",
    menuPos: ["Networking", "IpPassthrough"],
    pageObjects: [IpPassthrough],
    onReady: function () {
        appendButtons({"save":"CSsave"});
        setButtonEvent('save', 'click', sendObjects);
    }
};

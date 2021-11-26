var GattServer = PageObj("GattServer", "Aurora App (BT) Server",
{
    members: [
        noticeText("guide", _("Enables or disables Bluetooth applications access to data on this device.")),
        toggleVariable("gattServerEnable", "Enable", "").setRdb("service.gatt_server.enable")
    ],
});

var pageData : PageDef = {
    title: "Aurora App (BT) Server",
    menuPos: ["Services", "GattServer"],
    pageObjects: [GattServer],
    onReady: function () {
        appendButtons({"save":"CSsave"});
        setButtonEvent('save', 'click', sendObjects);
    }
};

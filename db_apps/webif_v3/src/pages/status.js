// default Status page, which should be overridden

var DeviceInfo = PageObj("deviceInfo", "Device information", {
    readOnly: true,
    members: [
      staticTextVariable("softwareVersion","Software version").setRdb("sw.version"),
      staticTextVariable("produceModel","Model").setRdb("system.product.model")
    ]
});

var pageData : PageDef = {
  title: "Status",
  menuPos: [ "Status" ],
  pageObjects: [DeviceInfo]
}

// default System status page

var DeviceInfo = PageObj("SystemStatusDeviceInfo", "Device information", {
  readOnly: true,
  members: [
    staticTextVariable("produceModel","Model").setRdb("system.product.model")
  ]
});

var SoftwareInfo = PageObj("SystemStatusSoftwareInfo", "Software information", {
  readOnly: true,
  members: [
    staticTextVariable("softwareVersion","Software version").setRdb("sw.version"),
    staticTextVariable("modemFirmware","Modem firmware").setRdb("wwan.0.firmware_version")
  ]
});

var pageData : PageDef = {
  title: "System status",
  menuPos: [ "System", "SystemStatus" ],
  pageObjects: [DeviceInfo, SoftwareInfo]
}

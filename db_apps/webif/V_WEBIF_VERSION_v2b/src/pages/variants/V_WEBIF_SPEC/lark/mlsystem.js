//Copyright (C) 2019 NetComm Wireless Limited.

const getStorageSize = "df -Ph /dev/mmcblk0p1 | grep /dev/mmcblk0p1 | grep -Eo [0-9\\.]+[MG] | head -n1";
const getMacAddress = `ubus call network.device status '{"name": "wlan0"}' | grep macaddr | cut -d'"' -f4`;

var po = PageObj("systemInfo", "System information",
{
  readOnly: true,
  members: [
    staticTextVariable("hw_model", "Device model").setRdb("system.product.model"),
    staticTextVariable("sn", "Serial number").setRdb("system.product.sn"),
    staticTextVariable("hw_revision", "Hardware version").setRdb("system.product.hwver"),
    staticTextVariable("fw_revision", "Firmware version").setRdb("sw.version"),
    staticTextVariable("storage_size", "Storage size").setShell(getStorageSize, null, false),
    staticTextVariable("mac", "MAC address").setShell(getMacAddress, null, false),
    staticTextVariable("ssid", "SSID").setUci("wireless.ap.ssid")
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "System",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["NIT", "System"]
}

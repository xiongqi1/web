//Copyright (C) 2019 NetComm Wireless Limited.

#if 0
// each /mnt/emmc/firmware/[0-9]+.txt stores meta information of an OWA firmware
#endif

const getNumOfFirmwares = "ls -l /mnt/emmc/firmware/*.txt | grep -E /[0-9]+\\.txt | wc -l";
const getFreeStorage = "df -Ph /dev/mmcblk0p1 | grep /dev/mmcblk0p1 | grep -Eo [0-9\\.]+[MG] | tail -n1";
const getBatteryLevel = "rdb_get system.battery.capacity | sed s/$/%/";

var po = PageObj("systemStatus", "System status",
{
  readOnly: true,
  members: [
    staticTextVariable("firmware_num", "Number of OWA firmware").setShell(getNumOfFirmwares, null, false),
    staticTextVariable("free_storage", "Free storage").setShell(getFreeStorage, null, false),
    staticTextVariable("battery_level", "Battery level").setShell(getBatteryLevel, null, true),
    staticTextVariable("gradvar_date", "World Magnetic Model data release date").setRdb("sw.wmm.version")
  ]
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Status",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["NIT", "Status"]
}

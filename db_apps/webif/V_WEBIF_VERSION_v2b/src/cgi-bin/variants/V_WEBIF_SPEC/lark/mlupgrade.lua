--Copyright (C) 2019 NetComm Wireless Limited.

local function upgradeFirmware()
  print("flashing firmware/uboot " .. uploaded_file)

  local messages = {}
  local delay = 0
  local extra_param = ""

  messages.disconnect = "disconnectOWAPriorToUpgrading"
  messages.in_progress = "upgradingInProgress"
  messages.connect_wifi = "pleaseConnectDeviceWifi"

  if "firmware" == uploaded_target then
    messages.invalid = "invalidFirmware"
    messages.uploaded = "firmwareUploaded"
    messages.upgraded = "firmwareUpgraded"
    delay = 300
  else
    messages.invalid = "invalidBootLoader"
    messages.uploaded = "bootLoaderUploaded"
    messages.upgraded = "bootLoaderUpgraded"
    delay = 115
    extra_param = "--boot-loader "
  end

  local ret = os.execute("sysupgrade_star.sh --verify-only " .. extra_param .. uploaded_file)
  local status = luardb.get("install_tool.mode")
  local upgradeable = (status == "battery" or status == "calibration")

  if 0 ~= ret then
    processed_message = messages.invalid
  elseif not upgradeable then
    processed_message = messages.disconnect
  elseif not uploaded_commit then
    processed_message = messages.uploaded
  end

  if upgradeable and 0 == ret and uploaded_commit then
    os.execute("sysupgrade_star.sh --skip-verify " .. extra_param .. uploaded_file .. "&")
    processed_message = messages.in_progress
    extra_messages = messages.connect_wifi .. ";" .. messages.upgraded
    ping_delay = delay
    ping_url = "/"

    luardb.set("install_tool.update", "start")
  end

end

if "firmware" == uploaded_target or "uboot" == uploaded_target then
  local power_level = tonumber(luardb.get("system.battery.capacity")) or 0

  if 10 > power_level then
    processed_message = "batteryLevelTooLow"
  else
    upgradeFirmware()
  end

end

if "gridvar" == uploaded_target then
  print("upgrade grid variation coefficient file " .. uploaded_file)

  local ret = os.execute("mv " .. uploaded_file .. " /mnt/emmc/WMM.COF && /etc/init.d/lark_sensors restart")

  if 0 == ret then
    processed_message = "coefficientUpdated"
  else
    processed_message = "coefficientCantBeUpdated"
  end
end

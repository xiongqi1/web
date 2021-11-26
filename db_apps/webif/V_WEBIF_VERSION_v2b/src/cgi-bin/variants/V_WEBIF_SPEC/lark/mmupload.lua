--Copyright (C) 2019 NetComm Wireless Limited.
local lfs = require "lfs"
local firmware_path = "/mnt/emmc/firmware/"

-- Find the next available index number for storing firmware
local function findIndex()
  local index = 1

  for file in lfs.dir(firmware_path) do
    local match = string.match(file, "([0-9]+).txt")

    if nil ~= match then
      match = tonumber(match)
      if(match >= index) then
        index = match + 1
      end
    end

  end

  return index
end

-- Store firmware and extract meta information for web UI presenting
local function storeFirmware(ufile, index, firmware_type)
  local dest = firmware_path .. index .. ".star"
  local meta = firmware_path .. index .. ".txt"

  local ret = os.execute("mv " .. ufile .. " " .. dest)
  if 0 ~= ret then
    processed_message = "failedToStoreTheFirmware"
    return
  end

  local size = lfs.attributes (dest, "size")/1024/1024
  size = string.format("%.1f MB", size)


  local cmd = "star " .. dest .. " -M fw.version fw.date > " .. meta .. " && " ..
              "echo '" .. size .."' >> " .. meta  .." && " ..
              "star " .. dest .. " -M fw.variants >> " .. meta .. " && " ..
              "echo '" .. firmware_type .."' >> " .. meta

  local ret = os.execute(cmd)
  if 0 ~= ret then
    processed_message = "invalidFirmware"
    os.remove(dest)
    os.remove(meta)
    return
  end
  if "engineering" == firmware_type then
    processed_message = "engineeringFirmwareUploaded"
  else
    processed_message = "firmwareUploaded"
  end

end

local function processFirmware (ufile)
  local file = io.popen("df -Pm /dev/mmcblk0p1 | grep /dev/mmcblk0p1 | grep -Eo ' [0-9]+ ' | tail -n1")
  local free_size = tonumber(file:read("*a")) or 0

  if 200 > free_size then
    processed_message = "internalFreeStorageTooLow"
    return
  end

  print("verifying magpie firmware " .. ufile)

  -- Try production keys first
  local ret = os.execute("sysupgrade_star.sh --verify-only " .. ufile)
  local firmware_type = "production"

  if 0 ~= ret then
    -- Try engineering keys
    local ret = os.execute("sysupgrade_star.sh --verify-only --eng-key " .. ufile)

    if 0 ~= ret then
      processed_message = "failedToVerifyTheFirmware"
      os.remove(ufile)
      return
    else
      firmware_type = "engineering"
    end
  end

  local index = findIndex()
  storeFirmware(ufile, index, firmware_type)
end

if "firmware" == uploaded_target then
  processFirmware(uploaded_file)
  os.remove(uploaded_file)
end

--Copyright (C) 2020 Casa System.
local lfs = require "lfs"
local store_path = "/mnt/emmc/rtconfig/"

-- Find the next available index number for storing configuration
local function findIndex()
  local index = 1

  for file in lfs.dir(store_path) do
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

-- Store configuration and extract meta information for web UI presenting
local function storeConfiguration(ufile, index, config_type)
  local dest = store_path .. index .. ".star"
  local meta = store_path .. index .. ".txt"

  local ret = os.execute("mv " .. ufile .. " " .. dest)
  if 0 ~= ret then
    processed_message = "failedToStoreTheConfig"
    return
  end

  local cmd = "star " .. dest .. " -M config_id | tr ',' '\\n'> " .. meta .. " && " ..
              "echo '" .. config_type .. "' >> " .. meta

  local ret = os.execute(cmd)
  if 0 ~= ret then
    processed_message = "invalidConfig"
    os.remove(dest)
    os.remove(meta)
    return
  end
  if "engineering" == config_type then
    processed_message = "engineeringConfigUploaded"
  else
    processed_message = "configUploaded"
  end

end

local function processConfiguration(ufile)
  local file = io.popen(string.format("df -Pm %s | grep -Eo ' [0-9]+ ' | tail -n1", store_path))
  local free_size = tonumber(file:read("*a")) or 0

  if 2 > free_size then
    processed_message = "internalFreeStorageTooLow"
    return
  end

  print("verifying runtime configuration file " .. ufile)

  -- Try production keys first
  local ret = os.execute("sysupgrade_star.sh --verify-only " .. ufile)
  local config_type = "production"

  if 0 ~= ret then
    -- Try engineering keys
    local ret = os.execute("sysupgrade_star.sh --verify-only --eng-key " .. ufile)

    if 0 ~= ret then
      processed_message = "failedToVerifyTheConfig"
      return
    else
      config_type = "engineering"
    end
  end

  local cmd =  "star " .. ufile .. " -M config_id | egrep [^,]+,[^,]+,[^,]+,[^,]+,[^,]+,[^,]+ "
  ret = os.execute(cmd)
  if 0 ~= ret then
    processed_message = "invalidConfig"
    return
  end

  if not os.rename(store_path, store_path) then
    lfs.mkdir(store_path)
  end

  local index = findIndex()
  storeConfiguration(ufile, index, config_type)
end

if "rtconfig" == uploaded_target then
  processConfiguration(uploaded_file)
  os.remove(uploaded_file)
end

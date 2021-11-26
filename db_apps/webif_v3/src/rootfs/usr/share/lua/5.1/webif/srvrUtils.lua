local mime = require("mime")
local bit = require("bit")
local sockUrl = require("socket.url")
require('stringutil')

function encode(data)
  if data then
    return mime.b64(data)
  end
  return data
end

function decode(data)
  if data and #data % 4 == 0 and data:match("^[A-Za-z0-9+/]+=*$") then
    return mime.unb64(data) or ''
  end
  return ''
end

function setRdb(key,value,volatile)
  if value then
    if not volatile then
      luardb.set(key, value, 'p')
    else
      luardb.set(key, value)
    end
  end
end

function getRdb(key)
  return luardb.get(key) or ''
end

-- TODO Not used yet finish when required
function setObjInstance(clsName, value)
  luardb.lock()
  luardb.unlock()
end


-- This returns an array from RdbOjects
-- This is not currently used as we don't have many RdbObjects in our system
-- More typically the data is just an array and the net function handles them
-- Args
--   clsName is the name of the RdbObject
-- Returns
--  array of objects from the RDB
function getObjs(clsName)
  local class = rdbobject.getClass(clsName)
  local instances = class:getAll()
  objs = {}
  luardb.lock()
  for _, instance in ipairs(instances) do
    local id = class:getId(instance)
    local iter = class:iterator(instance)
    local obj = {}
    obj.__id = id
    for k, v in iter do
        obj[k] = v
    end
    table.insert(objs, obj)
  end
  luardb.unlock()
  return objs
end

-- This returns an array of objects from the RDB
-- Args
--   authenticated - true if the session is logged in
--   clsName is the name of the Rdb Array
--   lower is the first index
--   upper is the last index
--   untilEmpty is a bool that indicates the returned array
--      can be shorter if there is no data in the RDB
--   members is an array of rdb member names
--   lockRdb - whether to lock/unlock RDB before/after reading
-- Returns
--  array of objects from the RDB
-- Example getRdbArray('link.profile',1,6,{'name','enable'})))

function getRdbArray(authenticated, clsName, lower, upper, untilEmpty, members, lockRdb)
  if lockRdb ~= false then lockRdb = true end -- default to lock RDB
  local objs = {}
  local idx = 1
  if lockRdb then luardb.lock() end
  for id = lower, upper do
    local obj = {}
    local rdbPre = clsName .. '.' .. id
    obj.__id = id

    for _, v in ipairs(members) do
      local member = v
      local data
      if v == '' then
        member = 'rawdata'
       data = luardb.get(rdbPre)
      else
        data = luardb.get(rdbPre  .. '.' .. v)
      end
      if untilEmpty == true and (data == nil or data == '') then
        if lockRdb then luardb.unlock() end
        return objs
      end
      obj[member] = data  or ''
    end
    objs[idx] = obj
    idx = idx + 1
  end
  if lockRdb then luardb.unlock() end
  return objs
end

-- This writes an array of objects to the RDB
-- Args
--   authenticated - true if the session is logged in
--   clsName is the name of the Rdb Array
--   members is an array of rdb member names
--     the array index is actually from the objects themselves (obj.__id)
--   value is the array of objects
--   lockRdb - whether to lock/unlock RDB before/after writing
-- Returns
-- Example setRdbArray(true,'link.profile',{'name','enable'},array)
--
-- If obj.__deleted flag is set then delete the RDB array instance physically
-- to allow index hole.
function setRdbArray(authenticated, clsName, members, value, lockRdb)
  if lockRdb ~= false then lockRdb = true end -- default to lock RDB
  local objs = value.objs
  if lockRdb then luardb.lock() end

  for _, obj in ipairs(objs) do
    local rdbName = clsName .. '.' .. obj.__id
    for _, mem in ipairs(members) do
      if mem == '' then
        if obj.__deleted then
          luardb.unset(rdbName)
        else
          setRdb(rdbName, obj.rawdata)
        end
      else
        if obj.__deleted then
          luardb.unset(rdbName .. '.' .. mem)
        else
          setRdb(rdbName .. '.' .. mem, obj[mem])
        end
      end
    end
  end
  if lockRdb then luardb.unlock() end
end

-- combine two RDB arrays into one
-- @param arr1 The first RDB array, into which the second one will be merged
-- @param arr2 The second RDB array that will be merged into the first one
-- @note The two RDB arrays must have the same size and ordering of elements
function combineRdbArray(arr1, arr2)
  for idx, obj in ipairs(arr1) do
    if obj.__id == arr2[idx].__id then
      for k,v in pairs(arr2[idx]) do
        if k ~= '__id' then
          obj[k] = v
        end
      end
    end
  end
end

-- This function executes a command and returns and array of lines returned.
-- The notification of this data is left to the caller
function executeCommand(cmd)
  local lines = {}
  local stdout = io.popen(cmd, "r")
  for line in stdout:lines() do
      table.insert(lines, line)
  end
  stdout:close()
  return lines
end

-- This function does an ls command with the provided filespec and returns and array of lines returned.
-- The nicification of this data is left to the caller
-- This is called by the fileUploader page element to return file status - loaded/ not uploaded
function getFiles(filespec)
  return executeCommand('ls -R1l '..filespec)
end

-- return true if val is valid
-- val may be undefined
isValid = {}
function isValid.Hex(val)
  if val and val:match("^[%x]+$") then
    return true
  end
  return false
end

function isValid.PrintableAscii(val)
  if val and val:match("^[a-zA-Z0-9!\"#$%%&'()*+,%-%./:;<=>?@%[\\%]^_`{|}~]+$") then
    return true
  end
  return false
end

-- Expand Hostname regex with ':', '/', '[', ']' for URL
function isValid.URL(val)
  parsedUrl = sockUrl.parse(val)
  if parsedUrl and parsedUrl.scheme and parsedUrl.host and
    parsedUrl.host:match("^[a-zA-Z0-9][a-zA-Z0-9%-_.:/%[%]]*[a-zA-Z0-9/%]]$") then
    return true
  end
  return false
end

-- RFC 1123 say we can start with digits
function isValid.Hostname(val)
  if val and (#val > 0) and (
    val:match("^[a-zA-Z]$") or
    val:match("^[a-zA-Z0-9][a-zA-Z0-9%-_.]*[a-zA-Z0-9]$")
  ) then
    return true
  end
  return false
end

function isValid.Username(val)
  return isValid.PrintableAscii(val) and
    --from the browser code var unsafeString = "\"<>%\\^[]`\+\$\,='#&:\t";
    not val:match("[$^\\<>%%\"%[%]%+%,`='#&:]")
end

require('validateEmail')

function isValid.EmailAddress(val)
  local res, err = validateEmailAddress(val)
  if res == true then return true end
  return false
end

function isValid.UserOrEmail(val)
   return isValid.Username(val) or isValid.EmailAddress(val)
end

function isValid.IpNumber(v)
  local res = false
  if v ~= nil then
    res = (1*v < 256) and (1*v >= 0)
  end
  return res
end

function getOctets(val)
  return val:match("(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)")
end

function OctetsToInt(o1,o2,o3,o4)
  return bit.lshift(o1,24)+bit.lshift(o2,16)+bit.lshift(o3,8)+o4
end

function isValid.IpAddress(val)
  if val == nil then return false end
  local o1,o2,o3,o4 = getOctets(val)
  return isValid.IpNumber(o1) and isValid.IpNumber(o2) and isValid.IpNumber(o3) and isValid.IpNumber(o4)
end

-- This function was lifted from https://stackoverflow.com/questions/10975935/lua-function-check-if-ipv4-or-ipv6-or-string
-- Some enhancements to handle mixed like 2001::127.0.0.1
local R = {ERROR = 0, IPV4 = 1, IPV6 = 2, STRING = 3}
function GetIPType(ip, check6)

  if type(ip) ~= "string" then return R.ERROR end

  -- check for format 1.11.111.111 for ipv4
  local chunks = { ip:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$") }
  if (#chunks == 4) then
    for _,v in pairs(chunks) do
      if tonumber(v) > 255 then return R.STRING end
    end
    return R.IPV4
  end

  if check6 == false then
    return  R.STRING
  end
  -- check for ipv6 format, should be max 8 'chunks' of numbers/letters
  local addr = ip:match("^([a-fA-F0-9:%.]+)$")
  if addr ~= nil and #addr > 1 then
    -- address part
    local nc, dc = 0, false      -- chunk count, double colon
    for chunk, colons in addr:gmatch("([^:]*)(:*)") do
      if nc > (dc and 7 or 8) then return R.STRING end    -- max allowed chunks
      if #colons == 0 and GetIPType(chunk, false) == R.IPV4 then
        return R.IPV6
      end
      if #chunk > 0 then
        local hextext = tonumber(chunk, 16)
        if hextext == nil or hextext > 65535 then
          return R.STRING
        end
      end
      if #colons > 0 then
        -- max consecutive colons allowed: 2
        if #colons > 2 then return R.STRING end
        -- double colon shall appear only once
        if #colons == 2 and dc == true then return R.STRING end
        if #colons == 2 and dc == false then dc = true end
      end
      nc = nc + 1
    end
    return R.IPV6
  end
end

function isValid.Ipv6Address(val)
  return GetIPType(val, true) == R.IPV6;
end

function isValid.IpMask(val)
  return isValid.IpAddress(val)
end

function isValid.IpWithinRange(val,addr,mask)
  val = OctetsToInt(getOctets(val))
  addr = OctetsToInt(getOctets(addr))
  mask = OctetsToInt(getOctets(mask))
  local lowest = bit.band(addr,mask)
  local highest = bit.bor(addr,bit.bnot(mask))
  return (val >= lowest) and (val <= highest)
end

function isValid.VlanIpAddress(val)
  local res = isValid.IpAddress(val)
  if res then
    -- check if VLAN ip has same network segment with LAN
    lanIpAddress = luardb.get("link.profile.0.address")
    lanNetmask = luardb.get("link.profile.0.netmask")
    -- return false if has same range
    res = not isValid.IpWithinRange(val, lanIpAddress, lanNetmask)
  end

  return res
 end

function isValid.Bool(val)
  return val == 0 or val == 1 or val == '0' or val == '1'
end

function isValid.BoolOrEmpty(val)
  return val == '' or val == 0 or val == 1 or val == '0' or val == '1'
end

function toBool(val)
  return val == 1 or val == '1'
end

function isValid.Enum(val,enumArray)
  for _, d in ipairs(enumArray) do
    if val == d then
      return true
    end
  end
  return false
end

-- Check if a key is in a dictionary
--
-- @param key The key to be checked
-- @param dict The dictionary as a table
-- @return true if key exists in dict; false otherwise
function isValid.Key(key, dict)
  return dict[key] ~= nil
end

-- Only accepts unsigned integers (digits only)
function isValid.Integer(val)
  return (val~=nil) and (val:match("^[%d]+$")~=nil)
end

function isValid.BoundedInteger(val, lower, upper)
  return isValid.Integer(val) and (1*val >= lower) and (1*val <= upper)
end

-- Accept an optional minus sign before a string of digits.
function isValid.SignedInteger(val)
  return (val~=nil) and (val:match("^-?[%d]+$")~=nil)
end

function isValid.BoundedSignedInteger(val, lower, upper)
  return isValid.SignedInteger(val) and (1*val >= lower) and (1*val <= upper)
end

-- Only accepts unsigned float (digits only)
function isValid.Float(val)
  return (val~=nil) and (val:match("^[%d.]+$")~=nil)
end

function isValid.BoundedFloat(val, lower, upper)
  return isValid.Float(val) and (1.0*val >= lower) and (1.0*val <= upper)
end

-- Accept an optional minus sign before a string of digits.
function isValid.SignedFloat(val)
  return (val~=nil) and (val:match("^-?[%d.]+$")~=nil)
end

function isValid.BoundedSignedFloat(val, lower, upper)
  return isValid.SignedFloat(val) and (1.0*val >= lower) and (1.0*val <= upper)
end

-- validate hostname and optional 16-bit value port number
function isValid.HostnameWPort(v)
  local p = v:find(':')
  if p == nil then
    return isValid.Hostname(v)
  end
  local host = v:sub(1,p-1)
  local port = v:sub(p+1)
  return isValid.Hostname(host) and isValid.BoundedInteger(port,1,65535)
end

function shellExecute(command, unlock_rdb)
  if(unlock_rdb) then
    luardb.unlock()
  end

  local handle = io.popen(command)
  local result = handle:read("*a")
  handle:close()

  if(unlock_rdb) then
    luardb.lock()
  end

  return result
end

function getUci(uci)
  local command = "uci get " .. uci
  return shellExecute(command, false)
end

function setUci(uci, value)
  local command = "uci set " .. uci .. "=" .. vvalue
  os.execute(command)
end

-- get first IP address (v4 and v6) of given interface
-- @param inf Network interface
-- @ret table { up = true/false, ipv4 = IPv4 address, ipv6 = IPv6 address}
function getIpOfInterface(inf)
  local res = executeCommand("ip addr show dev "..inf)
  local infUp
  local ipv4
  local ipv6
  for i = 1, #res do
    if not infUp then
      infUp = res[i]:match("^%d+:%s"..inf..":.+[<,]UP[,>]")
    else
      -- first one only
      ipv4 = ipv4 or res[i]:match("^%s+inet ([0-9./]+).* scope global")
      ipv6 = ipv6 or res[i]:match("^%s+inet6 ([A-Fa-f0-9:/]+).* scope global")
    end
  end

  local ret = {
    up = infUp and true or false,
    ipv4 = ipv4,
    ipv6 = ipv6
  }
  return ret
end

-- Get all network interface names
-- @param up Whether to only include interfaces that are UP
-- @return An array of strings of network interface names
function getNetIfaces(up)
  local ifaces = {}
  local lines = executeCommand(string.format("ip link show %s", up and "up" or ""))
  for _, line in ipairs(lines) do
    local iface = line:match("^%d+:%s*([%w._]+)")
    if iface then
      ifaces[#ifaces + 1] = iface
    end
  end
  return ifaces
end

-- Get a list of all LAN/VLAN interfaces
-- @param enabledOnly Whether to only include interfaces that are enabled
-- @return A dictionary of {vlan_index: vlan interface name}, both key and value are strings
function getVlanList(enabledOnly)
  local vlans = {}
  local iface = luardb.get("link.profile.0.dev")
  if not iface or iface == "" then
    iface = "eth0"
  else
    iface = iface:gsub("%.", "")
  end
  vlans["-1"] = iface
  local num = tonumber(luardb.get("vlan.num")) or 0
  local en = luardb.get("vlan.enable") == "1"
  if num <= 0 then
    return vlans
  end
  if enabledOnly and not en then
    return vlans
  end
  for ix = 0, num - 1 do
    local prefix = string.format("vlan.%d.", ix)
    if not enabledOnly or luardb.get(prefix .. "enable") == "1" then
      -- try human friendly name first
      iface = luardb.get(prefix .. "rule_name")
      if not iface or iface == '' then
        -- use interface name as a backup
        iface = luardb.get(prefix .. "name")
      end
      if iface and iface ~= "" then
        vlans[tostring(ix)] = iface
      end
    end
  end
  return vlans
end

-- check if a daemon is running
-- @param pidfile The path to the pid file of the daemon
-- @return true if daemon is running; false if not
function isDaemonRunning(pidfile)
  local cmd = string.format("start-stop-daemon -K -q -p %s -t", pidfile)
  local status = os.execute(cmd)
  return status == 0
end

-- start a daemon
-- @param program The path of the daemon executable
-- @param pidfile The path of the pid file
-- @param arguments Arguments to be passed to the daemon command line
-- @return true on success; false on failure
function startDaemon(program, pidfile, arguments)
  local cmd = string.format("start-stop-daemon -S -b -q -m -p %s -x %s", pidfile, program)
  if arguments and #arguments > 0 then
    cmd = string.format("%s -- %s", cmd, arguments)
  end
  local status = os.execute(cmd)
  return status == 0
end

-- stop a daemon
-- @param pidfile The path of the pid file
-- @return true on success; false on failure
function stopDaemon(pidfile)
  local cmd = string.format("start-stop-daemon -K -q -p %s", pidfile)
  local status = os.execute(cmd)
  if status == 0 then
    os.execute(string.format("rm -f %s", pidfile))
  end
  return status == 0
end

-- Get default RDB value from the given default config file
-- @param confFile Default config file name in full path
-- @param var RDB variable name
-- @return A default LAN IP address
function getDefConfVal(confFile, var)
  local file = io.open(confFile, "r")
  if not file then return nil end
  local conf = file:read "*a"
  file:close()
  local defVal
  local st = 0, ed, val
  while true do
    st,ed,val = string.find(conf, var..";%d;%d;%d;%d+;([%d%.]+)", st+1)
    if st == nil then break end
    defVal = val
  end
  return defVal
end

-- Check if the file exists
-- @param name A file name to check
function file_exists(name)
   local f = io.open(name, "r")
   return f ~= nil and io.close(f)
end

-- Check if IP passthrough is active.
function isIpPassThroughActive()
    local globalIpPassThroughEnabled = luardb.get("service.ip_handover.enable")
    if globalIpPassThroughEnabled ~= "1" then return false end
    -- find default route profile
    local defaultRouteIdx = nil
    for i = 1, 6 do
        local defaultRoute = luardb.get("link.profile." .. i.. ".defaultroute")
        if defaultRoute == "1" then
            defaultRouteIdx = i
            break
        end
    end
    if not defaultRouteIdx then return false end
    profileIpPassThroughEnabled = luardb.get("link.profile." ..defaultRouteIdx.. ".ip_handover.enable")
    return profileIpPassThroughEnabled == "1"
end

-- Get the default lan rdbs for redirection after reset to default
-- @param level The reset level, e.g. 'carrier'
function getDefConfLanRdbs(level)
  local configIds = executeCommand('environment -r CONFIG_RDB')
  local defLanIpAddr, defHttpEn, defHttpsEn, defHttpsWebServerEn, configPath, configDir
  if #configIds > 0 and configIds[1] ~= '' then
    configIds = configIds[1]:explode(',')
    configDir = '/persist/configs/'..level..'/rdb/'
    for i = #configIds, 1, -1 do
      configPath = configDir..configIds[i]..'/default.conf'
      defLanIpAddr = defLanIpAddr or getDefConfVal(configPath, 'link.profile.0.address')
      defHttpEn = defHttpEn or getDefConfVal(configPath, 'admin.local.enable_http')
      defHttpsEn = defHttpsEn or getDefConfVal(configPath, 'admin.local.enable_https')
      defHttpsWebServerEn = defHttpsWebServerEn or getDefConfVal(configPath, 'service.webserver.https_enabled')
      if defLanIpAddr and defHttpEn and defHttpsEn and defHttpsWebServerEn then
        return defLanIpAddr, defHttpEn, defHttpEn, defHttpsWebServerEn
      end
    end
  end
  configPath = '/etc/cdcs/conf/default.conf'
  defLanIpAddr = defLanIpAddr or getDefConfVal(configPath, 'link.profile.0.address')
  defHttpEn = defHttpEn or getDefConfVal(configPath, 'admin.local.enable_http')
  defHttpsEn = defHttpsEn or getDefConfVal(configPath, 'admin.local.enable_https')
  defHttpsWebServerEn = defHttpsWebServerEn or getDefConfVal(configPath, 'service.webserver.https_enabled')
  return defLanIpAddr, defHttpEn, defHttpEn, defHttpsWebServerEn
end

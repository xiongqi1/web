local mime = require("mime")
local bit = require("bit")

function encode(data)
  if data then
    return mime.b64(data)
  end
  return data
end

function decode(data)
  if data then
    return mime.unb64(data) or ''
  end
  return ''
end

function setRdb(key,value)
  if value then
    luardb.set(key, value, 'p')
  end
end

function getRdb(key)
  return luardb.get(key) or ''
end

function file_exists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
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
    obj.id = id
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
-- Returns
--  array of objects from the RDB
-- Example getRdbArray('link.profile',1,6,{'name','enable'})))

function getRdbArray(authenticated, clsName, lower, upper, untilEmpty, members)
  local objs = {}
  local idx = 1
  luardb.lock()
  for id = lower, upper do
    local obj = {}
    local rdbPre = clsName .. '.' .. id
    obj.id = id

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
        luardb.unlock()
        return objs
      end
      obj[member] = data  or ''
    end
    objs[idx] = obj
    idx = idx + 1
  end
  luardb.unlock()
  return objs
end

-- This writes an array of objects to the RDB
-- Args
--   authenticated - true if the session is logged in
--   clsName is the name of the Rdb Array
--   members is an array of rdb member names
--   the array index is actually from the objects themselves (obj.id)
-- value is the array of objects
-- Returns
-- Example setRdbArray(true,'link.profile',{'name','enable'},array)
function setRdbArray(authenticated, clsName, members, value)
  local objs = value.objs
  luardb.lock()
  for _, obj in ipairs(objs) do
    local rdbName = clsName .. '.' .. obj.id
    for _, mem in ipairs(members) do
      if mem == '' then
        setRdb(rdbName, obj.rawdata)
      else
        setRdb(rdbName .. '.' .. mem, obj[mem])
      end
    end
  end
  luardb.unlock()
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

-- Expand Hostname regex with ':' and '/' for URL
function isValid.URL(val)
  if val and val:match("^[a-zA-Z0-9][a-zA-Z0-9%-_.:/]*[a-zA-Z0-9/]$")  then
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
    --from the browser code var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.:\t";
    not val:match("[$^\\<>%%\"%[%]%+%.,`='#&:]")
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

function isValid.Bool(val)
  return val == 0 or val == 1 or val == '0' or val == '1'
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

function check_passwd(password, hash)
  local handle = io.popen("/sbin/check_passwd  \""..password.."\" \""..hash.."\"")
  local result = handle:read("*a")
  handle:close()
  return result
end

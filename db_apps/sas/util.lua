
local util = {}

function util.split(inputstr, sep)
  if sep == nil then sep = "%s" end
  local t = {}
  local i = 1
  for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
    t[i] = str
    i = i + 1
  end
  return t
end

function util.shallow_copy(target, source)
  if not target then
    target = {}
  end
  if type(source) == 'table' then
    for i,v in pairs(source) do
      target[i] = v
    end
  end
  return target
end

function util.copy_only_vars(target, source, vars)
  if not target then
    target = {}
  end
  if type(source) == 'table' then
    for i,v in ipairs(vars) do
      if source[v] ~= nil then
        target[v] = source[v]
      end
    end
  end
  return target
end

function util.copy_only_different(target, source, vars)
  local different = false
  if not target then
   target = {}
  end
  if type(source) == 'table' then
    for i,v in ipairs(vars) do
      if source[v] ~= nil and (target[v] == nil or target[v] ~= source[v]) then
        different = true
        target[v] = source[v]
      end
    end
  end
  return different, target
end

function util.del_vars(target, vars)
  for _, k in ipairs(vars)
  do
    if target[k] ~= nil then
      target[k] = nil
    end
  end
end

function util.copy_named_vars(prefix, target, source, vtab)
  if not target then
    target = {}
  end
  if type(source) == 'table' then
    for k,v in pairs(source) do
      local nkey = prefix..k
      if vtab and vtab[v] then
        target[nkey] = vtab[v]
      end
    end
  end
  return target
end

function util.slurp(path)
    local f = io.open(path)
    local s = f:read("*a")
    f:close()
    return s
end

function util.ends_with(str, ending)
  return ending == "" or str:sub(-#ending) == ending
end

-- Your zone offset may change at any time

function util.zone()
  local now   = os.time()
  local lmt   = os.date("*t",now)
  local gmt   = os.date("!*t",now)
  local timel = os.time(lmt)
  local timeg = os.time(gmt)
  local tzone  = os.difftime(timel,timeg)

  if lmt.isdst then
    tzone = tzone + 3600
 --[[if tzone < 0 then
      tzone = tzone + 3600
    else
      tzone = tzone - 3600
    end
 --]]
  end
  return tzone
end

-- sadly os.time even when you give it values does not give you utc time
function util.parse_json_date(json_date)
    local pattern = "(%d+)%-(%d+)%-(%d+)%a(%d+)%:(%d+)%:([%d%.]+)([Z%+%-])(%d?%d?)%:?(%d?%d?)"
    local year, month, day, hour, minute,
        seconds, offsetsign, offsethour, offsetmin = json_date:match(pattern)
    local timestamp = os.time{year = year, month = month,
        day = day, hour = hour, min = minute, sec = seconds}
    local offset = 0
    if offsetsign ~= 'Z' then
      offset = tonumber(offsethour) * 60 + tonumber(offsetmin)
      if xoffset == "-" then offset = offset * -1 end
    end
    local zone = util.zone()
    return timestamp + offset + zone
end

function util.zulu(t)
  return os.date("!%Y-%m-%dT%TZ",t)
end

function util.tprint (tbl, indent)
  if not indent then indent = 1 end
  local s = ''
  for k, v in pairs(tbl) do
    s = s .. string.rep("  ", indent) .. tostring(k) .. ": "
    if type(v) == "table" then
      if indent > 5 then s = s ..'[TRUNCATED]\n' break end
      s = s .. '\n' .. util.tprint(v, indent+1)
    else
      s = s .. tostring(v) .. '\n'
    end
  end
  return s
end

function util.fstr(sn)
  return sn or 'nil'
end

function util.splitlines(s)
  if s:sub(-1)~="\n" then s=s.."\n" end
  return s:gmatch("(.-)\n")
end

return util


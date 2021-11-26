local M = {}

luanvramDB=M

database = {}

function M.init ()
  database = nil
  database = {}
end

function M.set (name, val)
  database[tostring(name)] = tostring(val)
end

function M.get (name)
  local retVal=""
  local useDB=0

  for i, val in pairs(database) do
    if i == name then
      retVal = val
      useDB=1
      break
    end
  end

  if useDB == 0 then
    retVal = luanvram.get(name)
  end
  return retVal
end

function M.printDB ()
  print("================================")
  if database == nil then
    print("DB is nil")
    return 0
  end

  for name, val in pairs(database) do
    print("name= ".. name .. ", val= ".. val)
  end
  print("================================\n\n")
end

-- for test
local function printmemsize(header)
	local tempStr = execute_CmdLine('ps | grep dimclient | grep -v grep')

	tempStr = tempStr:match("%s%s%s%d+")

	luasyslog.log('LOG_INFO', header..": "..tempStr)
end
-- for test

function M.commit ()  --> delete DB when call commit function
  local count = 0

---- start
  local nveventQ = luanvram.get("tr069.eventQ")
  local skipeventQ = 0

  for name, val in pairs(database) do
    if name == "tr069.eventQ" then
      if nveventQ == val then skipeventQ = 1 end
      break
    end
  end

  for name, val in pairs(database) do
    if name ~= "tr069.eventQ" or skipeventQ ~= 1 then
      count = count + 1
      luanvram.bufset(name, val)	
    end
  end

  if count ~= 0 then
    luanvram.commit()
    M.init ()
  end
---- end

--[[
  luanvram.close()
  luanvram.init()

  for name, val in pairs(database) do
    if name == "tr069.eventQ" then
      local NVvalue=luanvram.get("tr069.eventQ")
      NVvalue = string.gsub(NVvalue:gsub("^%s+", ""), "%s+$", "")
      if NVvalue ~= val then
        count = count + 1
        luanvram.bufset(name, val)
      end
    else
      count = count + 1
      luanvram.bufset(name, val)
    end
  end

  if count ~= 0 then
    luanvram.commit()
    M.init ()
  end
]]

end

function M.set_with_commit (name, val)
  M.set (name, val)
  M.commit ()
end
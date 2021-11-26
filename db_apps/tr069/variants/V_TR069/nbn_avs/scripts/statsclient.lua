#!/usr/bin/env lua
require ('luardb')
local socket = require ('socket')

local connection = {}

local function writestring(client, data, len)

  local s = 1

  if not len then
    len = string.len(data)
  end
    
  while true do
    local ad, emsg, n = client:send(data, s, len)

    if ad then
      return ad
    end
    if emsg == 'timeout' and n and n >= s then
      s = n
    else
      return nil, emsg
    end
  end
end

local port

local conxn

connection.sendbuf = function (data, len)
  if not port then return end
  if not conxn then
    conxn = socket.connect("127.0.0.1", port)
  end
  if not conxn then return nil, 'noserver' end
  if not len then
    len = string.len(data)
  end
  local f, emsg = writestring(conxn, 'tdata '..len..'\n', nil)
  if not f then
    conxn:close()
    conxn = socket.connect("127.0.0.1", port)
    if not conxn then return nil, 'noserver' end
    local f, emsg = writestring(conxn, 'tdata '..len..'\n', nil)
    if not f then
      conxn:close()
      conxn = nil
      return nil, emsg
    end
  end
  local line = conxn:receive('*l')
  if not line then
    conxn:close()
    conxn = nil
    return nil, 'no reply'
  end
  f, emsg = writestring(conxn, data, len)
  if not f then
    conxn:close()
    conxn = nil
    return nil, emsg
  end
  local line = conxn:receive('*l')
  if not line then
    conxn:close()
    conxn = nil
    return nil, 'no reply'
  end
  return true, ''
end

 --[[
    It is used to build up stats data, and commit them to file roll server
    the file roll server is started by statsroll.lua
 ]]
local self = {}
local data

self.init = function(fileRollPort)
  port  = fileRollPort or 2220
end

self.start = function()
  luardb.lock()
  data = 'NEWDATA\n'
end

self.set = function(k,v)
  -- v can be nil if the stat client's RDB variable does not exist
  v = v or ''
  luardb.set(k, v)
  data = data..k..' '..v..'\n'
end

self.commit = function()
  luardb.unlock()
  if data then
    local len = string.len(data)
    if len > 0 then
      connection.sendbuf(data, len)
      data = ''
    end
  end
end

return self


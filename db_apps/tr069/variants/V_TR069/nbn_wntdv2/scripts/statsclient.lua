#!/usr/bin/env lua
require ('socket')

local self = {}

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

self.sendbuf = function (data, len)
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
 
self.setport = function (p)
  port = p
end

return self


#!/usr/bin/env lua
require('luardb')
require('stringutil')
require('tableutil')

function rdbtable(prefix)
  local keys = luardb.keys(prefix)
  local tab = {}
  -- print(' keys :'..table.tostring(keys))
  for _, k in ipairs(keys) do
    -- check the prefix and optional dot
    local nk,n = k:gsub('^'..prefix..'%.?', '')
    if n > 0 then
      local val  = luardb.get(k)
      -- print(' key :'..k..'-> val :'..val)
      local l = string.explode(nk, '.')
      local t = tab
      while #l > 1 do
        local name = table.remove(l, 1)
        if not t[name] then
          t[name] = { }
        end
        t = t[name]
      end
      local name = table.remove(l, 1)
      t[name] = val
    end
  end
  return tab
end
      

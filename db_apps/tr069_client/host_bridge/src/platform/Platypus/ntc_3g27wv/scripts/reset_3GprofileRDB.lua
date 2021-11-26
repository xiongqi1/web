#!/usr/bin/lua

require('luardb')

local count = luardb.get('tr069.saved.numOf3Gprofile')

if count == nil then return 0 end

count = tonumber(count)
if count == nil or count < 1 then return 0 end

luardb.set('tr069.saved.numOf3Gprofile', '0')

for i=1, count do
	luardb.unset('tr069.saved.3Gprofile'..i)
end
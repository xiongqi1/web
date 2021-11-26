#!/usr/bin/env lua
require('luardb')
require('tableutil')

local curIndex = arg[1]
local apnlist_file = '/tmp/TR069savedAPNList'
local savedAPN = ''

if not curIndex then
	return 1
end

local status = luardb.get('link.profile.' .. curIndex .. '.status')

if not status then
	return 1
end
-- roll back apn list
if status == 'down' or status == '' then
	savedAPN = table.load(apnlist_file)

	if not savedAPN then 
		os.remove (apnlist_file)
		return 1
	end

	local prevIndex = savedAPN['Index']

	if not prevIndex or not tonumber(prevIndex) then
		os.remove (apnlist_file)
		return 1
	end

	for key, value in pairs(savedAPN) do
		if key ~= "Index" then
			luardb.set('link.profile.' .. prevIndex .. '.' .. key, value or '')
		end
	end

	if tonumber(curIndex) ~= tonumber(prevIndex) then 
		luardb.set('link.profile.' .. curIndex .. '.enable', '0')
		os.execute('sleep 1')
	end

	luardb.set('link.profile.' .. prevIndex .. '.enable', '1')	
end

-- clear resource
os.remove (apnlist_file)
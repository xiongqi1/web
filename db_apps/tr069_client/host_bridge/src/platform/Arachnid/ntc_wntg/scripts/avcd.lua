#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')

luasyslog.open('avcd', 'LOG_DAEMON')

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')

-- validations
validValues = {
	['enable'] = { '0', '1' },
	['status'] = { 'Disabled', 'Error', 'Up' },
}

function log(level, msg)
--	print(level, msg)
	luasyslog.log(level, msg)
end

function setStatus(id, status)
	if table.contains(validValues['status'], status) then
		local currentStatus = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.status')
		if currentStatus ~= status then
			luardb.set(conf.wntd.avcPrefix .. '.' .. id .. '.status', status)
			log('LOG_INFO', 'AVC ' .. id .. ' status := ' .. status)
		end
	else
		log('LOG_ERR', 'request to set invalid status "' .. status .. '" for AVC ' .. id)
	end
end

function setEnable(id, enable)
	if table.contains(validValues['enable'], enable) then
		log('LOG_INFO', 'AVC ' .. id .. ': enable := ' .. enable)
		-- FIXME: actually do it!
	else
		log('LOG_ERR', 'request to set invalid enable "' .. enable .. '" for AVC ' .. id)
		setStatus(id, 'Error')
	end
end

function configAVC(id, address, mplsTag, unid, vid)
	log('LOG_INFO', 'AVC ' .. id .. ': address := ' .. address)
	log('LOG_INFO', 'AVC ' .. id .. ': mplsTag := ' .. mplsTag)
	log('LOG_INFO', 'AVC ' .. id .. ': unid := ' .. unid)
	log('LOG_INFO', 'AVC ' .. id .. ': vid := ' .. vid)
end

function handleCreate(id)
	log('LOG_INFO', 'AVC ' .. id .. ': created')
end

function handleDelete(id)
	log('LOG_INFO', 'AVC ' .. id .. ': deleted')
end

function handleChange(id)
	log('LOG_INFO', 'AVC ' .. id .. ': changed')

	local enable = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.enable')
	local address = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.peer_address')
	local mplsTag = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.mpls_tag')
	local unid = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.unid')
	local vid = luardb.get(conf.wntd.avcPrefix .. '.' .. id .. '.vid')

	configAVC(id, address, mplsTag, unid, vid)
	setEnable(id, enable)
end

function getInstanceIds()
	local index = luardb.get(conf.wntd.avcPrefix .. '._index')
	if not index or index == '' then return {} end
	return index:explode(',')
end

knownInstanceIds = getInstanceIds()

function handleNotify(id)
	local currentInstanceIds = getInstanceIds()

	log('LOG_DEBUG', 'notify = "' .. id .. '", current = "' .. table.concat(currentInstanceIds, ',') .. '", known = "' .. table.concat(knownInstanceIds, ',') .. '"')

	if not table.contains(currentInstanceIds, id) and table.contains(knownInstanceIds, id) then
		-- instance deleted
		handleDelete(id)
	elseif not table.contains(knownInstanceIds, id) and table.contains(currentInstanceIds, id) then
		-- instance created
		handleCreate(id)
	else
		-- instance changed
		handleChange(id)
	end

	knownInstanceIds = currentInstanceIds
end

-- we watch the "<avcPrefix>.changed" variable
-- the TR-069 client will put a comma seperated list
-- of UNI-D IDs which have been created, deleted or changed into this variable
luardb.watch(conf.wntd.avcPrefix .. '.changed', function(k, ids)
	if ids ~= '' then
		luardb.lock()
		for _, id in ipairs(ids:explode(',')) do
			local avcId = tonumber(id) or 0
			if avcId > 0 then
				handleNotify(id)
			else
				log('LOG_INFO', 'invalid changed AVC ID ' .. id)
			end
		end
		luardb.set(conf.wntd.avcPrefix .. '.changed', '')
		luardb.unlock()
	end
end)


-- wait for changes
while true do
	luardb.wait(5)
end

local uploadDir = '/opt/cdcs/upload/'
local firmwareFile = uploadDir .. 'tr069-firmware.cdi'

----
-- Transfer Store
----
local function deleteAll()
	dimclient.log('info', 'transfer.deleteAll()')
	for _, key in ipairs(luardb.keys(conf.rdb.transferPrefix .. '.')) do
		if key:startsWith(conf.rdb.transferPrefix .. '.') then
--			print('matchedKeyForDelete', key)
			luardb.unset(key)
		end
	end
	return 0
end

local function delete(name)
	dimclient.log('info', 'transfer.delete(' .. name .. ')')
	luardb.unset(conf.rdb.transferPrefix .. '.' .. name)
	return 0
end

local function add(name, value)
	dimclient.log('info', 'transfer.add(' .. name .. ', "' .. value .. '")')
	luardb.set(conf.rdb.transferPrefix .. '.' .. name, value, 'p');
	return 0
end

local function getAll()
	local prefix = conf.rdb.transferPrefix .. '.'
	local transfers = {}
	dimclient.log('info', 'transfer.getAll()')
	for _, key in ipairs(luardb.keys(prefix)) do
		if key:startsWith(prefix) then
			local name = key:sub(prefix:len() + 1)
			transfers[name] = luardb.get(key)
		end
	end
	return transfers
end

----
-- Transfer Callbacks
----
local function downloadBefore(filename, downloadType)
	dimclient.log('info', 'transfer.downloadBefore(' .. filename .. ', ' .. downloadType .. ')')

	if downloadType == '1 Firmware Upgrade Image' then
		-- validate file path
		if not filename:startsWith(conf.fs.transferDir) then
			dimclient.log('info', 'invalid upload filename path.')
			return cwmpError.InvalidArgument
		end
		return 0
	end

	dimclient.log('info', 'download type not supported "' .. downloadType .. '".')
	return cwmpError.InvalidArgument -- not supported download
end

local function downloadAfter(filename, downloadType)
	dimclient.log('info', 'transfer.downloadAfter(' .. filename .. ', ' .. downloadType .. ')')
	if downloadType == '1 Firmware Upgrade Image' then
		-- do firmware upgrade
		dimclient.log('info', 'queuing firmware upgrade using "' .. filename .. '"')
		local ret, msg = os.rename(filename, conf.fs.firmwareFile)
		if not ret then
			dimclient.log('info', 'Could not rename download file: ' .. msg)
			return cwmpError.DownloadFailure
		end
		dimclient.callbacks.register('postSession', function()
			luasyslog.log('LOG_INFO', 'transfer: doing firmware upgrade...')
			os.execute('sync')
			os.execute('killall rdb_manager')
			os.execute(conf.fs.upgradeScript .. ' ' .. conf.fs.firmwareFile)
		end)
		return 0
	end

	dimclient.log('info', 'download type not supported "' .. downloadType .. '" in post-download callback?!')
	return cwmpError.InvalidArgument -- not supported download
end

local function uploadBefore(filename, filetype)
	dimclient.log('info', 'transfer.uploadBefore(' .. filename .. ', ' .. filetype .. ')')
	return 0
end

local function uploadAfter(filename, filetype)
	dimclient.log('info', 'transfer.uploadAfter(' .. filename .. ')')
	return 0
end

return {
	['deleteAll'] = deleteAll,
	['delete'] = delete,
	['getAll'] = getAll,
	['add'] = add,
	
	['downloadBefore'] = downloadBefore,
	['downloadAfter'] = downloadAfter,
	['uploadBefore'] = uploadBefore,
	['uploadAfter'] = uploadAfter
}

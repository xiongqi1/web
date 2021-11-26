local transferPrefix = 'tr069.transfer.'
local uploadDir = '/var/tr069/upload/'
local firmwareFile = uploadDir .. 'tr069-firmware.dat'
local configFile = uploadDir .. 'tr069-configure.dat'

transferTbl={}

function table.topurestr( tbl )
  local result, done = {}, {}
  for k, v in ipairs( tbl ) do
    table.insert( result, v )
    done[ k ] = true
  end
  for k, v in pairs( tbl ) do
    if not done[ k ] then
      table.insert( result, k .. "=" .. v )
    end
  end
  return table.concat( result, "," )
end

local function deleteAllTbl()
	transferTbl={}
	luanvramDB.set_with_commit("tr069.transferTbl", "");
end

local function deleteTbl(name)
	local tempTbl={}
	local nvramVal=""

	for i, item in ipairs(transferTbl) do
		if item[1] == nil or item[2] == nil then break end

                local sitem1=string.gsub(item[1],"%s+", "")

		if tostring(name) ~= sitem1 then
			tempTbl[#tempTbl+1] = {tostring(sitem1), tostring(item[2])}
		end
	end

	transferTbl=tempTbl

	for i, item in ipairs(tempTbl) do
		tbuf = table.topurestr(item)
		nvramVal= nvramVal..tbuf.."&"
	end

	luanvramDB.set_with_commit("tr069.transferTbl", nvramVal);
end

local function addTbl(name, value)
	local nvramVal=""
	local founditem=false
	local idx=0

	tname=tostring(name)
	tvalue=tostring(value)

	for i,item in ipairs(transferTbl) do
		if item[1] == tname then
			founditem=true
			idx=i
			break
		end
	end

	if founditem then
		transferTbl[tonumber(idx)]= {tname, tvalue}
	else
		transferTbl[#transferTbl+1] = {tname, tvalue}
	end

	for i, v in ipairs(transferTbl) do
		tbuf = table.topurestr(v)
		nvramVal= nvramVal..tbuf.."&"
	end

	luanvramDB.set_with_commit("tr069.transferTbl", nvramVal);
end


local function getAllTbl()
	local nvramstr = luanvramDB.get("tr069.transferTbl")
	local tArray = nvramstr:explode('&')

	for i, item in ipairs(tArray) do
		local subitems= item:explode(',')

		if subitems[1] == nil or subitems[2] == nil then break end

                local sitem1=string.gsub(subitems[1],"%s+", "")

		if sitem1 == "" then break end
		luardb.set(transferPrefix .. sitem1, subitems[2], 'p');
	end
end

----
-- Transfer Store
----
local function deleteAll()
	dimclient.log('info', 'transfer.deleteAll()')
	for _, key in ipairs(luardb.keys(transferPrefix)) do
		if key:startsWith(transferPrefix) then
--			print('matchedKeyForDelete', key)
			luardb.unset(key)
		end
	end
	deleteAllTbl()
	return 0
end

local function delete(name)
	dimclient.log('info', 'transfer.delete(' .. name .. ')')
	luardb.unset(transferPrefix .. name)
	deleteTbl(name)
	return 0
end

local function add(name, value)
	dimclient.log('info', 'transfer.add(' .. name .. ', "' .. value .. '")')
	luardb.set(transferPrefix .. name, value, 'p');
	addTbl(name, value)
	return 0
end

local function getAll()
	local transfers = {}
	dimclient.log('info', 'transfer.getAll()')
	getAllTbl()
	for _, key in ipairs(luardb.keys(transferPrefix)) do
		if key:startsWith(transferPrefix) then
			local name = key:sub(transferPrefix:len() + 1)
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
		if not filename:startsWith(uploadDir) then
			dimclient.log('info', 'invalid upload filename path.')
			return cwmpError.InvalidArgument
		end
		return 0
	elseif downloadType == '3 Vendor Configuration File' then
		-- validate file path
		if not filename:startsWith(uploadDir) then
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
		local ret, msg = os.rename(filename, firmwareFile)
		if not ret then
			dimclient.log('info', 'Could not rename download file: ' .. msg)
			return cwmpError.DownloadFailure
		end
--		dimclient.callbacks.register('postSession', function()
			luasyslog.log('LOG_INFO', 'transfer: doing firmware upgrade...')
			os.execute('rdb_set newname ' .. firmwareFile)
--			os.execute('/bin/upgrade.sh')
			os.execute('ln -s /bin/upgrade.sh /etc/rcSD && /sbin/reboot')
--			luardb.set('tr069.firmware.file', firmwareFile)
--			luardb.set('tr069.firmware.trigger', 1)
--		end)
		return 0
	elseif downloadType == '3 Vendor Configuration File' then
		-- do restore configuration
		dimclient.log('info', 'queuing configuration restore using "' .. filename .. '"')
		local ret, msg = os.rename(filename, configFile)
		if not ret then
			dimclient.log('info', 'Could not rename configuration file: ' .. msg)
			return cwmpError.DownloadFailure
		end
		dimclient.callbacks.register('postSession', function()
			luasyslog.log('LOG_INFO', 'transfer: doing configuration restore...')
			os.execute('ralink_init show 2860 2>/dev/null | grep \"tr069\\.\" 2>/dev/null 1>> ' .. configFile)
			os.execute('ralink_init renew 2860 ' .. configFile)
		end)
		return 0
	end

	dimclient.log('info', 'download type not supported "' .. downloadType .. '" in post-download callback?!')
	return cwmpError.InvalidArgument -- not supported download
end

local function uploadBefore(filename, filetype)
	dimclient.log('info', 'transfer.uploadBefore(' .. filename .. ', ' .. filetype .. ')')
	if filetype == '1 Vendor Configuration File' then
		if filename == nil then return 1 end

		os.execute('echo "#The following line must not be removed." > ' .. filename)
		os.execute('echo "Default" >> ' .. filename)
		os.execute('ralink_init show 2860 2>/dev/null | grep -v "tr069\." 2>/dev/null >> ' .. filename)

		local file = io.open(filename, 'r')
		if file == nil then
			return 1
		else
			file:close()
			return 0
		end
		return 0
	else
		return cwmpError.InvalidArgument
	end
end

local function uploadAfter(filename, filetype)
	dimclient.log('info', 'transfer.uploadAfter(' .. filename .. ', ' .. filetype .. ')')
	if filetype == '1 Vendor Configuration File' then
		os.execute('rm -f ' .. filename)
	end

	return 0
end

local function getConfigfileName ()
	dimclient.log('info', 'getConfigfileName()')
	local filename = "3G38W-int_backup_"
	local serialNum = "NETC:000000000000"

	local lan_MAC=execute_CmdLine("mac -r eth")
	lan_MAC = lan_MAC:gsub(":", "")
	if lan_MAC == nil or string.gsub(lan_MAC,"%s+", "") == "" then
		serialNum = "NETC:000000000000"
	else
		serialNum = "NETC:" .. string.match(lan_MAC, "%x+")
	end

	local datetime = os.date('%y%m%d_%H:%M:%S')
	datetime = datetime:gsub("%s+","")
	dimclient.log('info', 'get Config File name:' .. filename .. serialNum .. '_' .. datetime)
	return filename .. serialNum .. '_' .. datetime
end

return {
	['deleteAll'] = deleteAll,
	['delete'] = delete,
	['getAll'] = getAll,
	['add'] = add,

	['downloadBefore'] = downloadBefore,
	['downloadAfter'] = downloadAfter,
	['uploadBefore'] = uploadBefore,
	['uploadAfter'] = uploadAfter,
	['getConfigfileName'] = getConfigfileName
}

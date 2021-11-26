----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----
require('Logger')
Logger.addSubsystem('Transfer')

require('luardb')
require('rdbobject')
require('tableutil')
local urlModule = require('socket.url')
require('variants')

Transfer = {}

local classConfig = {
	persist = true,
	idSelection = 'sequential'
}
Transfer.class = rdbobject.getClass(conf.rdb.transferPrefix, classConfig)


Transfer.eventTypes = {
	['download'] = 'M Download',
	['upload'] = 'M Upload',
}

local _transferTypes = { 'download', 'scheduledDownload', 'upload' }

local _persistedFields = {
	'type',
	'state',
	'created',
	'updated',

	'key',
	'fileType',
	'url',
	'username',
	'password',
	'size',
	'target',
	'delay',
	'successURL',
	'failureURL',

	'started',
	'completed',

	'startAt',
	'result',
	'message',
	'inProgressExt',
	'rebootLocked',
}


local function lockStepProgress(instance)
	local handler = instance:getHandler()
	local extension = instance:getExtension()

	instance.inProgressExt = extension

	if handler.reboot then
		instance.rebootLocked = 'locked'
	end
end

local function unlockStepProgress(instance)
	instance.inProgressExt = ''
	instance.rebootLocked = ''
end

local function isLockStepProgress(instance)
	local transfers = Transfer.class:getByProperty('inProgressExt', instance:getExtension())
	local rebootLocks = Transfer.class:getByProperty('rebootLocked', 'locked')
	local handler = instance:getHandler()

	if (#transfers > 0) or (handler.reboot and #rebootLocks > 0) then
		return true
	else
		return false
	end
end

-- Check if download has expired by checking the time elapsed since the start
-- of download request against a pre defined timeout in config.
-- Also returns the appropriate error code and removes the transfer if expired,
-- otherwise does nothing.
-- @NOTE: expects time to be set properly by external services like NTP.
local function checkAndExpireDownloads(instance)
	local startTime = tonumber(instance.startAt)
	if (startTime == nil or startTime == '') then
		return
	end
	local downloadTimeout = tonumber(luardb.get(conf.rdb.downloadTimeout))
	local currTime = tonumber(os.time())
	Logger.log('Transfer', 'debug', 'state: ' .. tostring(instance.state) .. ' currTime : '
		.. tostring(currTime) .. ' startTime: '.. tostring(startTime)
		.. ' downloadTimeout: ' .. tostring(downloadTimeout))
	if (currTime > startTime and (currTime - startTime >= downloadTimeout)) then
		Logger.log('Transfer', 'error', 'elapsed time for download:'
		.. tostring(currTime - startTime))
		instance:setResult(9017, 'Download took longer than expected time: '
			.. tostring(downloadTimeout) .. ' secs')
		instance:setStatus('completed')
		unlockStepProgress(instance)
		instance:persist()
		daemon.host:sync()
	end
end

local function getFileName(path)
	local i = string.len(path)

	while string.sub(path, i, i) == "/" and i > 0 do
		path = string.sub(path, 1, i - 1)
		i = i - 1
	end
	while i > 0 do
		if string.sub(path, i, i) == "/" then
			break
		end
		i = i - 1
	end
	if i > 0 then
		path = string.sub(path, i + 1, -1)
	end
	if path == "" then
		path = "/"
	end

	return urlModule.unescape(path)
end

local _transferMachines = {
	['download'] = {
		['new'] = function(transfer)
			-- new job to do, work out start time
			transfer.startAt = tonumber(transfer.created) + (tonumber(transfer.delay) or 0)
			transfer:setStatus('waiting')
			transfer:persist()
		end,
		['waiting'] = function(transfer)
			-- triggered?
			assert(transfer.startAt, '')
			if tonumber(transfer.startAt) <= os.time() then
				-- Check expiry and take corrective actions for 'stuck' download requests.
				if type(conf.rdb.downloadTimeout) == 'string' then
					checkAndExpireDownloads(transfer)
				end
				if isLockStepProgress(transfer) == false then
					lockStepProgress(transfer)
					transfer:setStatus('prepare')
					transfer:persist()
				end
			end
		end,
		['prepare'] = function(transfer)
			if conf.transfer.prepareCommand then
				Logger.log('Transfer', 'debug', 'Transfer prepare command - ' .. conf.transfer.prepareCommand .. '.')
				os.execute(conf.transfer.prepareCommand)
			end
			transfer:setStatus('downloading')
			transfer:persist()
		end,
		['downloading'] = function(transfer)
			-- Check expiry and take corrective actions for 'stuck' download requests.
			if type(conf.rdb.downloadTimeout) == 'string' then
				checkAndExpireDownloads(transfer)
			end
			local handler = transfer:getHandler()
			-- delete any existing file
			local filename
			if handler.file then
				filename = conf.transfer.dir .. '/' .. handler.file
			else
				filename = conf.transfer.dir .. '/' .. getFileName(transfer.url)
			end
			os.remove(filename)

			-- fetch the file
			if not transfer.started then
				transfer.started = os.time()
				transfer:persist()
			end
			local ret, msg = Daemon.fetchURLToFile(transfer.url, filename, transfer.username, transfer.password)
			if ret then
				transfer:setResult(9010, 'Download failed: ' .. msg)
				transfer:setStatus('completed')
				unlockStepProgress(transfer)
				if variants.V_EVENT_NOTIFICATION == "y" then
					-- send notification for FOTA/DOTA result
					cmd = 'elogger 20 "TR-069 FOTA/DOTA: Download failed: ' .. msg .. '"'
					ret = os.execute(cmd)
				end
			else
				transfer:setStatus('installing')
			end
			transfer:persist()
		end,
		['installing'] = function(transfer)
			-- Check expiry and take corrective actions for 'stuck' download requests.
			if type(conf.rdb.downloadTimeout) == 'string' then
				checkAndExpireDownloads(transfer)
			end
			-- don't attempt upgrade mid-inform
			if daemon.host:isRequestLocked() then return end

			-- pause inform sessions (reboot will clear this)
			daemon.host:setPause(true)

			local handler = transfer:getHandler()

			-- as the install scripts may need to unmount the file system
			-- and may choose to never return we store a successful result
			-- first, then overwrite it if the script fails
			transfer:setResult(0, 'Success')
			transfer:setStatus('reboot-wait')
			transfer:persist()
			daemon.host:sync()

			local targetName = ''

			if handler.targetDir then
				if handler.target then
					targetName = handler.targetDir .. '/' .. handler.target

					-- escape ' character 
					targetName = targetName:gsub('([\'])', '\\%1')
					targetName = "'" .. targetName .. "'"
				else
					local filename = getFileName(transfer.url)
					if filename then
						targetName = handler.targetDir .. '/' .. filename

						-- escape ' character 
						targetName = targetName:gsub('([\'])', '\\%1')
						targetName = "'" .. targetName .. "'"
					end
				end
			end

			-- attempt upgrade
			-- the script must take care of doing whatever must be done to install the file
			-- if a reboot is required it is expected to trigger it, but we will if it doesn't
			local cmd
			local download_file = ''
			if handler.file then
				download_file = conf.transfer.dir .. '/' .. handler.file
			else
				download_file = conf.transfer.dir .. '/' .. getFileName(transfer.url)
			end

			-- escape ' character 
			download_file = download_file:gsub('([\'])', '\\%1')
			download_file = "'" .. download_file .. "'"
			cmd = handler.script .. ' ' .. download_file .. ' ' .. targetName

			ret = os.execute(cmd)

			-- run post command if it exists
			if handler.postcommand then
				os.execute("cd '" .. conf.transfer.dir .. "';" .. handler.postcommand)
			end

			if handler.keeptempfile and handler.keeptempfile == '0' then
				os.remove(download_file)
			end

			if ret ~= 0 then
				-- upgrade failed immediately, unpause and return error
				local msg = handler.msgRdb and luardb.get(handler.msgRdb)
				if not msg or msg == '' then msg = 'Firmware install failed' end
				transfer:setResult(9010, msg)
				transfer:setStatus('completed')
				unlockStepProgress(transfer)
				if variants.V_EVENT_NOTIFICATION == "y" then
					-- send notification for FOTA/DOTA result
					cmd = 'elogger 20 "TR-069 FOTA/DOTA: Firmware install failed"'
					ret = os.execute(cmd)
				end
			else
				if variants.V_EVENT_NOTIFICATION == "y" then
					-- upgrade successfully exited
					cmd = 'elogger 20 "TR-069 FOTA/DOTA: Firmware installed successfully";sleep 20;'
					ret = os.execute(cmd)
				end
				if handler.bootstrap then
					-- delete bootstrap flag
					host:setBootstrap(false)
					daemon.host:sync()
				end
				if handler.reboot and not handler.rebootHandler and not handler.rebootWaitCb then
					-- force a reboot if the script didn't
					luardb.set(conf.rdb.deviceResetReason, 'ACS System Upgrade') 
					luardb.set(conf.rdb.deviceReset, '1') -- trigger reboot template.
					if variants.V_CUSTOM_FEATURE_PACK == "wntd3_router" then
						os.execute('(sleep 10; echo "ACS System Upgrade" > /opt/normal_reboot; /sbin/reboot;)&') -- hard reboot (if template manager is already down)
					else
						os.execute('(sleep 10; echo "ACS System Upgrade" > /tmp/etc_rw/normal_reboot; /sbin/reboot;)&') -- hard reboot (if template manager is already down)
					end
				elseif type(handler.rebootWaitCb) == 'function' then
					do end -- do nothing.
				elseif handler.rebootHandler and type(handler.rebootHandler) == 'function' then

					if handler.rebootHandler(transfer) == true then
						luardb.set(conf.rdb.deviceResetReason, 'ACS System Upgrade') 
						luardb.set(conf.rdb.deviceReset, '1') -- trigger reboot template.
						if variants.V_CUSTOM_FEATURE_PACK == "wntd3_router" then
							os.execute('(sleep 10; echo "ACS System Upgrade" > /opt/normal_reboot; /sbin/reboot;)&') -- hard reboot (if template manager is already down)
						else
							os.execute('(sleep 10; echo "ACS System Upgrade" > /tmp/etc_rw/normal_reboot; /sbin/reboot;)&') -- hard reboot (if template manager is already down)
						end
					else
						transfer:setStatus('completed')
						unlockStepProgress(transfer)
					end
				else
					-- no reboot needed
					transfer:setStatus('completed')
					unlockStepProgress(transfer)
				end
			end
			transfer:persist()
			daemon.host:sync()
			daemon.host:setPause(false)
		end,
		['reboot-wait'] = function(transfer)
			local handler = transfer:getHandler()
			if type(handler.rebootWaitCb) == "function" then
				local status = handler.rebootWaitCb()
				if status == "success" or status == "failure" then
					if status == "success" then
						transfer:setResult(0, "Success")
					else
						transfer:setResult(9010, "Firmware install failed")
					end
					transfer:setStatus("completed")
					unlockStepProgress(transfer)
					transfer:persist()
					daemon.host:sync()
					daemon.host:setPause(false)
				end
			end
		end,
		['completed'] = function(transfer)
			-- nothing, job done
		end,
		['archived'] = function(transfer)
			-- nothing, archiving is just for debug inspection of transfer state
		end,
	},
	['upload'] = {
		['new'] = function(transfer)
			-- new job to do, work out start time
			transfer.startAt = tonumber(transfer.created) + (tonumber(transfer.delay) or 0)
			transfer:setStatus('waiting')
			transfer:persist()
		end,
		['waiting'] = function(transfer)
			-- triggered?
			assert(transfer.startAt, '')
			if tonumber(transfer.startAt) <= os.time() then
				transfer:setStatus('fetching')
				transfer:persist()
			end
		end,
		['fetching'] = function(transfer)
			local getFileNameScript
			local uploadParms=conf.transfer.types['upload'][transfer.fileType]
			local cmd = uploadParms.script
			if uploadParms.variableFileName == true then
				getFileNameScript = uploadParms.getFileNamescript
			end
			-- Slight enahancment to the upload to increase the upload types, required for the Hannibal XEMS smartcity ( and possibly others )
			-- We look for a match in the URL i.e. http://server/uploaddir/match/
			-- If we match the upload will be to http://server/uploaddir/
			local matchInUrl = uploadParms.matchInUrl
			if matchInUrl then
				for match,script in pairs(matchInUrl) do
					local strToMatch='/'..match..'/'
					if string.match( transfer.url, strToMatch ) then
						local start, finish = string.find( transfer.url, strToMatch )
						transfer.url = string.sub( transfer.url, 1, start )
						getFileNameScript='/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua '..match
						cmd=script
						break
					end
				end
			end

			local filename
			if getFileNameScript then
				filename = conf.transfer.dir .. '/' .. Daemon.readCommandOutput(getFileNameScript)
			else
				filename= conf.transfer.dir .. '/' .. uploadParms.file
			end

			os.remove(filename)
			transfer.target = filename

			-- fetch the file
			if not transfer.started then
				transfer.started = os.time()
				transfer:persist()
			end
			ret = os.execute(cmd..' '.. filename)
			if ret ~= 0 then
				transfer:setResult(9011, 'Fetching failed: ')
				transfer:setStatus('completed')
				os.execute('rm -f ' .. filename)
			else
				transfer:setStatus('uploading')
			end
			transfer:persist()
		end,
		['uploading'] = function(transfer)
			-- delete any existing file
			local filename = transfer.target

			-- fetch the file
			if not transfer.started then
				transfer.started = os.time()
				transfer:persist()
			end
			local ret, msg = Daemon.uploadFileToURL(transfer.url, filename, transfer.username, transfer.password)
			if ret then
				transfer:setResult(9011, 'Uploading failed: ' .. msg)
			else
				transfer:setResult(0, 'Success')
			end
			os.execute('rm -f ' .. filename)
			transfer:setStatus('completed')
			transfer:persist()
		end,
		['completed'] = function(transfer)
			-- nothing, job done
		end,
		['archived'] = function(transfer)
			-- nothing, archiving is just for debug inspection of transfer state
		end,
	}
}

function Transfer.isSupportedTransportURLType(url)
	for _, transportType in ipairs(conf.transfer.supportedTransports) do
		if url:match('^' .. transportType .. ':') then return true end
	end
	return false
end

function Transfer.isSupportedFileType(downloadType, fileType)
	assert(conf.transfer.types[downloadType], 'Unknown download type "' .. downloadType .. '".')
	return (conf.transfer.types[downloadType][fileType] ~= nil)
end



local function persist(self)
	luardb.lock()
	local rdbInstance
	if self.id then
		-- existing instance
		rdbInstance = Transfer.class:getById(self.id)
		self.updated = os.time()
	else
		-- new instance
		rdbInstance = Transfer.class:new()
		self.id = Transfer.class:getId(rdbInstance)
	end
	for _, name in ipairs(_persistedFields) do
		rdbInstance[name] = self[name]
	end

	luardb.unlock()
end

local function delete(self)
	if self.id then
		-- persisted
		luardb.lock()
		local instance = Transfer.class:getById(self.id)
		Transfer.class:delete(instance)
		luardb.unlock()
	else
		-- not persisted; do nothing
		Logger.log('Transfer', 'warning', 'Attempt to delete() a non-persisted transfer instance.')
	end
end

local function setStatus(self, state, msg)
	Logger.log('Transfer', 'notice', 'Transfer ' .. self.id .. ': ' .. self.state .. ' -> ' .. state .. (msg and ': ' .. msg or '') .. '.')
	self.state = state
	self.completed = os.time()
	luardb.set(conf.rdb.serivcePulse, state) -- interrupt waiting in Client.poll

end


local function setResult(self, code, msg)
	Logger.log('Transfer', 'notice', 'Transfer ' .. self.id .. ' result: ' .. code .. ': ' .. msg .. '.')
	self.result = code
	self.message = msg
end

local function stepMachine(self)
	if self.type == nil then -- This if statement added for TT #10920.
		Logger.log('Transfer', 'error', 'Transfer RDB is corrupt, deleting')
		self:delete()    -- Due to nature of problem root cause is unknown but this allows recovery from
		return           -- the rdb corruption
	end
	assert(_transferMachines[self.type], 'No state machine handler for transfer type "' .. self.type .. '".')
	assert(_transferMachines[self.type][self.state], 'No state handler for state "' .. self.state .. '" of transfer type "' .. self.type .. '".')
	_transferMachines[self.type][self.state](self)
end

-- return the file extension from the filename
-- special case is a filename with cbq- in it. This will have an extension CBQ ( just internally for use in the config.lua file )
local function getFileExtension(filename)
	if filename then
		if string.find( filename, 'cbq%-' ) then
			return 'CBQ'
		end
		return string.match(filename,'%.([^.]+)$') -- everything from the end to the .
	end
	return nil
end

-- try and get the extension from the URL. If that doesn't work try the target filename
local function getExtension(self)
	--first get the filename (path) from the URL
	local parsed_url=urlModule.parse(self.url)
	return getFileExtension(parsed_url.path) or ( getFileExtension(self.target) or 'NOEXTENSION' )
end

local function getHandler(self)
	local extension = self:getExtension()
	assert(conf.transfer.types[self.type][self.fileType], 'File type "' .. self.fileType .. '" not supported for ' .. self.type .. ' transfers.')
	return assert(conf.transfer.types[self.type][self.fileType][extension], 'File extension "' .. extension .. '" not supported for "' .. self.fileType .. '" ' .. self.type .. ' transfers.')
end

local function getEventType(self)
	return assert(Transfer.eventTypes[self.type], 'No event type for transfer type "' .. self.type .. '".')
end

function Transfer.new(transferType)
	assert(table.contains(_transferTypes, transferType), 'Unknown transfer type "' .. transferType .. '".')
	local transfer = {
		id = nil,
		state = 'new',
		type = transferType,
		created = os.time(),

		['setStatus'] = setStatus,
		['setResult'] = setResult,
		['stepMachine'] = stepMachine,
		['getEventType'] = getEventType,
		['persist'] = persist,
		['delete'] = delete,
	}
	return transfer
end

-- fetch all transfers
function Transfer.getAll()
	local transfers = {}
	local rdbInstances = Transfer.class:getAll()
	for _, rdbInstance in ipairs(rdbInstances) do
		local id = Transfer.class:getId(rdbInstance)
		table.insert(transfers, Transfer.getById(id))
	end
	return transfers
end

-- fetch number of transfers by type
function Transfer.getNumTransfersByType(transferType)
	local rdbInstances = Transfer.class:getAll()
	local count = 0
	for _, rdbInstance in ipairs(rdbInstances) do
		local id = Transfer.class:getId(rdbInstance)
		local transfer = Transfer.getById(id)
		if (transfer.type == transferType) then
			count = count + 1
		end
	end
	return count
end

-- fetch transfer by its ID
function Transfer.getById(id)
	luardb.lock()
	local rdbInstance = Transfer.class:getById(id)
	local transfer = {
		id = id,

		['setStatus'] = setStatus,
		['setResult'] = setResult,
		['stepMachine'] = stepMachine,
		['getEventType'] = getEventType,
		['getExtension'] = getExtension,
		['getHandler'] = getHandler,
		['persist'] = persist,
		['delete'] = delete,
	}
	for _, name in ipairs(_persistedFields) do
		transfer[name] = rdbInstance[name]
	end
	luardb.unlock()
	return transfer
end

-- fetch all transfers in a specific state
function Transfer.getByState(state)
	local transfers = {}
	local rdbInstances = Transfer.class:getByProperty('state', state)
	for _, rdbInstance in ipairs(rdbInstances) do
		local id = Transfer.class:getId(rdbInstance)
		table.insert(transfers, Transfer.getById(id))
	end
	return transfers
end

-- FIXME: this may be a bit expensive?
function Transfer.completedPending()
	local completed = Transfer.class:getByProperty('state', 'completed')
	return #completed > 0
end

return Transfer

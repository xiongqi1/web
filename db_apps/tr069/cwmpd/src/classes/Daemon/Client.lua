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
require('Daemon')

require('cdcsutil')
require('CWMP.Session')
require('variants')

Daemon.Client = {}

local function addEvent(self, eventCode, commandKey)
	return self.host:addEvent(eventCode, commandKey)
end

-- test whether a BOOTSTRAP is pending to be sent
local function _isBootstrapPending(self)
	local events = self.host:getEvents()
	for _, event in ipairs(events) do
		if event.code == '0 BOOTSTRAP' then
			return true
		end
	end
	return false
end

local function _assertTaskType(self, taskType)
	assert(type(self.tasks[taskType]) == 'table', 'Unknown task type "' .. taskType .. '".')
end

local _builtInTasks = { 'postInit', 'preSession', 'postSession', 'postRPC', 'cleanUp', 'sessionDeferred', 'parameterPoll' }

local function addTaskType(self, taskType)
	assert(self.tasks[taskType] == nil, 'Task type "' .. taskType .. '" already registered.')
	self.tasks[taskType] = {}
end

local function removeTaskType(self, taskType)
	_assertTaskType(self, taskType)
	assert(not table.contains(_builtInTasks, taskType), 'You may not remove built-in task type "' .. taskType .. '".')
	self.tasks[taskType] = nil
end

local function addTask(self, taskType, func, persistent, data)
	persistent = persistent or false
	_assertTaskType(self, taskType)
	assert(type(func) == 'function', 'Task should be a function.')
	local task = {
		func = func,
		data = data,
		client = self,
		persistent = persistent,
	}
	table.insert(self.tasks[taskType], task)
end

local function removeTask(self, taskType, func, data)
	_assertTaskType(self, taskType)
	assert(type(func) == 'function', 'Task should be a function.')
	for idx, task in ipairs(self.tasks[taskType]) do
		if task.func == func and ((data ~= nil and task.data == data) or data == nil) then
			table.remove(self.tasks[taskType], idx)
		end
	end
	assert('Task not registered for task type "' .. taskType .. '".')
end

local function isTaskQueued(self, taskType, func, data)
	_assertTaskType(self, taskType)
	assert(type(func) == 'function', 'Task should be a function.')
	for idx, task in ipairs(self.tasks[taskType]) do
		if task.func == func and ((data ~= nil and task.data == data) or data == nil) then
			return true
		end
	end
	return false
end

local function runTasks(self, taskType, ...)
	local persistentTasks = {}
	_assertTaskType(self, taskType)
	for _, task in ipairs(self.tasks[taskType]) do
		local ret, errorMsg = pcall(task.func, task, ...)
		if not ret then
			Logger.log('Daemon', 'error', 'Task (' .. taskType .. ') error: ' .. errorMsg)
		end
		if task.persistent then
			table.insert(persistentTasks, task)
		end
	end
	self.tasks[taskType] = persistentTasks
end

local function freeMemory(self)
	local before = collectgarbage('count')
	Logger.log('Daemon', 'debug', 'Memory: ' .. string.format('%0.1f', before) .. ' KiB currently in use.')
	collectgarbage('collect')
	local after = collectgarbage('count')
	local reclaimed = before - after
	Logger.log('Daemon', 'debug', 'Memory: ' .. string.format('%0.1f', reclaimed) .. ' KiB reclaimed, '.. string.format('%0.1f', after) .. ' KiB used.')
end

local function _doSession(self)
	self.acs.version = self.host:getProtocolVersion()
	self.host:requestLock(true)
	self:runTasks('preSession')
	self:triggerPassiveNotif()
	self.inSession = true
	local session = CWMP.Session.new(self.acs)
	local ret = session:establish(self.tree, self.host)
	self:runTasks('postSession', ret)
	self:runTasks('cleanUp', ret)
	self:freeMemory()
	self.lastInform = os.time()
	self.inSession = false
	self.host:requestLock(false)
	return ret
end

local function poll(self)

	local max_idle_time = 60

	local now = os.time()

	-- inform retry expiry?
	if self.informRetryAt and self.informRetryAt <= now then
		Logger.log('Daemon', 'notice', 'Retrying failed session.')
		self.informRequired = true
		self.informRetryAt = nil
	end

	-- periodic inform expiry?
	local periodic = self.tree:getPeriodicInformData()
	if periodic.enabled and not self.host:isPaused() then
		local nextPeriodic

		if periodic.phase >= 0 then
			local lastInform = self.lastInform or now
			local delta = math.abs(lastInform - periodic.phase)
			local interval = periodic.interval

			if lastInform >= periodic.phase then
				if delta < periodic.interval then
					interval =  interval - delta
				else
					interval = interval - (delta % periodic.interval)
				end
			else
				if delta < periodic.interval then
					interval = delta
				else
					interval = delta % periodic.interval
				end
			end

			nextPeriodic = interval + lastInform

			if ((nextPeriodic + periodic.interval) < now) or ((nextPeriodic - periodic.interval) > now) then
				-- send it now!
				nextPeriodic = now
			end

-- 			while true do
-- 				if (nextPeriodic + periodic.interval) < now then
-- 					nextPeriodic = nextPeriodic + periodic.interval
-- 				elseif (nextPeriodic - periodic.interval) > now then
-- 					nextPeriodic = nextPeriodic - periodic.interval
-- 				else
-- 					break
-- 				end
-- 			end
		else
			nextPeriodic = (self.lastInform or now) + periodic.interval
		end


		Logger.log('Daemon', 'debug', 'Periodic: phase ' .. periodic.phase .. ', interval ' .. periodic.interval .. ', now ' .. now .. ', last ' .. (self.lastInform or '*never*') .. ', next ' ..  nextPeriodic .. '.')
		local idle_time =nextPeriodic - now
		if nextPeriodic <= now then
			idle_time=1
			Logger.log('Daemon', 'debug', 'Periodic inform requested.')
			if _isBootstrapPending(self) then
				Logger.log('Daemon', 'info', 'BOOTSTRAP pending, discarding PERIODIC')
			else
				self:asyncInform('2 PERIODIC')
			end
		end
		if idle_time < max_idle_time then
			max_idle_time = idle_time
		end


	end

	-- schedule inform
	if _isBootstrapPending(self) then
		Logger.log('Daemon', 'info', 'BOOTSTRAP pending, discarding SCHEDULED')
	else
		local schedInforms = self.host:getSchedInformsNeedToReport(now)
		if #schedInforms > 0 then
			Logger.log('Daemon', 'notice', 'Scheduled inform requested.')
			self:asyncInform('3 SCHEDULED')
			for _, sched in ipairs(schedInforms) do
				self:addEvent('M ScheduleInform', sched.key)
				self.host:deleteSchedInform(sched)
			end
		end
	end

	-- completed transfers
	local transfers = Transfer.getByState('completed')
	if #transfers > 0 then
		Logger.log('Daemon', 'notice', 'File transfer completion inform requested.')
		self:asyncInform('7 TRANSFER COMPLETE')
		for _, transfer in ipairs(transfers) do
			self:addEvent(transfer:getEventType(), transfer.key)
		end
	end

	if self.informRequired and not self.host:isPaused() then

		if conf.reportInformStatus then
			local informStartAt = os.date('!%Y-%m-%d %H:%M:%S') -- In UTC.
			local informEvents = ''
			for _, event in pairs(self.host:getEvents()) do
				local event = string.trim(event.code)
				if event ~= '' then
					informEvents = (informEvents ~= '' and informEvents .. ', ' or '') .. event
				end
			end
			luardb.set("tr069.informStartAt", informStartAt .. ' [' .. informEvents .. ']')
			luardb.set("tr069.informEndAt", '')
		end
		-- perform an inform session
		local ok = _doSession(self)

		if conf.reportInformStatus then
			local informEndAt = os.time() -- seconds since 1970-01-01 00:00:00 UTC
			local informStatus = ok and 'Success' or 'Failure'
			luardb.set("tr069.informEndAt", os.date('!%Y-%m-%d %H:%M:%S', informEndAt) .. ' [' .. informStatus .. ']') -- set time in UTC.
			-- record the last successful inform time.
			if ok then
				luardb.set("tr069.lastSuccInformAt", informEndAt)
			end
		end

		if ok then
			self.host:setStatus('inform-ok')
			Logger.log('Daemon', 'debug', 'ACS session completed OK.')

			if variants.V_CUSTOM_FEATURE_PACK == "Santos" then
				-- keep track of the number of fallback sessions
				-- After 5, switch back to the main interface
				if self.acs.isFallback == true then
					self.acs.fallback_sessions = self.acs.fallback_sessions + 1
					if self.acs.fallback_sessions >= 5 then
						Logger.log('Daemon', 'error', 'ACS: switchToMainInterface')
						self.acs.fallback_sessions = 0
						self.acs.current_interface = self.acs.interface
						luardb.set('tr069.server.current_interface', self.acs.current_interface)
						self.acs.isFallback = false
					end
				end
			end
		else
			self.host:setStatus('inform-error')
			local waitTime = self.acs:getRetryDelay(self.tree)
			self.informRetryAt = now + waitTime
			max_idle_time = waitTime

			if variants.V_CUSTOM_FEATURE_PACK == "Santos" then
				Logger.log('Daemon', ((self.acs.retries <= 10) and 'warning' or 'error'), 'ACS session failed, retry ' .. self.acs.retries ..  ' in ' .. waitTime .. ' seconds.')

				if self.acs.isFallback == true then
					Logger.log('Daemon', 'error', 'On Fallback')
				else
					Logger.log('Daemon', 'error', 'On Main')
				end

				if self.acs.isFallback == true then
					Logger.log('Daemon', 'error', 'Already on fallback')
					if self.acs.retries >= 10 then
						Logger.log('Daemon', 'error', 'ACS: switchToMainInterface')
						self.acs.fallback_sessions = 0
						self.acs.current_interface = self.acs.interface
						luardb.set('tr069.server.current_interface', self.acs.current_interface)
						self.acs.isFallback = false
					end
				else
					if self.acs.retries >= 5 then
						Logger.log('Daemon', 'error', 'ACS: switchToFallbackInterface')
						self.acs.fallback_sessions = 0
						self.acs.current_interface = self.acs.fallbackInterface
						luardb.set('tr069.server.current_interface', self.acs.current_interface)
						self.acs.isFallback = true
					end
				end
			end

			--[[
				Trigger a reboot if enough retries have failed.

				FIXME: This is a stop-gap measure requested by Hitachi in an
				attempt to recover from a loss of remote access due to
				lingering undiagnosed connectivity issues. Long term, this
				check really, really needs to go away.
			]]
			if variants.V_CUSTOM_FEATURE_PACK == "hitachi_nedo" and self.acs.retries > self.retryLimit then
				Logger.log("Daemon", "error", self.acs.retries-1 .. " retries have failed. Rebooting device.")
				os.execute("reboot")
			else
				Logger.log('Daemon', 'warning', 'ACS session failed, retry ' .. self.acs.retries ..  ' in ' .. waitTime .. ' seconds.')
			end
		end
		self.informRequired = false
		self:runTasks('sessionDeferred', ret)
	end


	self:triggerActiveNotif() -- could require infor

	-- calculate next wake-up for ScheduleInform
	now = os.time()
	local schedInforms = self.host:getSchedInformsNeedToReport(now + max_idle_time)
	if #schedInforms > 0 then
		for _, sched in ipairs(schedInforms) do
			reportAt = tonumber(sched.reportAt)
			if now + max_idle_time > reportAt then
				max_idle_time = reportAt - now
			end
		end
	end

	if conf.enabledBulkDataCollection == true then
		if self:isBulkDataEncodigRequested() then
			local profileObj = self.mBulkDataParam.getProfileObj()
			local profileInsts = profileObj:getAll()
			for _, pInst in pairs(profileInsts) do
				if self.mBulkDataDaemon.getState(pInst) == "waitEncoding" then
					local paramInsts = self.mBulkDataParam.getSubInstance("Parameter", profileObj:getId(pInst), nil)
					local dataTbl = {}
					local paramNodes
					for _, pInst in pairs(paramInsts) do
						if pInst.Reference and pInst.Reference ~= "" then
							paramNodes = self.tree:getByWildcards(pInst.Reference)
							if paramNodes then
								for _, node in ipairs(paramNodes) do
									if node:isParameter() then
										local key = node:getAlternativeName(pInst.Name, pInst.Reference)
										local ret, value = node:getCWMPValue()
										if key and ret == 0 then
											dataTbl[key] = value
										end
									end
								end
							end
						end
					end

					local retEncoding = false
					if next(dataTbl) and pInst._tmpEncodingFile then
						local retTbl = {
							Report={dataTbl},
							uri=nil,
						}

						-- Get RequestURIParameter
						local uriInsts = self.mBulkDataParam.getSubInstance("HttpUri", profileObj:getId(pInst), nil)
						local httpUri = {}
						for _, uInst in pairs(uriInsts) do
							local name = uInst.Name
							local ref = uInst.Reference
							if ref and ref ~= "" then
								local node = self.tree:find(ref)
								if node and node:isParameter() then
									local ret, value = node:getCWMPValue()
									if ret == 0 then
										local qAttrib = ""
										local qValue = ""
										if not name or name == "" then
											qAttrib = tostring(value)
										else
											qAttrib = name
											qValue = tostring(value)
										end
										table.insert(httpUri, string.format("%s=%s", self.mUrl.escape(qAttrib), self.mUrl.escape(qValue)))
									end
								end
							end
						end

						if #httpUri > 0 then
							retTbl.uri = table.concat(httpUri, "&")
						end

						local fd = io.open(pInst._tmpEncodingFile, "w+") -- update mode, all previous data is erased.
						local status, content = pcall(self.mJSON.encode_pretty, self.mJSON, retTbl)
						if fd and status then
							fd:write(content)
							fd:close()
							retEncoding = true
						end
					end
					if retEncoding then
						self.mBulkDataDaemon.setState(pInst, "encodingSuccess")
					else
						self.mBulkDataDaemon.setState(pInst, "encodingFailed")
					end
				end
			end
			self:resetBulkDataEncodingReq()
		end
	end

--[[
	be careful, eventWait or luardb.wait is only called here.
	if there functions are called anywhere else,
	it could take a long time to wait for transfer,
	becasue one luardb.wait consumes one rdb event.
--]]

	if self.informRequired then
		self.host:eventWait(1)
	else
		self:runTasks('parameterPoll') -- only run if no inform is pending
		-- determine the wait time since parameterPoll tasks might update it
		local pWait = self.paramPollWait
		self.paramPollWait = nil
		if pWait and pWait < max_idle_time then
			max_idle_time = pWait
		end
		if max_idle_time <= 0 then
			max_idle_time = 1 -- wait for 1 sec at minimum
		end
		self.host:eventWait(max_idle_time)
	end
end

-- dynamic ACS change ($ROOT.ManagementServer.URL change)
local function _changeACS(self)
	assert(not self.inSession, 'ACS change attempted in-session.')
	Logger.log('Daemon', 'notice', 'ACS changed, performing bootstrap.')
	-- self.acs:close() -- ACS is replaced outside session, so will already be closed
	self.acs = Daemon.ACS.new()
	self:asyncInform('0 BOOTSTRAP') -- clears all other events too
	self.host:setBootstrap(true)
	self.lastInform = nil
	self.informRetryAt = nil
	self.informRequired = true
end

local function asyncParameterChange(self, node, param, value)
	if self.inSession then
		-- if we are in session with the ACS we defer the change until session completion
		Logger.log('Daemon', 'notice', 'Deferring asynchronous parameter change: ' .. param .. ' := ' .. value)
		self:addTask('sessionDeferred', function()
			self:asyncParameterChange(node, param, value)
		end)
		return
	end

	Logger.log('Daemon', 'info', 'Asynchronous parameter change: ' .. param .. ' := ' .. value .. ' notify ' .. node.notify)
	if _isBootstrapPending(self) then
		Logger.log('Daemon', 'info', 'BOOTSTRAP pending, discarding VALUE CHANGE')
		self.informRequired = true

		-- if the ACS URL changed reset everything after session is completed!
		if self.tree:isManagementServerURL(param) then
			_changeACS(self)
		end
		return
	end

	if node.notify > 0 then
		self.tree:markChanged(node)
	end

	-- notification handling
	if node.notify == 2 then
		-- active notification
		self:asyncInform('4 VALUE CHANGE')
	elseif node.notify == 1 then
		-- passive notification
		self:addEvent('4 VALUE CHANGE')
	end

	-- if the ACS URL changed reset everything after session is completed!
	if self.tree:isManagementServerURL(param) then
		_changeACS(self)
	end
end

local function asyncInform(self, event, commandKey)
	if self.inSession then
		-- if we are in session with the ACS we defer the event addition until session completion
		Logger.log('Daemon', 'debug', 'Deferring asynchronous inform request' .. (event and (': ' .. event) or '') .. '.')
		self:addTask('sessionDeferred', function()
			self:asyncInform(event, commandKey)
		end)
		return
	end

	Logger.log('Daemon', 'debug', 'Asynchronous inform requested' .. (event and (': ' .. event) or '') .. '.')
	if event then
		self.host:addEvent(event, commandKey)
	end
	self.informRequired = true
end

local function stop(self)
	self.running = false
end

local function init(self)
	-- init all parameters
	self.tree:init()

	-- events already in queue?
	local events = {}
	for _, event in pairs(self.host:getEvents()) do
		table.insert(events, event.code)
	end
	if #events > 0 then
		Logger.log('Daemon', 'notice', 'Pending events at start-up: ' .. table.concat(events, ', '))
	end

	-- once-off bootstrap?
	if not self.host:getBootstrap() then
		self:addEvent('0 BOOTSTRAP')
		self.host:setBootstrap(true)
		Logger.log('Daemon', 'notice', 'Bootstrap inform requested.')
		-- TODO:: should avoid using V variable at runtime.
		-- Need to separate this part with lua module or something else later.
		if variants.V_CUSTOM_FEATURE_PACK == "bellca" then
			local reasonFactoryReset = self.host:getFactoryResetReason()
			if reasonFactoryReset == 'tr069' then
				self:addEvent('X OUI M Factory Reset')
			elseif reasonFactoryReset == 'webui' then
				self:addEvent('X OUI U Factory Reset')
			else
				self:addEvent('X OUI RG Factory Reset')
			end
			self.host:resetFactoryResetReason()
		end
	end

	-- client init is assumed to be boot (for now)
	self:addEvent('1 BOOT')

	-- post init tasks
	self:runTasks('postInit')

	-- register callbacks for connection request and shutdown
	self.host:onShutdown(function()
		self:stop()
	end)
	self.host:onRequest(function()
		self:asyncInform('6 CONNECTION REQUEST')
	end)

	if variants.V_CUSTOM_FEATURE_PACK == "hitachi_nedo" then
		local retryLimit = tonumber(luardb.get("tr069.retryLimit"))
		if retryLimit ~= nil and retryLimit > 0 then
			self.retryLimit = retryLimit
		end
		Logger.log('Daemon', 'info', "Device will reboot if "..self.retryLimit.." consecutive inform requests fail.")
	end

	if conf.enabledBulkDataCollection == true then
		self.host:onBulkDataEncoding()
	end

	Logger.log('Daemon', 'notice', 'Boot inform requested.')
	self.informRequired = true
end

local function addNotification(self, notifType, path)

	self:removeNotification(path)

	local node = self.tree:find(path)
	local ret, value = node:getValue(path)

	if ret == 0 and node.value ~= value then
		node.value = value
	end

	if notifType == 'active' then
		if not table.contains(self.activeNotiTbl, path) then
			table.insert(self.activeNotiTbl, path)
		end
	elseif notifType == 'passive' then
		if not table.contains(self.passiveNotiTbl, path) then
			table.insert(self.passiveNotiTbl, path)
		end
	else
		Logger.log('Daemon', 'error', 'Invalid Notification Type to add')
		error('Invalid Notification Type: ' .. notifType .. '(' .. path .. ').')
	end

--[[
	Logger.log('Daemon', 'error', 'addNotification type=' .. notifType .. ', path=[' ..  path .. ']')
	Logger.log('Daemon', 'error', 'activeNotiTbl addNotification Num Of entries' .. #self.activeNotiTbl )
	Logger.log('Daemon', 'error', '-----------------------------------------------')
	for i, entry in ipairs(self.activeNotiTbl) do
		Logger.log('Daemon', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Daemon', 'error', '-----------------------------------------------')

	Logger.log('Daemon', 'error', 'passiveNotiTbl addNotification Num Of entries' .. #self.passiveNotiTbl )
	Logger.log('Daemon', 'error', '-----------------------------------------------')
	for i, entry in ipairs(self.passiveNotiTbl) do
		Logger.log('Daemon', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Daemon', 'error', '-----------------------------------------------')
--]]


	return 0
end

local function removeNotification(self, path)

	for idx, entry in ipairs(self.activeNotiTbl) do
		if entry == path then
			table.remove(self.activeNotiTbl, idx)
		end
	end

	for idx, entry in ipairs(self.passiveNotiTbl) do
		if entry == path then
			table.remove(self.passiveNotiTbl, idx)
		end
	end

--[[
	Logger.log('Daemon', 'error', 'removeNotification path=[' .. path .. ']')
	Logger.log('Daemon', 'error', 'activeNotiTbl removeNotification Num Of entries' .. #self.activeNotiTbl )
	Logger.log('Daemon', 'error', '-----------------------------------------------')
	for i, entry in ipairs(self.activeNotiTbl) do
		Logger.log('Daemon', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Daemon', 'error', '-----------------------------------------------')

	Logger.log('Daemon', 'error', 'passiveNotiTbl removeNotification Num Of entries' .. #self.passiveNotiTbl )
	Logger.log('Daemon', 'error', '-----------------------------------------------')
	for i, entry in ipairs(self.passiveNotiTbl) do
		Logger.log('Daemon', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Daemon', 'error', '-----------------------------------------------')
--]]

	return 0
end

local function triggerPassiveNotif (self)

	for _, path in ipairs(self.passiveNotiTbl) do
		local node = self.tree:find(path)

		if node then
			local ret, value = node:getValue(path)

			if ret == 0 and node.value ~= value then
				node.value = value
				self:asyncParameterChange(node, path, value)
			end
		end
	end
end

local function triggerActiveNotif (self)

	for _, path in ipairs(self.activeNotiTbl) do
		local node = self.tree:find(path)

		if node then
			local ret, value = node:getValue(path)

			if ret == 0 and node.value ~= value then
				node.value = value
				self:asyncParameterChange(node, path, value)
			end
		end
	end
end

-- update paramPollWait only when desiredWait is shorter
local function updateParamPollWait (self, desiredWait)
	if not desiredWait then
		return
	end
	if not self.paramPollWait or self.paramPollWait > desiredWait then
		self.paramPollWait = desiredWait
	end
end

local function reqBulkDataEncodig(self)
	if conf.enabledBulkDataCollection == true then
		self.bulkDataInfo._reqBulkDataEncodig = true
	end
end

local function isBulkDataEncodigRequested(self)
	if conf.enabledBulkDataCollection == true then
		if luardb.get("tr069.bulkData.config.encoding_trigger") == "1" then
			luardb.set("tr069.bulkData.config.encoding_trigger", "0")
			return true
		end
		return self.bulkDataInfo._reqBulkDataEncodig == true
	else
		return false
	end
end

local function resetBulkDataEncodingReq(self)
	if conf.enabledBulkDataCollection == true then
		self.bulkDataInfo._reqBulkDataEncodig = false
	end
end

function Daemon.Client.new(acs, tree, host)
	local client = {
		running = true,
		informRequired = false,
		informRetryAt = nil,
		inSession = false,
		lastInform = nil,
		retryLimit = 8,
		bulkDataInfo = {},

		acs = acs,
		tree = tree,
		host = host,

		tasks = {},
		parameterWatchers = {},

		passiveNotiTbl = {},
		activeNotiTbl = {},

		paramPollWait = nil,
		updateParamPollWait = updateParamPollWait,

		init = init,

		addTaskType = addTaskType,
		removeTaskType = removeTaskType,
		addTask = addTask,
		removeTask = removeTask,
		isTaskQueued = isTaskQueued,
		runTasks = runTasks,

		addEvent = addEvent,

		addNotification = addNotification,
		removeNotification = removeNotification,
		triggerPassiveNotif = triggerPassiveNotif,
		triggerActiveNotif = triggerActiveNotif,

		reqBulkDataEncodig = reqBulkDataEncodig,
		isBulkDataEncodigRequested = isBulkDataEncodigRequested,
		resetBulkDataEncodingReq = resetBulkDataEncodingReq,

		freeMemory = freeMemory,
		poll = poll,
		asyncInform = asyncInform,
		asyncParameterChange = asyncParameterChange,
		stop = stop,

	}
	for _, taskType in ipairs(_builtInTasks) do
		client:addTaskType(taskType)
	end

if conf.enabledBulkDataCollection == true then
	client.mBulkDataParam = require("BulkData.Parameter")
	client.mBulkDataDaemon = require("BulkData.Daemon")
	client.mJSON = require("JSON")
	client.mUrl = require("socket.url")
end

	return client
end

return Daemon.Client

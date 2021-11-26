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

require('CWMP')
require('luardb')
require('rdbevent')
require('rdbobject')
require('tableutil')

Daemon.Host = {}

local function setStatus(self, status)
	luardb.set(conf.rdb.status, status)
end

local eventConf = {
	persist = true,
	idSelection = 'sequential'
}
local eventClass = rdbobject.getClass(conf.rdb.eventPrefix, eventConf)

local function addEvent(self, eventCode, commandKey)
	assert(CWMP.eventExists(eventCode), 'No such CWMP Event "' .. eventCode .. '".')
	for code, info in pairs(CWMP.events) do
		if table.contains(info.removedBy, eventCode) then
			self:deleteEventsByCode(code)
		end
	end

	-- Should report all of 'M ScheduleInform' events that have empty command key.
	if commandKey and (commandKey ~= '' or eventCode ~= 'M ScheduleInform') then
		self:deleteEventsByCodeAndKey(eventCode, commandKey)
	end

	luardb.lock()
	local event = eventClass:new()
	event.code = eventCode
	event.key = commandKey
	luardb.unlock()
	return event
end

local function getEvents(self)
	luardb.lock()
	local events = eventClass:getAll()
	luardb.unlock()
	return events
end

local function getEventsByCode(self, eventCode)
	local matchingEvents = {}
	local events = self:getEvents()
	for _, event in ipairs(events) do
		if event.code == eventCode then
			table.insert(matchingEvents, event)
		end
	end
	return matchingEvents
end

local function deleteEvent(self, event)
	luardb.lock()
	eventClass:delete(event)
	luardb.unlock()
end

local function deleteEventsByCode(self, eventCode)
	local events = self:getEventsByCode(eventCode)
	for _, event in ipairs(events) do
		eventClass:delete(event)
	end
end

local function deleteEventsByCodeAndKey(self, eventCode, commandKey)
	local events = self:getEventsByCode(eventCode)
	for _, event in ipairs(events) do
		if event.key == commandKey then
			eventClass:delete(event)
		end
	end
end

local function clearEventsByResponse(self, response)
	assert(CWMP.typeExists(response), 'No such CWMP Message type "' .. response .. '".')
	for code, info in pairs(CWMP.events) do
		if info.clearedBy == response then
			self:deleteEventsByCode(code)

			if conf.enableRebootReasonReport == true and code == "1 BOOT" then
				-- * Reboot reason is reported, so clear it.
				--
				-- * "service.tr069.lastreboot" is non-persist rdb variable,
				--   so it does not need to check value changes to set. (No concern on flash wear out)
				luardb.set("service.tr069.lastreboot", "")
			end
		end
	end
end

local function getBootstrap(self)
	return (luardb.get(conf.rdb.bootstrap) == '1')
end

local function setBootstrap(self, state)
	assert(type(state) == 'boolean', 'State must be boolean, got ' .. type(state))
	Logger.log('Daemon', 'notice', (state and 'Setting' or 'Clearing') .. ' the bootstrap flag.')
	luardb.set(conf.rdb.bootstrap, (state and '1' or '0'), 'p')
end

local function getFactoryResetReason(self)
	return luardb.get('service.factoryreset.reason') or ''
end

local function resetFactoryResetReason(self)
	luardb.set('service.factoryreset.reason', '')
end

local function getProtocolVersion(self)
	local ver = luardb.get(conf.rdb.version)
	return ver and ver or conf.cwmp.defaultVersion
end

local function reboot(self, commandKey)
	Logger.log('Daemon', 'notice', 'Reboot requested: ' .. commandKey)
	client:addTask('postSession', function()
		Logger.log('Daemon', 'notice', 'Doing reboot: ' .. commandKey)
		self:addEvent('M Reboot', commandKey)
		self:sync()
		luardb.set(conf.rdb.deviceResetReason, 'Acs Request')
		luardb.set(conf.rdb.deviceReset, '1') -- trigger reboot template
		client:stop()
	end)
	return 0
end

local function factoryReset(self)
	Logger.log('Daemon', 'notice', 'Factory reset requested.')
	client:addTask('postSession', function()
		Logger.log('Daemon', 'notice', 'Doing factory reset.')
		self:setBootstrap(false)  -- remove bootstrap-done marker
		self:sync()
		luardb.set(conf.rdb.factoryReset, '1') -- trigger factory reset template
		client:stop()
	end)
	return 0
end

local function sync(self)
	os.execute(conf.fs.syncScript)
end

local function onRequest(self, func)
	assert(type(func) == 'function', 'Host.onRequest callback must be a function.')
	luardb.set(conf.rdb.requestPrefix .. '.trigger', '0')
	rdbevent.onChange(conf.rdb.requestPrefix .. '.trigger', function(key, value)
		if value == '1' then
			Logger.log('Daemon', 'notice', 'ACS connection requested.')
			func()
			luardb.set(conf.rdb.requestPrefix .. '.trigger', '0')
		end
	end)
end

local function requestLock(self, lock)
	assert(type(lock) == 'boolean', 'Lock must be boolean, got ' .. type(lock))
	Logger.log('Daemon', 'debug', (lock and 'Masking' or 'Unmasking') .. ' connection request daemon triggering.')
	luardb.set(conf.rdb.requestPrefix .. '.busy', (lock and '1' or '0'))
end

local function isRequestLocked(self)
	return (luardb.get(conf.rdb.requestPrefix .. '.busy') == '1')
end

local function setPause(self, pause)
	assert(type(pause) == 'boolean', 'Pause must be boolean, got ' .. type(pause))
	Logger.log('Daemon', 'debug', (pause and 'Pausing' or 'Unpausing') .. ' inform session establishment.')
	luardb.set(conf.rdb.pause, (pause and '1' or '0'))
end

local function isPaused(self)
	return (luardb.get(conf.rdb.pause) == '1')
end

local function onShutdown(self, func)
	assert(type(func) == 'function', 'Host.onShutdown callback must be a function.')
	rdbevent.onChange(conf.rdb.enable, function(key, value)
		if value == '0' then
			Logger.log('Daemon', 'notice', 'Service shutdown requested.')
			func()
		end
	end)
end

local function eventWait(self, timeout)
	luardb.wait(timeout)
	rdbevent.poll()
end

-- <start> Methods for schedule inform rdbobject
local schedInformConf = {
	persist = true,
	idSelection = 'sequential'
}
local schedInformClass = rdbobject.getClass('tr069.scheduleinform', schedInformConf)

-- Create ScheduleInform rdbobject instance.
--
-- @param commandKey "CommandKey" argument on ScheduleInform RPC
-- @param delaySec "DelaySeconds" argument on ScheduleInform RPC
--
-- @return new ScheduleInform rdbobject instance.
-- If there are instances that have same non-empty commandKey, the instances are removed.
local function addSchedInform(self, commandKey, delaySec)
	assert(type(commandKey) == "string",
		"Invalid CommandKey: " .. tostring(commandKey))
	assert(tonumber(delaySec) and tonumber(delaySec) > 0,
		"Invalid DelaySeconds: " .. tostring(delaySec))

	if commandKey ~= '' then
		self:deleteSchedInformsByKey(commandKey)
	end

	local sched = schedInformClass:new()
	sched.key = commandKey
	sched.delay = delaySec
	sched.created = os.time()
	sched.reportAt = tonumber(sched.created) + (tonumber(sched.delay) or 0)
	return sched
end

-- get all of ScheduleInform rdbobject instances.
--
-- @return table of ScheduleInform rdbobject instances.
local function getSchedInforms(self)
	return schedInformClass:getAll()
end

-- get all of ScheduleInform rdbobject instances that have the same commandkey.
--
-- @return table of ScheduleInform rdbobject instances that have the same commandkey.
local function getSchedInformsByKey(self, commandKey)
	local matchingScheds = {}
	local scheds = self:getSchedInforms()
	for _, sched in ipairs(scheds) do
		if sched.key == commandKey then
			table.insert(matchingScheds, sched)
		end
	end
	return matchingScheds
end

-- get all of ScheduleInform rdbobject instances that need to report.
--
-- @param sysTime system time
--
-- @return table of ScheduleInform rdbobject instances that need to report at given system time.
local function getSchedInformsNeedToReport(self, sysTime)
	local criterion = tonumber(sysTime)
	assert(criterion, 'sysTime should be number')

	local matchingScheds = {}
	local scheds = self:getSchedInforms()
	for _, sched in ipairs(scheds) do
		reportAt = tonumber(sched.reportAt)
		if reportAt <= criterion then
			table.insert(matchingScheds, sched)
		end
	end
	return matchingScheds
end

-- delete ScheduleInform rdbobject instance
--
-- @param instance ScheduleInform rdbobject instance to remove
local function deleteSchedInform(self, instance)
	schedInformClass:delete(instance)
end

-- delete ScheduleInform rdbobject instances that have commandKey
--
-- @param commandKey specify commandKey to delete instances
local function deleteSchedInformsByKey(self, commandKey)
	local scheds = self:getSchedInformsByKey(commandKey)
	for _, sched in ipairs(scheds) do
		schedInformClass:delete(sched)
	end
end
-- <end> Methods for schedule inform rdbobject

-- Bulk data collection
local function onBulkDataEncoding(self)
	local rdbEncodingTrigger = "tr069.bulkData.config.encoding_trigger"
	local value = luardb.get(rdbEncodingTrigger)
	if value == "1" then -- already triggered before onChange watcher is registered.
		client:reqBulkDataEncodig()
		luardb.set(rdbEncodingTrigger, "0")
	end
	rdbevent.onChange(rdbEncodingTrigger, function(key, value)
		if value == "1" then
			client:reqBulkDataEncodig()
			luardb.set(rdbEncodingTrigger, "0")
		end
	end)
end

function Daemon.Host.new()
	local host = {
		['setStatus'] = setStatus,

		['addEvent'] = addEvent,
		['getEvents'] = getEvents,
		['getEventsByCode'] = getEventsByCode,
		['deleteEvent'] = deleteEvent,
		['deleteEventsByCode'] = deleteEventsByCode,
		['deleteEventsByCodeAndKey'] = deleteEventsByCodeAndKey,
		['clearEventsByResponse'] = clearEventsByResponse,
		
		['getBootstrap'] = getBootstrap,
		['setBootstrap'] = setBootstrap,
		
		['getProtocolVersion'] = getProtocolVersion,
		
		['reboot'] = reboot,
		['factoryReset'] = factoryReset,
		['sync'] = sync,
		
		['onRequest'] = onRequest,
		['requestLock'] = requestLock,
		['isRequestLocked'] = isRequestLocked,
		
		['setPause'] = setPause,
		['isPaused'] = isPaused,
		
		['onShutdown'] = onShutdown,

		['eventWait'] = eventWait,

		-- <start> Methods for schedule inform rdbobject
		['addSchedInform'] = addSchedInform,
		['getSchedInforms'] = getSchedInforms,
		['getSchedInformsByKey'] = getSchedInformsByKey,
		['getSchedInformsNeedToReport'] = getSchedInformsNeedToReport,
		['deleteSchedInform'] = deleteSchedInform,
		['deleteSchedInformsByKey'] = deleteSchedInformsByKey,
		-- <end> Methods for schedule inform rdbobject

		['getFactoryResetReason'] = getFactoryResetReason,
		['resetFactoryResetReason'] = resetFactoryResetReason,

		-- Bulk data collection
		['onBulkDataEncoding'] = onBulkDataEncoding,

	}
	-- to interrrupt Client.poll
	rdbevent.onChange(conf.rdb.serivcePulse, function(key, value) end)
	return host
end

return Daemon.Host

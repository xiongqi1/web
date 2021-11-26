#!/usr/bin/env lua

require('tableutil')

require('luardb')
require('luasyslog')
require('rdbobject')
rpc = require('rdbrpcserver')

----
-- Config
----

conf = {
	-- RDB bindings
	alarmClass = 'alarms',
	alarmClassConfig = { persist = false, idSelection = 'sequential' },
	alarmEndpoint = 'alarmd',
	alarmLimitVar = 'alarmd.max_alarms',
	alarmTimestampVar = 'alarmd.timestamp',

	-- limits on alarm count
	alarmLimitMin = 1,
	alarmLimitMax = 1000,
	alarmLimitDefault = 20,
	alarmLimit = 20
}


----
-- Alarm Management Functions
----

-- sort alarms
function compareAlarmsByRaised(a, b)
	return (tonumber(a.raised or 0) < tonumber(b.raised or 0))
end

-- remove the least-recently-raised alarms to keep at most conf.alarmLimit
-- conf.alarmLimit is dynamically configurable via conf.alarmLimitVar RDB variable
function cleanupLRR()
	local allAlarms = alarms:getAll()
	local alarmCount = table.count(allAlarms)
	if alarmCount > conf.alarmLimit then
		-- trim oldest alarms
		table.sort(allAlarms, compareAlarmsByRaised)
		local numToDrop = alarmCount - conf.alarmLimit
		luasyslog.log('warning', 'Alarm table overflow, dropping oldest alarms.')
		for i = 1, numToDrop do
			luasyslog.log('notice', 'Alarm ' .. alarms:getId(allAlarms[i]) .. ' dropped.')
			alarms:delete(allAlarms[i])
		end
	end
end

-- touch the alarm timestamp variable
-- this notifies other parts of the system that care about the alarm table
function touch(stamp)
	luardb.set(conf.alarmTimestampVar, stamp)
end

-- raise a new alarm
function raise(serv, cmd, args)
	if not args.subsys then error('Argument "subsys" is manditory.') end
	if not args.message then error('Argument "message" is manditory.') end

	local alarm = alarms:new()
	local id = alarms:getId(alarm)
	alarm.raised = os.date('%s')
	alarm.cleared = ''
	alarm.subsys = args.subsys
	alarm.message = args.message
	cleanupLRR()
	touch(alarm.raised)
	luasyslog.log('alert', 'New alarm ' .. id .. ': ' .. args.subsys .. ': ' .. args.message)
	return id
end

-- clear an existing alarm
function clear(serv, cmd, args)
	if not args.id then error('Argument "id" is manditory.') end
	local alarm = alarms:getById(args.id)
	alarm.cleared = os.date('%s')
	touch(alarm.cleared)
	luasyslog.log('notice', 'Alarm ' .. args.id .. ' cleared.')
	return 'Alarm ' .. args.id .. ' cleared.'
end

-- delete an existing alarm
function delete(serv, cmd, args)
	if not args.id then error('Argument "id" is manditory.') end
	local alarm = alarms:getById(args.id)
	alarms:delete(alarm)
	touch(os.date('%s'))
	luasyslog.log('notice', 'Alarm ' .. args.id .. ' deleted.')
	return 'Alarm ' .. args.id .. ' deleted.'
end


----
-- Daemon Process
----

luasyslog.open('alarmd', 'LOG_DAEMON')
luasyslog.log('info', 'Started.')

-- RDB object collection of alarm instances
alarms = rdbobject.getClass(conf.alarmClass, conf.alarmClassConfig)

-- max alarms live RDB parameter
function alarmLimitChange(k, v)
	local maxAlarms = tonumber(v) or conf.alarmLimitDefault
	if maxAlarms < conf.alarmLimitMin then maxAlarms = conf.alarmLimitMin end
	if maxAlarms > conf.alarmLimitMax then maxAlarms = conf.alarmLimitMax end
	if maxAlarms ~= conf.alarmLimit then
		luasyslog.log('notice', 'Alarm limit changed from ' .. conf.alarmLimit .. ' to ' .. maxAlarms .. '.')
		conf.alarmLimit = maxAlarms
		cleanupLRR()
		touch(os.date('%s'))
	end
	if v ~= tostring(maxAlarms) then
		luardb.set(conf.alarmLimitVar, maxAlarms)
	end
end
alarmLimitChange(conf.alarmLimitVar, luardb.get(conf.alarmLimitVar))
luardb.watch(conf.alarmLimitVar, alarmLimitChange)

-- RPC service for managing alarms
serv = rpc:new(conf.alarmEndpoint)
--serv.debug = true
serv:addCommand('raise', raise)
serv:addCommand('clear', clear)
serv:addCommand('delete', delete)
serv:start()

-- main loop
rpc:run()

luasyslog.log('info', 'Ended.')

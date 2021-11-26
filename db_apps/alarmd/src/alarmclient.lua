require('rdbobject')
require('rdbrpcclient')

local function inst_clear(self)
	return self._client.rpc:invoke('clear', { ['id'] = self.id }, self._client.timeout)
end

local function inst_delete(self)
	return self._client.rpc:invoke('delete', { ['id'] = self.id }, self._client.timeout)
end

local function _rdbobjToInstance(self, alarm)
	return {
		_client = self,

		id = self.rdb:getId(alarm),
		raised = alarm.raised,
		cleared = alarm.cleared,
		subsys = alarm.subsys,
		message = alarm.message,

		['clear'] = inst_clear,
		['delete'] = inst_delete,
	}
end

local function getAll(self)
	local ret = {}
	for _, alarm in ipairs(self.rdb:getAll()) do
		table.insert(ret, _rdbobjToInstance(self, alarm))
	end
	return ret
end

local function getById(self, id)
	local ret = {}
	local alarm = self.rdb:getById(id)
	return _rdbobjToInstance(self, alarm)
end

local function getBySubsystem(self, subsys)
	local ret = {}
	for _, alarm in ipairs(self.rdb:getByProperty('subsys', subsys)) do
		table.insert(ret, _rdbobjToInstance(self, alarm))
	end
	return ret
end

local function raise(self, subsystem, message)
	return self.rpc:invoke('raise', { subsys = subsystem, message = message }, self.timeout)
end

local function flush(self)
	local alarms = self:getAll()
	for _, alarm in ipairs(alarms) do
		alarm:delete()
	end
end

alarmclient = {}

function alarmclient.new(timeout, rpcEndpoint, rdbClass)
	timeout = timeout or 20
	rpcEndpoint = rpcEndpoint or 'alarmd'
	rdbClass = rdbClass or 'alarms'

	local client = {
		rdb = rdbobject.getClass(rdbClass),
		rpc = rdbrpcclient:new(rpcEndpoint),
		timeout = timeout,

		['getAll'] = getAll,
		['getById'] = getById,
		['getBySubsystem'] = getBySubsystem,
		['raise'] = raise,
		['flush'] = flush,
	}
	return client
end

return alarmclient

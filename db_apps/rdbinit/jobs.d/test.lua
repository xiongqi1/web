require('tableutil')

local job = {
	['name'] = 'test',
	['description'] = 'Example job maintaining a bunch of randomly sleeping processes.',

	['minSleep'] = 1,	-- shortest sleep, seconds
	['maxSleep'] = 10,	-- longest sleep, seconds
	['procCount'] = 0,	-- number of processes to maintain
	['maxProcs'] = -1,
	['cmd'] = '/bin/sleep',

	['workers'] = {}
}

local procsStarted = 0

local function _startWorker(self)
	if self.maxProcs > 0 and procsStarted >= self.maxProcs then return end
	local howLong = math.random(self.minSleep, self.maxSleep)
	local args = { [0] = self.cmd, [1] = howLong }
	local pid = self.engine:startProc(self, args, {})
	procsStarted = procsStarted + 1
	self.workers[pid] = pid
	print('new worker', pid, howLong)
end

local function _workerDeath(self, data)
	print('worker died', data.pid, data.exit or data.signalName)
--	print(table.tostring(data))
	self.workers[data.pid] = nil
end

function job.init(self, engine)
	self.engine = engine
end

function job.event(self, eventType, data)
	if eventType == 'sys.init' then
		for i = 1,self.procCount do
			_startWorker(self)
		end
	elseif eventType == 'sys.term' then
		
	elseif eventType == 'proc.start' then
		-- ignored
	elseif eventType == 'proc.death' or eventType == 'proc.lost' then
		if self.workers[data.pid] then
			_workerDeath(self, data)
--			self:poll()
			_startWorker(self)
		end
	else
		error('Unexpected event "' .. eventType .. '".')
	end
end

function job.poll2(self)
--	while table.count(self.workers) < self.procCount and procsStarted < self.maxProcs do
--		_startWorker(self)
--	end
	if table.count(self.workers) < self.procCount then
		_startWorker(self)
	end
end

return job

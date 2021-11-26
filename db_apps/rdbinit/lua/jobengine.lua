require('lfs')
require('luasyslog')
require('tableutil')
require('rdbevent')

local function _loadJob(self, fileName)
	local file = self.conf.jobDir .. '/' .. fileName
	if self.conf.debug then
		luasyslog.log('debug', 'Loading job file "' .. file .. '".')
	end
	local job = dofile(file)
	if type(job) ~= 'table' then error('Job did not return a table.') end
	
	-- jobs with a type get passed through their manager 1st
	if job.type then
		if not self.jobTypes[job.type] then
			error('Job specifies an unknown manager type "' .. job.type .. '".')
		end
		job = self.jobTypes[job.type](job)
	end
	
	-- validate basics of job
	local manditoryMembers = {
		['name'] = 'string',
		['description'] = 'string',
		['init'] = 'function',
--		['poll'] = 'function',
		['event'] = 'function',
	}
	for memberName, memberType in ipairs(manditoryMembers) do
		if not job[memberName] then error('Job has no "' .. memberName .. '" member.') end
		if type(job[memberName]) ~= memberType then error('Job member "' .. memberName .. '" must be of type ' .. memberType .. '.') end
	end
	
	return job
end

local function _loadJobs(self)
	if self.conf.debug then
		luasyslog.log('debug', 'Loading jobs from "' .. self.conf.jobDir .. '".')
	end
	for file in lfs.dir(self.conf.jobDir) do
		if file:match('%.lua$') then
--			local ret, jobOrError = true, _loadJob(self, file)
			local ret, jobOrError = pcall(_loadJob, self, file)
			if ret then
				luasyslog.log('info', 'Loaded job: ' .. jobOrError.name .. '(' .. file .. ').')
				self:addJob(jobOrError)
			else
				luasyslog.log('error', 'Error loading job "' .. file .. '": ' .. jobOrError)
			end
		end
	end
end

local function addJob(self, job)
	if table.contains(self.jobs, job) then
		error('Engine already has job ' .. job.name .. ' registered.')
	end
	local ret, msg = pcall(job.init, job, self) -- job:init(self)
	if not ret then
		luasyslog.log('error', 'Job "' .. job.name .. '" init() error: ' .. msg)
	else
		table.insert(self.jobs, job)
	end
end

local function rdbSubscribe(self, key, eventSubtype)
end

local function event(self, eventType, data)
	if self.debug then
		luasyslog.log('debug', 'Event: ' .. eventType .. ', ' .. table.tostring(data))
	end
	if data.proc and data.proc.job then
		local ret, msg = pcall(data.proc.job.event, data.proc.job, eventType, data) -- data.proc.job:event(eventType, data)
		if not ret then
			luasyslog.log('error', 'Job "' .. data.proc.job.name .. '" event(' .. eventType .. ') error: ' .. msg) 
		end
	else
		for _, job in ipairs(self.jobs) do
			local ret, msg = pcall(job.event, job, eventType, data) -- job:event(eventType, data)
			if not ret then
				luasyslog.log('error', 'Job "' .. job.name .. '" event(' .. eventType .. ') error: ' .. msg) 
			end
		end
	end
end

local function init(self)
	local jobTypes = { 'rdbmanaged' }
	for _, jobType in ipairs(jobTypes) do
		self.jobTypes[jobType] = require('job_' .. jobType)
	end
	_loadJobs(self)
	if self.conf.initEvent then
		self:event(self.conf.initEvent, self.conf.initArgs)
	end
end

local function startProc(self, job, args, env)
	local proc = {}
	proc.job = job
	proc.args = args
	proc.env = env
	proc.pid = procman.fork(args, env)
	
	if self.conf.debug then
		luasyslog.log('debug', 'procman.form(' .. args[0] .. ') = ' .. proc.pid)
	end

	self.procs[proc.pid] = proc
	self:event('proc.start', proc)
	
	return proc.pid
end

local function signalProc(self, job, pid, signal)
	local proc = self.procs[pid]
	if not proc then
		luasyslog.log('notice', 'Job "' .. job.name .. '" signalling unknown process ' .. pid .. '.')
	elseif proc.job ~= job then
		luasyslog.log('notice', 'Job "' .. job.name .. '" signalling job "' .. proc.job.name .. '" process.')
	end
	
	luaprocman.kill(pid, signal)
	if self.conf.debug then
		luasyslog.log('debug', 'procman.kill(' .. pid .. ', ' .. signal .. ')')
	end
end

local function _doWait(self, pid, proc)
	proc = proc or self.procs[pid]

	local ret, stat = pcall(procman.wait, pid, true)
	if not ret then
		if pid == -1 then
			-- no children to reap
		else
			-- misplaced process?
			luasyslog.log('error', 'procman.poll(' .. pid .. '): ' .. stat)
			stat = {}
			stat.status = 'lost'
			stat.proc = proc
			self:event('proc.lost', stat)
			self.procs[pid] = nil
		end
	elseif stat then
		if self.conf.debug then
			luasyslog.log('debug', 'procman.poll(' .. pid .. ') = ' .. stat.status)
		end
		-- normal status change
		stat.proc = proc
		if stat.status == 'exited' or stat.status == 'signalled' then
			self:event('proc.death', stat)
			self.procs[stat.pid] = nil
		elseif stat.status == 'stopped' then
			self:event('proc.stop', stat)
		elseif stat.status == 'continued' then
			self:event('proc.continue', stat)
		else
			error('Unexpected status "' .. stat.status .. '" from wait.')
		end
	else
		-- nothing has changed
		if self.conf.debug then
			luasyslog.log('debug', 'procman.poll(' .. pid .. ') = nil')
		end
	end
end

local function poll(self)

	-- poll all processes we manage for state change
	for pid, proc in pairs(self.procs) do
		_doWait(self, pid, proc)
	end

	-- poll for anything we may have inherited (or own)
	_doWait(self, -1, nil)
	
	-- give all jobs a poll call if they implement it
	for _, job in ipairs(self.jobs) do
		if job.poll then
			local ret, msg = pcall(job.poll, job) -- job:poll()
			if not ret then
				luasyslog.log('error', 'Job "' .. job.name .. '" poll() error: ' .. msg)
			end
		end
	end
	
--	print('jobs', table.count(self.jobs), 'procs', table.count(self.procs))
end

jobengine = {}

function jobengine.new(conf)
	conf = conf or {
		debug = false,
		jobDir = '/etc/rdbinit.d',
		initEvent = 'sysinit',
		initArgs = {}
	}
	
	local engine = {
		['conf'] = conf,
		['running'] = true,
		['jobTypes'] = {},
		['jobs'] = {},
		['procs'] = {},

		['addJob'] = addJob,
		['rdbSubscribe'] = rdbSubscribe,

		['init'] = init,
		['poll'] = poll,
		['event'] = event,
		['startProc'] = startProc,
		['signalProc'] = signalProc,
	}
	
	return engine
end

return jobengine

local stateHandlers = {
	['init'] = function(job)
	end,
}

local function init(job, engine)
	job.status = 'init'
	engine:rdbSubscribe(job, job.rdbEnable, 'enable')
	if job.rdbConfig then
		for _, key in ipairs(job.rdbConfig) do
			engine:rdbSubscribe(job, key, 'config')
		end
	end
end

local function event(job, type, data)
	if type == 'rdb.enable' then
	elseif type == 'rdb.config' then
	else
		error('Unknown event type "' .. type .. '".')
	end
end

local function poll(job)
end



local function _validateAndDefault(job)
	local members = {
		cmd		= { type = 'string',	required = true },
		args		= { type = 'table',	required = false,	default = {} },
		env		= { type = 'table',	required = false,	default = { PATH = '/bin:/usr/bin' } },
		respawn		= { type = 'boolean',	required = false,	default = false },
		rdbEnable	= { type = 'string',	required = true },
		rdbStatus	= { type = 'string',	required = false },
		stdin		= { type = 'string',	required = false },
		stdout		= { type = 'string',	required = false },
		stderr		= { type = 'string',	required = false },
	}

	for memberName, memberType in ipairs(members) do
		if not job[memberName] and memberType.default then
			job[memberName] = memberType.default
		end
		if memberType.required and not job[memberName] then
			error('Job "' .. job.name .. '" has no ' .. memberName .. '.')
		end
		if job[memberName] and type(job[memberName]) ~= memberType.type then
			error('Job "' .. job.name .. '" ' .. memberName .. ' is not ' .. memberType.type .. '.')
		end
	end
end

return function(job)
	_validateAndDefault(job)

	job.init = init
	job.event = event
	job.poll = poll
	
	job.pid = -1

	return job
end

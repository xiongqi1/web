--[[
This script handles objects/parameters under Device.PeriodicStatistics.

  SampleSet.{i}.
  SampleSet.{i}.Parameter.{i}.

Copyright (C) 2016 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Logger")
require("rdbobject")

------------------local variable----------------------------
local logSubsystem = "PeriodicStats"
local debug = conf.log.debug
debug = 7

local subRoot=conf.topRoot..'.PeriodicStatistics.'

-- we use rdbobjects to store sample sets
local sampleSetRdbPrefix = 'tr069.periodicstats.sampleset'
local sampleSetClassConfig = { persist = true, idSelection = 'nextLargest' }
-- each parameter is stored as an rdbobject in a sample set object
local sampleSetParamClassConfig = { persist = true,
                                    idSelection = 'nextLargest' }

local sampleSetObjectClass = rdbobject.getClass(sampleSetRdbPrefix,
                                                sampleSetClassConfig)

local collectTime = {} -- {set_id : next_collect_time}
local changedIds = {} -- changed sample set ids

------------------------------------------------------------

------------------local function prototype------------------

------------------------------------------------------------

------------------local function definition------------------

Logger.addSubsystem(logSubsystem)

-- debug info log utility
local function dinfo(...)
    if debug > 1 then
        local printResult = ''
        for _, v in ipairs(arg) do
            printResult = printResult .. tostring(v) .. '\t'
        end
        Logger.log(logSubsystem, 'info', printResult)
    end
end

-- get the sample set object corresponding to node
local function getSampleSetObject(node)
    local id = tonumber(node.name)
    return sampleSetObjectClass:getById(id)
end

-- get the parameter object class corresponding to sample set object id
local function getParamClass(id)
    return rdbobject.getClass(sampleSetRdbPrefix .. '.' .. id .. '.param',
                              sampleSetParamClassConfig)
end

-- get the parameter object corresponding to node
local function getParamObject(node)
    local pid = tonumber(node.parent.parent.name)
    local paramClass = getParamClass(pid)
    local id = tonumber(node.name)
    return paramClass:getById(id)
end

-- add a sample set object id to changedIds table if it does not exist
local function addChangedId(id)
    id = tonumber(id)
    if not table.contains(changedIds, id) then
        table.insert(changedIds, id)
    end
end

-- clear a sample set object (reset all records)
local function clearSampleSet(obj)
    local current_time = os.time()
    obj.start_time = current_time
    obj.end_time = current_time
    obj.sample_seconds = ''
    local pid = sampleSetObjectClass:getId(obj)
    local paramClass = getParamClass(pid)
    local params = paramClass:getAll()
    for _, p in pairs(params) do
        p.sample_seconds = ''
        p.suspect_data = ''
        p.values = ''
    end
end

-- append a value to a comma separated string msg
-- subject to the max number of elements: limit
-- return new_msg, numRemoved
-- numRemoved is the number of entries removed from msg due to limit.
local function appendValue(msg, val, limit)
    val = tostring(val)
    if not msg or msg == '' then
        return val, 0
    end
    local t = msg:explode(',')
    table.insert(t, val)
    local numRemoved = 0
    if limit then
        limit = tonumber(limit)
        while #t > limit do -- remove front elements until it meets the limit
            table.remove(t, 1)
            numRemoved = numRemoved + 1
        end
    end
    return table.concat(t, ','), numRemoved
end

-- collect a sample set and store in obj
local function collectSampleSet(obj)
    -- collect a sample set {cid}
    local cid = sampleSetObjectClass:getId(obj)
    local current_time = os.time()
    dinfo('Collect sample set '..cid, 'collect time '..collectTime[cid],
          'now '..current_time)
    local sample_interval = obj.sample_interval or 3600
    local report_samples = obj.report_samples or 24
    local numRemoved
    obj.sample_seconds, numRemoved = appendValue(obj.sample_seconds,
                                                 sample_interval,
                                                 report_samples)
    if numRemoved > 0 then -- adjust start time if overflow
        obj.start_time = current_time - sample_interval * report_samples
    end

    local paramClass = getParamClass(cid)
    local params = paramClass:getAll()
    dinfo('Got ' .. #params .. ' params')
    for _, param in pairs(params) do
        if param.enable == '1' then
            if param.reference and param.reference ~= '' then
                dinfo('Getting ' .. param.reference)
                local status, value = pcall(paramTree.getValue, paramTree, param.reference)
                if status then
                    -- get value succeeded
                    dinfo('Got ' .. param.reference .. ': ' .. value)
                    local interval = tostring(((param.interval_skips or 0) + 1) * sample_interval)
                    param.sample_seconds = appendValue(param.sample_seconds, interval, report_samples)
                    param.suspect_data = appendValue(param.suspect_data, '0', report_samples)
                    param.values = appendValue(param.values, value, report_samples)
                    param.interval_skips = 0
                else
                    dinfo('Failed to get ' .. param.reference)
                    param.interval_skips = (param.interval_skips or 0) + 1
                end
            end
        end
    end
    obj.end_time = current_time
end

-- poll function to be added in a parameterPoll task
local function pollPeriodicStats(task)
    local current_time = os.time()

    -- check if any sample set configuration is changed
    for _, id in pairs(changedIds) do
        local obj = sampleSetObjectClass:getById(id)
        clearSampleSet(obj)
        if obj.enable == '1' then
            collectTime[id] = current_time + (obj.sample_interval or 3600)
        else
            collectTime[id] = nil
        end
    end
    changedIds = {}

    local interval, interval2 -- the shortest two intervals to wait
    local cid -- the object id corresponding to the shortest wait
    -- determine the interval to wait and the next set to collect
    for id, ct in pairs(collectTime) do
        local td = ct - current_time
        if not interval or interval > td then
            interval2 = interval
            interval = td
            cid = id
        elseif not interval2 or interval2 > td then
            interval2 = td
        end
    end

    local waitTime
    -- collect a sample when it is time
    if interval and interval <= 0 then
        local obj = sampleSetObjectClass:getById(cid)
        collectSampleSet(obj)
        local td = obj.sample_interval
        if not td or td == '' then
            td = 3600
        else
            td = tonumber(td)
        end
        collectTime[cid] = collectTime[cid] + td
        if not interval2 or td < interval2 then
            waitTime = td
        else
            waitTime = interval2
        end
    else
        waitTime = interval
    end

    -- update param poll wait time if necessary
    if waitTime then
        client:updateParamPollWait(waitTime)
    end
end

------------------------------------------------------------

return {
    [subRoot .. 'SampleSet'] = {
        init = function(node, name)
            dinfo('SampleSet init')
            node:setAccess('readwrite')
            local ids = sampleSetObjectClass:getIds()
            for _, id in ipairs(ids) do
                local instance = node:createDefaultChild(id)
                addChangedId(id)
            end
            if client:isTaskQueued('parameterPoll',
                                   pollPeriodicStats) ~= true then
                client:addTask('parameterPoll', pollPeriodicStats, true)
            end
            return 0
        end,

        create = function(node, name)
            dinfo('SampleSet create ' .. name)
            local obj = sampleSetObjectClass:new()
            obj.enable = '0'
            obj.name = ''
            obj.sample_interval = 3600
            obj.report_samples = 24
            obj.start_time = 0
            obj.end_time = 0
            obj.sample_seconds = ''
            local id = sampleSetObjectClass:getId(obj)
            local instance = node:createDefaultChild(id)
            instance:recursiveInit()
            return 0, id
        end,
    },

    [subRoot .. 'SampleSet.*'] = {
        init = function(node, name)
            dinfo('SampleSet.* init', name)
            node.value = node.default
            return 0
        end,
        delete = function(node, name)
            dinfo('SampleSet.* delete', name)
            local id = tonumber(node.name)
            local paramClass = getParamClass(id)
            local params = paramClass:getAll()
            for _, p in ipairs(params) do
                paramClass:delete(p)
            end
            local obj = sampleSetObjectClass:getById(id)
            if obj.enable == '1' then
                -- we need to inform the daemon
                addChangedId(id)
            end
            sampleSetObjectClass:delete(obj)
            node.parent:deleteChild(node)
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.Enable'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.enable or '0'
        end,
        set = function(node, name, value)
            local obj = getSampleSetObject(node.parent)
            if obj.enable ~= value then
                local id = sampleSetObjectClass:getId(obj)
                addChangedId(id)
            end
            obj.enable = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.Name'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.name or ''
        end,
        set = function(node, name, value)
            local obj = getSampleSetObject(node.parent)
            obj.name = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.SampleInterval'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.sample_interval or '3600'
        end,
        set = function(node, name, value)
            local obj = getSampleSetObject(node.parent)
            if obj.sample_interval ~= value then
                local id = sampleSetObjectClass:getId(obj)
                addChangedId(id)
            end
            obj.sample_interval = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.ReportSamples'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.report_samples or '24'
        end,
        set = function(node, name, value)
            local obj = getSampleSetObject(node.parent)
            obj.report_samples = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.ReportStartTime'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.start_time or '0'
        end,
    },

    [subRoot .. 'SampleSet.*.ReportEndTime'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.end_time or '0'
        end,
    },

    [subRoot .. 'SampleSet.*.SampleSeconds'] = {
        get = function(node, name)
            local obj = getSampleSetObject(node.parent)
            return 0, obj.sample_seconds or ''
        end,
    },

    [subRoot .. 'SampleSet.*.ParameterNumberOfEntries'] = {
        get = function(node, name)
            local pid = tonumber(node.parent.name)
            local paramClass = rdbobject.getClass(sampleSetRdbPrefix .. '.' .. pid .. '.param', sampleSetParamClassConfig)
            local ids = paramClass:getIds()
            return 0, tostring(#ids)
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter'] = {
        init = function(node, name)
            dinfo('SampleSet.*.Parameter init', name)
            node:setAccess('readwrite')
            local pid = tonumber(node.parent.name)
            local paramClass = getParamClass(pid)
            local ids = paramClass:getIds()
            for _, id in ipairs(ids) do
                local instance = node:createDefaultChild(id)
            end
            return 0
        end,

        create = function(node, name)
            dinfo('SampleSet.*.Parameter create', name)
            local pid = tonumber(node.parent.name)
            local paramClass = getParamClass(pid)
            local obj = paramClass:new()
            obj.enable = '0'
            obj.reference = ''
            obj.sample_seconds = ''
            obj.suspect_data = ''
            obj.values = ''
            local id = paramClass:getId(obj)
            local instance = node:createDefaultChild(id)
            instance:recursiveInit()
            return 0, id
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*'] = {
        init = function(node, name)
            dinfo('SampleSet.*.Parameter.* init', name)
            node.value = node.default
            return 0
        end,
        delete = function(node, name)
            dinfo('SampleSet.*.Parameter.* delete', name)
            local pid = tonumber(node.parent.parent.name)
            local paramClass = getParamClass(pid)
            local id = tonumber(node.name)
            local obj = paramClass:getById(id)
            paramClass:delete(obj)
            node.parent:deleteChild(node)
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*.Enable'] = {
        get = function(node, name)
            local obj = getParamObject(node.parent)
            return 0, obj.enable or '0'
        end,
        set = function(node, name, value)
            local obj = getParamObject(node.parent)
            obj.enable = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*.Reference'] = {
        get = function(node, name)
            local obj = getParamObject(node.parent)
            return 0, obj.reference or ''
        end,
        set = function(node, name, value)
            local obj = getParamObject(node.parent)
            obj.reference = value
            return 0
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*.SampleSeconds'] = {
        get = function(node, name)
            local obj = getParamObject(node.parent)
            return 0, obj.sample_seconds or ''
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*.SuspectData'] = {
        get = function(node, name)
            local obj = getParamObject(node.parent)
            return 0, obj.suspect_data or ''
        end,
    },

    [subRoot .. 'SampleSet.*.Parameter.*.Values'] = {
        get = function(node, name)
            local obj = getParamObject(node.parent)
            return 0, obj.values or ''
        end,
    },
}


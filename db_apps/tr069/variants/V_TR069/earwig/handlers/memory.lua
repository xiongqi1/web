require("Logger")
local logSubsystem = 'DeviceInfoMemProc'
Logger.addSubsystem(logSubsystem)

-------------------local variable-----------------------------
--------------------------------------------------------------


-------------------local function prototype-------------------
--------------------------------------------------------------


------------------local function definition-------------------
--------------------------------------------------------------
local function getMemInfo(memkey)
    memkey = memkey or 'MemTotal'
    local f = io.open('/proc/meminfo', 'r')
    if not f then
        Logger.log(logSubsystem, 'error', 'Failed to open /proc/meminfo')
        return CWMP.Error.InternalError, 'Unknown memory status'
    end
--[[ cat /proc/meminfo
     MemTotal:         123872 kB
     MemFree:           76396 kB
     ...
--]]
    for line in f:lines() do
        local key, value = line:match('^(.+):%s*(%d+)')
        if key == memkey then
            f:close()
            return 0, value
        end
    end
    f:close()
    return CWMP.Error.InvalidParameterValue, 'Unknown memory key ' .. memkey
end

local function split(instr, delim)
    delim = delim or '%s' -- default delimiter is whitespace
    local t = {}
    local i = 1
    for str in instr:gmatch('([^'..delim..']+)') do
        t[i] = str
        i = i + 1
    end
    return t
end

local function createPsTbl(subRoot, numOfEnt)
    Logger.log(logSubsystem, 'info', 'createPsTbl '..subRoot.name)
    -- update CPUUsage
    local f = io.open('/proc/stat', 'r')
    if not f then
        Logger.log(logSubsystem, 'error', 'Failed to open /proc/stat')
    else
        local cpuStat = split(f:read()) -- the first line is for average cpu stat
        f:close()
        local cpuUsage = '0'
        if tonumber(cpuStat[2]) and tonumber(cpuStat[4]) and tonumber(cpuStat[5]) then
            cpuUsage = tostring(math.floor((cpuStat[2] + cpuStat[4]) * 100 / (cpuStat[2] + cpuStat[4] + cpuStat[5])))
        end
        local node = subRoot.parent:getChild('CPUUsage')
        if not node then
            Logger.log(logSubsystem, 'error', 'Failed to find CPUUsage parameter')
            return
        end
        node.value = cpuUsage
    end

	local stateMap = { R = 'Running',
                       S = 'Sleeping',
                       D = 'Uninterruptible',
                       Z = 'Zombie',
                       T = 'Stopped'}
	local psFd = io.popen('ps', 'r')
    if not psFd then
        Logger.log(logSubsystem, 'error', 'Failed to launch ps')
        return
    end
    local instId = 1
    for line in psFd:lines() do
        repeat -- this is to mimic continue with break
            local pid = line:match('%S+') -- first non-whitespace word is pid
            if not tonumber(pid) then
                break
            end
            local prefix = '/proc/' .. pid .. '/'
            local f = io.open(prefix .. 'cmdline', 'r')
            if not f then
                break
            end
            local cmd = f:read('*all') or ''
            f:close()
            cmd = cmd:gsub('%z', ' '):match('^(.-)%s*$') -- convert to space delim
            f = io.open(prefix .. 'stat', 'r')
            if not f then
                break
            end
            local t = split(f:read('*all'))
            f:close()
            if cmd == '' and t[2] then
                cmd = t[2]
            end
            local state = stateMap[t[3]] or 'Running'
            local cpuTime = '0'
            if tonumber(t[14]) and tonumber(t[15]) then
                cpuTime = tostring((t[14] + t[15]) * 10) -- portable way to convert jiffies to ms?
            end
            local priority = tonumber(t[18]) or 20
            if priority < 0 then -- can be negative for real-time scheduling
                priority = - (priority + 1)
            end
            if priority > 99 then
                priority = 99
            end
            priority = tostring(priority)
            local size = '0'
            if tonumber(t[23]) then
                size = tostring(math.floor(t[23] / 1024)) -- unit of kB
            end

            -- now everything is ready for the new node
            local instance = subRoot:createDefaultChild(instId)
            instId = instId + 1
            for _, param in ipairs(instance.children) do
                if param.name == 'PID' then
                    param.value = pid
                elseif param.name == 'Command' then
                    param.value = cmd
                elseif param.name == 'Size' then
                    param.value = size
                elseif param.name == 'Priority' then
                    param.value = priority
                elseif param.name == 'CPUTime' then
                    param.value = cpuTime
                elseif param.name == 'State' then
                    param.value = state
                else
                    Logger.log(logSubsystem, 'error', 'Unknown param name: '..param.name)
                end
            end
            break
        until true
    end
    psFd:close()

    -- update ProcessNumberOfEntries
    if not numOfEnt then
        numOfEnt = subRoot.parent:getChild('ProcessNumberOfEntries')
    end
    if not numOfEnt then
        Logger.log(logSubsystem, 'error', 'Failed to find ProcessNumberOfEntries parameter')
        return
    end
    numOfEnt.value = tostring(instId - 1)
    Logger.log(logSubsystem, 'info', '#Ent: ' .. numOfEnt.value)
end

-- Clean up ProcessStatus.Process. subtree except default
local function cleanUpPsTbl(task)
    local node = task.data
    Logger.log(logSubsystem, 'info', 'cleanUpPsTbl called: ' .. node.name)
    local subRoot = node:getChild('Process')
    if not subRoot then
        Logger.log(logSubsystem, 'error', 'Failed to find Process collection')
        return
    end
    -- clean up Process subtree first (keeping default node!)
    for _, child in ipairs(subRoot.children) do
        if child.name ~= '0' then
            child.parent:deleteChild(child)
        end
    end
    -- create Process subtree
    createPsTbl(subRoot)
end

return {
    [conf.topRoot .. '.DeviceInfo.MemoryStatus.Total'] = {
        get = function(node, name)
            return getMemInfo('MemTotal')
        end,
    },

    [conf.topRoot .. '.DeviceInfo.MemoryStatus.Free'] = {
        get = function(node, name)
            return getMemInfo('MemFree')
        end,
    },

    [conf.topRoot .. '.DeviceInfo.ProcessStatus.*'] = {
        init = function(node, name, value)
            if client:isTaskQueued('preSession', cleanUpPsTbl) ~= true then
                client:addTask('preSession', cleanUpPsTbl, true, node.parent)
            end
            return 0
        end,
        get = function(node, name)
            return 0, node.value
        end,
    },

    [conf.topRoot .. '.DeviceInfo.ProcessStatus.Process.*.*'] = {
        get = function(node, name)
            return 0, node.value
        end,
    },

}

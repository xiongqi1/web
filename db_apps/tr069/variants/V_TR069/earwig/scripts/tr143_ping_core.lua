--[[
This module handles ipping diagnostics.
.ipping(host, reps, timeout, size)
ICMP echo request is sent according to routing/forwarding table.
ipping testing can be canceled by invoking setTestState('Canceled')
--]]

-- this relies on global table conf
package.path = conf.fs.code .. '/?.lua;' .. conf.fs.code .. '/classes/?/?.lua;' .. conf.fs.code .. '/classes/?.lua;' .. conf.fs.code .. '/utils/?.lua;' .. package.path

local debug = conf.log.debug or true
local log_level = conf.log.level or 'info'
debug = true
log_level = 'info'

-- debug info log utility. relies on luasyslog.open()
local function dinfo(...)
    if debug then
        local printResult = ''
        for _, v in ipairs{...} do
            printResult = printResult .. tostring(v) .. '\t'
        end
        luasyslog.log(log_level, printResult)
    end
end

local diagData = {} -- diagnostics result data

local rdbPrefix = conf.rdb.tr143Prefix .. '.ipping'

local testState -- this is to cancel in-progress testing
local function setTestState(state)
    testState = state
end

local function updateRdb()
    luardb.set(rdbPrefix .. '.success_count', diagData.successCount)
    luardb.set(rdbPrefix .. '.failure_count', diagData.failureCount)
    luardb.set(rdbPrefix .. '.avg_rsp_time', math.floor(diagData.avgRspTime))
    luardb.set(rdbPrefix .. '.min_rsp_time', math.floor(diagData.minRspTime))
    luardb.set(rdbPrefix .. '.max_rsp_time', math.floor(diagData.maxRspTime))
    luardb.set(rdbPrefix .. '.avg_rsp_time_detailed', math.floor(diagData.avgRspTime*1000))
    luardb.set(rdbPrefix .. '.min_rsp_time_detailed', math.floor(diagData.minRspTime*1000))
    luardb.set(rdbPrefix .. '.max_rsp_time_detailed', math.floor(diagData.maxRspTime*1000))
    if diagData.state == 'Complete' then
        luardb.set(rdbPrefix .. '.state', diagData.state)
    end
end

local function set_DiagnosticsState(val)
    diagData.state = val
    -- Completed will be set to rdb in updateRdb
    if val ~= 'Complete' then
        luardb.set(rdbPrefix .. '.state', val)
    end
end

local charset='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
-- generate a string of given len (from charset)
local function genString(len)
    len = tonumber(len)
    if not len or len <= 0 then
        return ''
    end
    return charset:rep(math.floor(len / #charset)) .. charset:sub(1, len % #charset)
end

local socket = require("socket")
-- check if host can be reached
local function hostCheck(host)
    local ipaddr = host
    if not host:find("^%d+%.%d+%.%d+%.%d+$") then
        -- resolve name
        ipaddr = socket.dns.toip(host)
        if not ipaddr then
            return nil, 'Error_CannotResolveHostName'
        end
    end
    -- check route
    local f = io.popen('ip route get ' .. ipaddr)
    if not f or not f:read() then
        return nil, 'Error_NoRouteToHost'
    end
    return true
end

local function ipping(host, reps, timeout, size, dscp, iface)
    host = host or 'localhost'
    local ret, err = hostCheck(host)
    if not ret then
        set_DiagnosticsState(err)
        return ret
    end

    reps = tonumber(reps) or 3
    timeout = tonumber(timeout) or 10000 -- milliseconds
    timeout = math.floor(timeout / 1000 + 0.5) -- convert to seconds
    if timeout < 1 then
        timeout = 1 -- we should wait for at least 1 sec
    end
    size = tonumber(size) or 56
    diagData.successCount = 0
    diagData.failureCount = 0
    diagData.avgRspTime = 0
    diagData.minRspTime = math.huge
    diagData.maxRspTime = 0
    local tx, rx, rt
    local data = genString(size)
    local cmd = 'oping -c 1 -d ' .. data .. ' -w ' .. timeout
    if dscp then
        cmd = cmd .. ' -Q ' .. (dscp * 4) -- DSCP is the 6 MSBs of DS field
    end
    if iface then
        cmd = cmd .. ' -D ' .. iface
    end
    cmd = cmd .. ' ' .. host .. ' 2>&1'
    dinfo('cmd=', cmd)
    for i = 1, reps do
        if testState == 'Canceled' or testState == 'Restart' then
            dinfo('ipping ' .. testState)
            if testState == 'Canceled' then
                set_DiagnosticsState('None')
            end
            return nil, testState
        end

        local start_time = os.time()

        local f = io.popen(cmd, 'r')
        if not f then
            dinfo('Failed: popen ' .. cmd)
            set_DiagnosticsState('Error_Internal')
            return nil
        end
        local s = f:read('*all')
        if not s then
            dinfo('Failed: read *all')
            set_DiagnosticsState('Error_Internal')
            f:close()
            return nil
        end
        f:close()
        --dinfo(s)
        tx, rx = s:match('(%d+)%s+packets%s+transmitted,%s*(%d+)%s+received')
        if tonumber(tx) ~= 1 then
            if s:find('unreachable') then
                dinfo('Failed: Network is unreachable')
                set_DiagnosticsState('Error_NoRouteToHost')
                return nil
            end
            if s:find('Name or service not known') then
                dinfo('Failed: Cannot resolve host name')
                set_DiagnosticsState('Error_CannotResolveHostName')
                return nil
            end
            break
        end
        if tonumber(rx) == 1 then
            diagData.successCount = diagData.successCount + 1
        else
            diagData.failureCount = diagData.failureCount + 1
        end
        rt = tonumber(s:match('RTT.+:%s*min%s*=%s*(%d+%.%d+)'))
        if rt then
            diagData.avgRspTime = diagData.avgRspTime + rt
            if diagData.minRspTime > rt then
                diagData.minRspTime = rt
            end
            if diagData.maxRspTime < rt then
                diagData.maxRspTime = rt
            end
        end
--      dinfo('tx='..tostring(tx)..', rx='..tostring(rx)..', rt='..tostring(rt))

        local end_time = os.time()
        if end_time == start_time then
            os.execute('sleep 1')
        end
    end

    if diagData.successCount ~= 0 then
        diagData.avgRspTime = diagData.avgRspTime / diagData.successCount
    end
    if diagData.minRspTime == math.huge then
        diagData.minRspTime = 0
    end

    dinfo('success='..tostring(diagData.successCount),
          'failure='..tostring(diagData.failureCount))
    dinfo('avg='..tostring(diagData.avgRspTime),
          'min='..tostring(diagData.minRspTime),
          'max='..tostring(diagData.maxRspTime))

    set_DiagnosticsState('Complete')
    updateRdb()

    return 1
end

return {
    ipping = ipping,
    setTestState = setTestState,
}

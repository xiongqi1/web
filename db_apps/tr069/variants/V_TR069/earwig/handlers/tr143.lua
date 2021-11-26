--[[
    This is the handler for Device.IP.Diagnostics.* of TR-181.
    Currently it implemented DownloadDiagnostics, UploadDiagnostics and IPPing.
--]]

require('stringutil')
require('tableutil')
require('Parameter')

------------------local variable----------------------------
local logSubsystem = "Diagnostics"
local debug = conf.log.debug
local log_level = conf.log.level or 'info'

local program_path = conf.fs.code .. "/scripts/"
local start_daemon = "start-stop-daemon -S -b -c :tr143 -m "
local stop_daemon = "start-stop-daemon -K -b -q "
local pidfile_path="/var/run/"

local subRoot = conf.topRoot .. '.IP.Diagnostics.'

local fixedInterface = conf.topRoot .. '.IP.Interface.1'

Logger.addSubsystem(logSubsystem)

-- debug info log utility
local function dinfo(...)
    if debug then
        local printResult = ''
        for _, v in ipairs{...} do
            printResult = printResult .. tostring(v) .. '\t'
        end
        Logger.log(logSubsystem, log_level, printResult)
    end
end

-- get rdb var name from cwmp path name
local function getRdbKey(node, path_name)
    local pathBits = path_name:explode('.')
    local fieldMapping = { -- cwmp field name -> rdb name
        -- function group
        ['DownloadDiagnostics'] = 'download',
        ['UploadDiagnostics']	= 'upload',
        ['IPPing'] = 'ipping',

        -- download
        ['DiagnosticsState'] = 'state',
        ['Interface'] = 'interface',
        ['DownloadURL'] = 'url',
        ['DownloadTransports'] = 'transports',
        ['DSCP'] = 'dscp',
        ['EthernetPriority'] = 'ethernet_priority',
        ['ROMTime'] = 'romtime',
        ['BOMTime'] = 'bomtime',
        ['EOMTime'] = 'eomtime',
        ['TestBytesReceived'] = 'test_bytes_rx',
        ['TotalBytesReceived'] = 'total_bytes_rx',
        ['TCPOpenRequestTime'] = 'tcp_open_req_time',
        ['TCPOpenResponseTime'] = 'tcp_open_rsp_time',

        -- upload
        ['UploadURL'] = 'url',
        ['UploadTransports'] = 'transports',
        ['TestFileLength'] = 'test_file_len',
        ['TestBytesSent'] = 'test_bytes_tx',
        ['TotalBytesSent'] = 'total_bytes_tx',

        -- ipping
        ['Host'] = 'host',
        ['NumberOfRepetitions'] = 'reps',
        ['Timeout'] = 'timeout',
        ['DataBlockSize'] = 'blksz',
        ['SuccessCount'] = 'success_count',
        ['FailureCount'] = 'failure_count',
        ['AverageResponseTime'] = 'avg_rsp_time',
        ['MinimumResponseTime'] = 'min_rsp_time',
        ['MaximumResponseTime'] = 'max_rsp_time',
        ['AverageResponseTimeDetailed'] = 'avg_rsp_time_detailed',
        ['MinimumResponseTimeDetailed'] = 'min_rsp_time_detailed',
        ['MaximumResponseTimeDetailed'] = 'max_rsp_time_detailed',
	}

    local rdb_name = conf.rdb.tr143Prefix .. '.' .. fieldMapping[pathBits[#pathBits-1]] .. '.' .. fieldMapping[pathBits[#pathBits]]
    -- dinfo('getRdbKey ' .. rdb_name)
    return rdb_name
end

-- get a rdb value and assign to node.value
-- if rdb is unset, set it with default value
local function getRdbValue(node, rdb_name)
    node.value = luardb.get(rdb_name);
    if not node.value or (node.value == '' and node.default ~= '') then
        node.value = node.default
        luardb.set(rdb_name, node.value)
    end
    return node.value;
end

-- update node value from related rdb var
local function updateValueFromRdb(node, path_name)
    if node.type ~= 'default' then
        local rdb_name = getRdbKey(node, path_name)
        getRdbValue(node, rdb_name)
    else
        node.value = node.default
    end
    -- dinfo('node type=' .. node.type .. ' value=' .. node.value)
end

local function dump_node( node)
    if debug then
        dinfo('node name=' .. node.name)
        dinfo('node path=' .. node:getPath())
        dinfo('node type=' .. node.type)
        dinfo('node value=' .. node.value)
    end
end

local program_table = {
    download = {
        name = "tr143_httpd",
        program = program_path .. "tr143_diagd.lua",
        argument = "-d",
    },
    upload = {
        name = "tr143_httpu",
        program = program_path .. "tr143_diagd.lua",
        argument = "-u",
    },
    ipping = {
        name = "tr143_ping",
        program = program_path .. "tr143_diagd.lua",
        argument = "-p",
    },
}

-- return pid by reading a pidfile, return nil if pidfile does not exist
local function get_pid(pidfile)
    local f = io.open(pidfile)
    if f then
        local pid = f:read("*number")
        io.close(f)
        return pid
    end
end

local function check_process_exist(pid)
    if pid then
        local status = os.execute("/bin/ls /proc/" .. pid .. ">/dev/null 2>&1")
        if status and status == 0 then
            return true
        end
    end
    return false
end

local function start_program(obj)
    dinfo("start_program (" .. obj.name .. ') begin')

    local ret = CWMP.Error.InternalError

    -- start-stop-daemon -S -b -m -p $PIDFILE -x $EXEC -- -d/-u/-p
    local pidfile = pidfile_path .. obj.name .. '.pid'
    local argument = obj.argument
    local cmd = start_daemon .. ' -p ' .. pidfile .. ' -x ' .. obj.program
                .. ' -- ' .. argument

    local status = os.execute(cmd)
    dinfo("execute " .. cmd .. ", status=" .. status)
    if status and status == 0 then
        os.execute("sleep 1")
        -- wait a while, make sure the process exist
        if  check_process_exist(get_pid(pidfile)) == true then
            dinfo('start_program (' .. obj.name .. ') succeeded')
            ret = 0
        else
            os.execute('/bin/rm ' .. pidfile)
            dinfo('start_program (' .. obj.name .. ') failed')
        end
    end

    dinfo("start_program (" .. obj.name .. ') end')
    return ret
end

function stop_program(obj)
    dinfo('stop_program (' .. obj.name .. ') begin');
    local ret = -1

    -- start-stop-daemon -K -b -q -p $PIDFILE
    local pidfile = pidfile_path .. obj.name .. '.pid'
    local pid = get_pid(pidfile)
    if check_process_exist(pid) == true then
        local cmd = stop_daemon .. ' -p ' .. pidfile
        local status = os.execute(cmd)
        dinfo("execute " .. cmd .. ", status=" .. status)

        -- wait a while, if process still exist, force delete
        os.execute("sleep 1")
        if check_process_exist(pid) == true then
            dinfo("force kill " .. pidfile .. ", pid=" .. pid)
            os.execute("/bin/kill -9 " .. pid)
        end

        os.execute("/bin/rm " .. pidfile)
        ret = 0
    end
    dinfo('stop_program (' .. obj.name .. ') end')
    return ret
end

local setDiagState = {} -- {key: val} table for delayed rdb set
local function setDiagnosticsState()
    for key, val in pairs(setDiagState) do
        dinfo('luardb set in postSession: key=' .. key .. ' value=' .. val)
        luardb.set(key, val)
    end
    setDiagState = {}
end

-- parameter specific validators
local function validate_string(value, len)
    return string.len(value) <= len and 0 or CWMP.Error.InvalidParameterValue
end

local function validate_int(value, lo, hi)
    value = tonumber(value)
    return (value >= lo and value <= hi) and 0 or
        CWMP.Error.InvalidParameterValue
end

local function validate_const(value, const)
    return value == const and 0 or CWMP.Error.InvalidParameterValue
end

local validators = {
    Interface = function(value) return validate_string(value, 256) end,
    Host = function(value) return validate_string(value, 256) end,
    NumberOfRepetitions = function(value) return validate_int(value, 1, 2494967295) end,
    Timeout = function(value) return validate_int(value, 1, 2494967295) end,
    DataBlockSize = function(value) return validate_int(value, 1, 65535) end,
    DSCP = function(value) return validate_int(value, 0, 63) end,
    DownloadURL = function(value) return validate_string(value, 256) end,
    UploadURL = function(value) return validate_string(value, 256) end,
    DownloadTransports = function(value) return validate_const(value, 'HTTP') end,
    UploadTransports = function(value) return validate_const(value, 'HTTP') end,
    EthernetPriority = function(value) return validate_int(value, 0, 7) end,
    TestFileLength = function(value) return validate_int(value, 0, 2494967295) end,
}

--[[
    Validate IP.Diagnostics.*.Interface SPV.
    There are two RDBs for this parameter:
    1) a normal tr069.diagnostics.*.interface; and
    2) an extra tr069.diagnostics.*.interface_iface.
    The former will save the parameter value, e.g. Device.IP.Interface.1;
    the latter will save the linux interface name, e.g. rmnet_data0,
    which can be obtained from Device.IP.Interface.1.Name in the example.
--]]
local function validateInterface(node, path_name, value)
    local rdbKey = getRdbKey(node, path_name)
    local extraRdbKey = rdbKey .. '_iface'

    if value:trim() == '' then
        luardb.set(extraRdbKey, '', 'p') -- clear interface
        return 0
    end

    -- value must be Device.IP.Interface.i according to TR-181
    local ifPathBits = value:explode('.')
    if #ifPathBits ~= 4 or ifPathBits[#ifPathBits-2] ~= 'IP' or ifPathBits[#ifPathBits-1] ~= 'Interface' then
        return CWMP.Error.InvalidParameterValue
    end
    local idx = tonumber(ifPathBits[#ifPathBits])
    if not idx or idx <= 0  then
        return CWMP.Error.InvalidParameterValue
    end

    local ret, iface = pcall(paramTree.getValue, paramTree, value .. '.Name')
    if ret then
        luardb.set(extraRdbKey, iface, 'p')
        return 0
    end
    return CWMP.Error.InvalidParameterValue
end

return {
    ----
    -- Container Objects
    ----
    [subRoot .. '*'] = {
        init = function(node, name)
            dinfo('init ' .. name)
            -- dump_node(node)

            local result = CWMP.Error.InvalidParameterName
            if node.name == 'IPv4PingSupported' then
                stop_program(program_table.ipping)
                result = start_program(program_table.ipping)
            elseif node.name == 'IPv4DownloadDiagnosticsSupported' then
                stop_program(program_table.download)
                result = start_program(program_table.download)
            elseif node.name == 'IPv4UploadDiagnosticsSupported' then
                stop_program(program_table.upload)
                result = start_program(program_table.upload)
            elseif node.name == 'IPv6PingSupported' or
                   node.name == 'IPv6DownloadDiagnosticsSupported' or
                   node.name == 'IPv6UploadDiagnosticsSupported' then
                result = 0
            end

            return result
		end,

        get = function(node, name)
            if node.name == 'IPv4PingSupported' or
               node.name == 'IPv4DownloadDiagnosticsSupported' or
               node.name == 'IPv4UploadDiagnosticsSupported' then
                return 0, '1'
            end
            return 0, '0'
        end,

        set = function(node, name, value)
            return CWMP.Error.ReadOnly
        end,
	},

	----
	-- Instance Parameters
	----
    [subRoot .. '*.*'] = {
        init = function(node, name)
            dinfo('init ' .. name)
            -- dump_node(node)

            if node.name == 'DiagnosticsState' then
                -- watch DiagnosticsState to send diagnostics complete
                local watcher = function(key, value)
                    dinfo('external change of rdb ' .. key)
                    if value then
                        if value ~= node.value then
                            dinfo('rdb ' .. key .. ' changed: "' .. node.value
                                      .. '" -> "' .. value .. '"')
                            if value ~= 'None' and value ~= 'Requested' and
                               value ~= 'Canceled' then
                                client:asyncInform('8 DIAGNOSTICS COMPLETE')
                            end
                            client:asyncParameterChange(node, node:getPath(),
                                                        value)
                        end
                    end
                end
                local rdbKey = getRdbKey(node, name)
                dinfo('adding watcher for rdb: ' .. rdbKey)
                luardb.watch(rdbKey, watcher)

		-- Piggy-back on DiagnosticsState to set up the interface name
		-- in rdb (as it's fixed on Earwig)
		local intf = node.parent:getChild('Interface')
		validateInterface(intf, intf:getPath(), fixedInterface)
            end

            updateValueFromRdb(node, name)
            return 0
        end,

        get = function(node, name)
            -- dinfo('get name=' .. name .. " ntype=" .. node.type)
            updateValueFromRdb(node, name)
            return 0, node.value
        end,

        set = function(node, name, value)
            dinfo('set ' .. name .. '=' .. value)
            if node.name == 'DiagnosticsState' then
                -- only allow writing Requested and Canceled
                if value ~= 'Requested' and value ~= 'Canceled' then
                    return CWMP.Error.InvalidParameterValue
                end
                --[[
                    TR-181 requires the CPE waits until session completion
                    before starting diagnostic test. So we postpone setting
                    DiagnosticsState rdb to postSession.
                --]]
                node.value = value
                setDiagState[getRdbKey(node, name)] = value
                if client:isTaskQueued('postSession', setDiagnosticsState)
                   ~= true then
                    client:addTask('postSession', setDiagnosticsState)
                end
            elseif node.access == 'readonly' then
                return CWMP.Error.ReadOnly
            else
                -- value range check
                local validator = validators[node.name]
                if not validator then
                    -- every allowed parameter should have a validator
                    return CWMP.Error.InvalidParameterName
                end
                local ret = validator(value)
                if ret ~= 0 then
                    return ret
                end
                if node.name == 'Interface' then -- Interface needs extra validation and an additional RDB for the interface name
                    ret = validateInterface(node, name, value)
                    if ret ~= 0 then
                        return ret
                    end
                end
                -- external program watches the writable rdb vars to take action
                node.value = value
                -- all writable parameters should be persisted in rdb
                luardb.set(getRdbKey(node, name), value, 'p')
            end
            return 0
        end,
    },
}

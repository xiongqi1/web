 --
 -- tr069 bluetooth handlers
 --
 -- Copyright Notice:
 -- Copyright (C) 2015 NetComm Pty. Ltd.
 --
 -- This file or portions thereof may not be copied or distributed in any form
 -- (including but not limited to printed or electronic forms and binary or
 -- object forms) without the expressed written consent of NetComm Wireless
 -- Pty. Ltd
 -- Copyright laws and International Treaties protect the contents of this file.
 -- Unauthorized use is prohibited.
 --
 --
 -- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 -- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 -- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 -- NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 -- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 -- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 -- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 -- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 -- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 -- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 -- SUCH DAMAGE.
 --
require("handlers.hdlerUtil")
require('Parameter.Validator')

require("Logger")
Logger.addSubsystem('Bluetooth')

local subROOT = conf.topRoot .. '.X_NETCOMM.Bluetooth.'
local RDB_CONF_BASE = 'bluetooth.conf.'
local RDB_BT_ENABLE_VAR = RDB_CONF_BASE .. "enable"
local RDB_BT_DISC_TIMEOUT_OPTS_VAR =
    RDB_CONF_BASE .. "discoverable_timeout_options"
local MAX_NAME_BUF_LEN = 128

local g_paired_device_list = {}
local g_devices_depth = 5

--
-- Boolean value validations.
--
validate_bool = function(value)
    if ((value == "0") or (value == "1")) then
        return true
    else
        return false
    end
end -- validate_bool

--
-- Bluetooth name validations.
--
validate_name = function(value)
    local len = value:len()
    if ((len > 0) and (len < MAX_NAME_BUF_LEN)) then
        return true
    else
        return false
    end
end -- validate_name

--
-- Discoverable timeout value validations. Checks whether the value
-- is one of the configured options.
--
validate_discoverable_timeout = function(value)
    local options = luardb.get(RDB_BT_DISC_TIMEOUT_OPTS_VAR)

    if not options then
        return false
    end

    local valid_values = options:explode(',')
    for _,val in ipairs(valid_values) do
        if (value == val) then
            return true
        end
    end

    return false
end -- validate_discoverable_timeout

--
-- Maps object parameter names to rdb variable and validate function
--
local g_param_data = {
    ['Enable'] = { rdb_var = 'enable', validate = validate_bool },
    ['DeviceName'] = { rdb_var = 'name', validate = validate_name },
    ['Pairable'] = { rdb_var = 'pairable', validate = validate_bool },
    ['DiscoverableTimeout'] = { rdb_var = 'discoverable_timeout',
                                validate = validate_discoverable_timeout },
}

--
-- Returns the list of currently paired devices.
--
get_paired_device_list = function()
    local device
    local paired_device_list = {}
    local get_devs_result =
        Daemon.readCommandOutput('rdb invoke btmgr.rpc get_devices 2 4096')

    if (get_devs_result:len() == 0) then
        -- No bluetooth devices at present
        return {}
    end

    --
    -- btmgr returns the device list string in this format:
    --    Address=B4:B6:76:54:34:99;Name=CDNTB100;Paired=false&Address=18:F1:45:24:A8:93;Name=foobar;Paired=true
    --
    local single_dev_result = get_devs_result:explode('&')
    for _,dev in ipairs(single_dev_result) do

        -- Extract device properties from the single device string
        device = {}
        device_properties = dev:explode(';')
        for _,prop in ipairs(device_properties) do
            local name, val = prop:match('(.+)=(.+)')

            if (name and val) then
                -- percent decoding
                val = string.gsub(val, ('%%%x%x'),
                                  function(str)
                                      return string.char(tonumber(str:sub(2,3),
                                                         16))
                                  end)
                device[name] = val
           end
       end

       if (device["Paired"] == "true") then
           table.insert(paired_device_list, device)
       end
    end

    return paired_device_list

end -- get_paired_device_list

--
-- Reconstructs the paired devices nodes
--
reconstruct_child_nodes = function(node)
	-- delete all of children data model object
	for _, child in ipairs(node.children) do
		if child.name ~= '0' then
			child.parent:deleteChild(child)
		end
	end

	-- add children from currently paired devices
	for i, v in ipairs(g_paired_device_list) do
		node:createDefaultChild(i)
	end
end -- reconstruct_child_nodes

--
-- Pre session task handler. Updates the paired devices nodes based on
-- the list of currently paired devices
--
task_handler = function (task)
	local new_paired_device_list = get_paired_device_list()

	-- update children
	g_paired_device_list = new_paired_device_list
	reconstruct_child_nodes(task.data)
end -- task_handler

--
-- Gets the rdb variable and validate function for a given node path
--
get_param_data = function(node_path)
    local path_bits = node_path:explode('.')
    local param_name = path_bits[#path_bits]
    local param_data = g_param_data[param_name]
    local rdb_var = nil
    local validate = nil

    if param_data then
        rdb_var = RDB_CONF_BASE .. param_data.rdb_var
        validate = param_data.validate
    end

    return rdb_var, validate
end -- get_param_data

return {
    [subROOT .. 'Enable|DeviceName|Pairable|DiscoverableTimeout'] = {
        get = function(node, name)
            local rdb_var
            local result = nil

            rdb_var = get_param_data(name)
            if rdb_var then
                result = luardb.get(rdb_var)
            end

            if not result then
                return CWMP.Error.InternalError
            end

            return 0, result
        end,

        set = function(node, name, value)
            local rdb_var
            local validate
            local en
            local old_val

            rdb_var, validate = get_param_data(name)

            if not rdb_var then
                return CWMP.Error.InternalError
            end

            if validate then
                if not validate(value) then
                    return CWMP.Error.InvalidParameterValue
                end
            end

            old_val = luardb.get(rdb_var)
            if (old_val == value) then
                return 0
            end

            luardb.set(rdb_var, value)

            -- Enable is the trigger variable so set it if not already
            -- triggered.
            if not (rdb_var == RDB_BT_ENABLE_VAR) then
                en = luardb.get(RDB_BT_ENABLE_VAR)
                luardb.set(RDB_BT_ENABLE_VAR, en)
            end

            return 0
        end,
    },

    [subROOT .. 'PairedDevices'] = {
        init = function(node, name, value)

            -- add children
            g_paired_device_list = get_paired_device_list();
            reconstruct_child_nodes(node)

            -- add persistent callback
            if client:isTaskQueued('preSession', task_handler) ~= true then
                client:addTask('preSession', task_handler, true, node)
            end

            return 0
        end,
    },

    [subROOT .. 'PairedDevices.*'] = {
        init = function(node, name, value)
            local pathBits = name:explode('.')
            g_devices_depth = #pathBits
            return 0
        end,
    },

    [subROOT .. 'PairedDevices.*.*'] = {
        get = function(node, name)
            local pathBits = name:explode('.')
            local dataModelIdx = pathBits[g_devices_depth]
            local paramName = pathBits[g_devices_depth+1]

            return 0, g_paired_device_list[tonumber(dataModelIdx)][paramName]
        end,
    },

} -- return

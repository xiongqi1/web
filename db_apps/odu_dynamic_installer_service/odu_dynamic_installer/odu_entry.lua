-- Copyright (C) 2021 Casa Systems.
-- helpful module to access ODU API

local m_luardb = require("luardb")
local m_client = require("lua_odu_web_client_helper")
local m_json = require("turbo.3rdparty.JSON")
local m_turbo_util = require("turbo.util")

-- override onDecodeError so it should not call assert on failure in parsing JSON
function m_json:onDecodeError(message, text, location, etc) end

-- get ODU info from Hello endpoint
-- @return (success) true,decoded table of ODU info; (failed) false, response body content
local function get_restful_odu_info()
    local res, code_or_error, body = m_client.do_get("api/v1/Hello")
    if res and code_or_error == 200 then
        return true, m_json:decode(body)
    else
        return false, body
    end
end

-- push file to ODU
-- @param file_path source file path to push
-- @param is_firmware_file true: the file is a firmware file
-- @return (success) true; (failed) false, error message
local function push_file_to_odu(file_path, is_firmware_file)
    local end_point = is_firmware_file and "api/v2/update/firmware" or "api/v2/update/config"
    local res, code_or_error, body = m_client.do_put_file(end_point, file_path)
    if res and code_or_error == 200 then
        return true
    else
        local err_msg
        if res then
            if code_or_error == 400 then
                err_msg = "Config file error"
            elseif code_or_error == 403 then
                err_msg = "Not authorised"
            elseif code_or_error == 401 then
                err_msg = "Not authenticated"
            elseif code_or_error == 405 then
                err_msg = "Method not allowed"
            elseif code_or_error == 500 and type(body) == "string" and body ~= "" then
                local err_tbl = m_json:decode(body)
                if type(err_tbl) == "table" and type(err_tbl["error"]) == "table" then
                    err_msg = "Failed in applying "..table.concat(err_tbl["error"], ",")
                else
                    err_msg = "ODU server response: "..body
                end
            else
                err_msg = "ODU server response code: "..tostring(code_or_error)
            end
        else
            err_msg = "Failed to connect ODU server: "..code_or_error
        end
        return false, err_msg
    end
end

-- query ODU update status
-- @param query_firmware true: query firmware update status; false: query config update status
-- @return decoded table
-- e.g. for config: [{'type': 'rdb', 'ids': ['1234', '5678']}, {'type': 'cert', 'ids': [...]},
--                   {'type': 'mbn', 'ids': [...]}, {'type': 'efs', 'ids': [...]}]
-- e.g. for firmware: {'type': 'firmware', 'ids': [inactive_version, active_version]}
local function query_odu_update(query_firmware)
    local end_point = query_firmware and "api/v2/update/firmware" or "api/v2/update/config"
    local res, code_or_error, body = m_client.do_get(end_point)
    if res and code_or_error == 200 then
        return true, m_json:decode(body)
    else
        return false, body
    end
end

-- get ODU connection status
-- @return table { phy_connect = true/false, interface_connect = true/false }
local function get_odu_connection_status()
    local phy_connect = m_luardb.get("owa.connected") == "1"
    local interface_connect = false
    if phy_connect then
        interface_connect = m_luardb.get("owa.interface.connected") == "1"
    end
    return {
        phy_connect = phy_connect,
        interface_connect = interface_connect
    }
end

-- @return table {
--     model = model (which is variant),
--     firmware_version = firmware version (i.e x.y.z),
--     hardware_version = hardware version,
--     config_update_status = {
--         rdb = {'config_id_1', ...},
--         cert = { ... },
--         mbn = { ... },
--         efs = { ... }
--     }
local function get_odu_info()
    local info = get_odu_connection_status()
    if not info.interface_connect then
        return info
    end
    local res, restful_info = get_restful_odu_info()
    if res and restful_info then
        if restful_info["GenericModel"] and restful_info["GenericModel"] ~= "" then
            info["model"] = restful_info["GenericModel"]
        else
            info["model"] = restful_info["ModelName"]
        end
        if type(restful_info["SoftwareVersion"]) == "string" then
            info["firmware_version"] = restful_info["SoftwareVersion"]:match("^[^_]+_(.+)$")
        end
        info["hardware_version"] = restful_info["HardwareVersion"]
    else
        return info
    end
    res, restful_info = query_odu_update(false)
    if res and type(restful_info) == "table" then
        info["config_update_status"] = {}
        for _, type_ids in pairs(restful_info) do
            if type(type_ids) == "table" and type_ids["type"]
                    and type(type_ids["ids"]) == "table" then
                info["config_update_status"][type_ids["type"]] = type_ids["ids"]
            end
        end
    end

    return info
end

return {
    get_odu_connection_status = get_odu_connection_status,
    get_odu_info = get_odu_info,
    push_file_to_odu = push_file_to_odu
}

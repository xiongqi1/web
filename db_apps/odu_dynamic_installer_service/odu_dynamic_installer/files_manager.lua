-- Copyright (C) 2021 Casa Systems.
-- files manager

local m_config = require("odu_dynamic_installer.config")
local m_odu_entry = require("odu_dynamic_installer.odu_entry")
local m_lfs = require("lfs")
require("stringutil")

local m_metadata_config_in = {
    "config_type",
    "config_id",
    "config_level"
}

local m_metadata_config_out = {
    "type",
    "configId",
    "configLevel"
}

local m_metadata_fw_in = {
    "fw.variants",
    "fw.version"
}

local m_metadata_fw_out = {
    "compatVariants",
    "fwVersion"
}

local m_metadata_config_optional = {
    ["compatVariants"] = {
        metadata_name = "compat_variants",
        arg_handler = function(arg)
            return arg or "*"
        end
    },
    ["compatSwVersion"] = {
        metadata_name = "compat_sw_version",
        arg_handler = function(arg)
            return arg or "*"
        end
    },
    ["compatHwVersion"] = {
        metadata_name = "compat_hw_version",
        arg_handler = function(arg)
            return arg or "*"
        end
    },
    ["reboot"] = {
        metadata_name = "reboot",
        arg_handler = function(arg)
            -- default: true
            if not arg or arg == "1" or arg == "Yes" or arg == "yes" or arg == "true" then
                return true
            else
                return false
            end
        end
    }
}

local m_cached_star_files

local function validate_uploaded_file_name(file_name)
    return type(file_name) == "string" and #file_name <= 128 and file_name:match("^[%w_%-%.]+%.star$")
end

local function get_optional_metadata(star_file_path, metadata_name, arg_handler)
    local tmpfile = os.tmpname()
    local cmd = "star "..star_file_path.." -M "..metadata_name.." > "..tmpfile

    local value = nil
    local code = os.execute(cmd)
    if code == 0 then
        local stdout_file = io.open(tmpfile)
        -- read first line only
        value = stdout_file:read("*line")
        stdout_file:close()
    end

    os.remove(tmpfile)

    return arg_handler(value)
end

-- helper function get metadata of config or firmware file
-- @param star_file_name input file name
-- @param metadata_fields list of mandatory metadata fields in file
-- @param metadata_fields_out list of corresponding output fields for mandatory metadata
-- @param optional_metadata_fields optional metadata handlers
-- @return table (metadata field names = values)
local function get_metadata_helper(star_file_name, metadata_fields, metadata_fields_out,
        optional_metadata_fields)
    if not validate_uploaded_file_name(star_file_name) then
        return
    end

    local path = m_config.upload_dir
    local star_file_path = path.."/"..star_file_name

    local field_str
    for _, field in ipairs(metadata_fields) do
        field_str = field_str and field_str.." "..field or field
    end

    local tmpfile = os.tmpname()
    local cmd = "star "..star_file_path.." -M "..field_str.." > "..tmpfile

    local data
    local code = os.execute(cmd)
    if code == 0 then
        data = {}
        local stdout_file = io.open(tmpfile)
        local num = 0
        for line in stdout_file:lines() do
            num = num  + 1
            if num > #metadata_fields_out then
                break
            end
            data[metadata_fields_out[num]] = line
        end
        stdout_file:close()

        if optional_metadata_fields then
            for field, handler in pairs(optional_metadata_fields) do
                data[field] = get_optional_metadata(star_file_path,
                    handler.metadata_name, handler.arg_handler)
            end
        end
    end

    os.remove(tmpfile)

    return data
end

-- @return table (metadata field names = values)
local function get_metadata(star_file_name)
    local data = get_metadata_helper(star_file_name,
        m_metadata_config_in, m_metadata_config_out, m_metadata_config_optional)
    if not data then
        -- try firmware
        data = get_metadata_helper(star_file_name, m_metadata_fw_in, m_metadata_fw_out)
        if data then
            data["type"] = "firmware"
            data["reboot"] = true
        end
    end

    return data
end

-- @return 0: equal; 1: version_a > version_b; 2: version_a < version_b; 3: not equal but unable to compare
local function compare_versions(version_a, version_b)
    if version_a == version_b then
        return 0
    end
    if type(version_a) ~= "string" or type(version_b) ~= "string" then
        return 3
    end
    version_a = version_a:trim()
    version_b = version_b:trim()
    local a = version_a:explode(".")
    local b = version_b:explode(".")
    local comp_n = math.min(#a, #b)
    for i = 1, comp_n do
        if a[i] ~= b[i] then
            local num_a = tonumber(a[i])
            local num_b = tonumber(b[i])
            if not num_a or not num_b then
                return 3
            elseif num_a ~= num_b then
                return num_a > num_b and 1 or 2
            end
        end
    end

    if #a == #b then
        return 0
    else
        return #a > #b and 1 or 2
    end
end

-- Test version against compat version
-- Format of version: {prefix}{version string}
-- Format of compat version: {prefix}{comparison operator}{version string}
-- where {prefix} must end with "-" (there can be multiple "-" in prefix)
-- ({prefix} and {comparison operator} are optional)
-- e.g
--     version: XXXX-YYYY-1.2.3
--     compat version: XXXX-YYYY->=1.2.3
-- @param version version to test
-- @param compat_version can contain wildcard "*" or comparison operator in prefix
-- @param test_prefix true: test prefix (prefix of version and compat_version must match);
--                    false: not test prefix
-- @return true: matches or satisfies comparison: false: otherwise
local function test_compat_version(version, compat_version, test_prefix)
    if type(version) ~= "string" or type(compat_version) ~= "string" then
        return false
    end
    version = version:trim()
    compat_version = compat_version:trim()
    if compat_version == "*" then
        return true
    end
    if test_prefix then
        local vp_s, vp_e = version:find("^.+%-")
        local cvp_s, cvp_e = compat_version:find("^.+%-")
        if vp_s and vp_e then
            if cvp_s and cvp_e then
                if version:sub(vp_s, vp_e) == compat_version:sub(cvp_s, cvp_e) then
                    version = version:sub(vp_e + 1):trim()
                    compat_version = compat_version:sub(cvp_e + 1):trim()
                else
                    return false
                end
            else
                return false
            end
        end
    end

    local op = "="
    local op_s, op_e = compat_version:find("^[<=>]+")
    if op_s and op_e then
        op = compat_version:sub(op_s, op_e)
        compat_version = compat_version:sub(op_e + 1):trim()
    end

    local comp = compare_versions(version, compat_version)
    if op == "=" or op == "==" then
        return comp == 0
    elseif op == ">" or op == ">=" then
        return comp == 0 or comp == 1
    elseif op == "<" or op == "<=" then
        return comp == 0 or comp == 2
    else
        return false
    end
end

-- @param variant variant to test
-- @param compat_variants string like "variant1,variant2"
-- @return true: matches; false: otherwise
local function test_compat_variants(variant, compat_variants)
    if type(variant) ~= "string" or type(compat_variants) ~= "string" then
        return false
    end
    if compat_variants == "*" then
        return true
    end
    compat_variants = compat_variants:explode(",")
    for _, v in pairs(compat_variants) do
        if variant == v then
            return true
        end
    end
    return false
end

local function get_file_path(file_name)
    return m_config.upload_dir.."/"..file_name
end

-- @param file_name file name to read data (file must be valid and existing)
-- @return data to be set to file entry of m_cached_star_files
local function get_file_entry_data(file_name)
    local data
    local attr = lfs.attributes(get_file_path(file_name))
    if attr and attr.mode == "file" then
        local metadata = get_metadata(file_name)
        if metadata then
            data = {
                file_name = file_name,
                metadata = metadata
            }
        end
    end

    return data
end

-- get list of uploaded star files
-- @param read_directory (boolean) false: return cache if possible; true: always read storage
-- @param new_file_name update specific new file
local function get_star_files(read_directory, new_file_name)
    if not read_directory and not new_file_name and m_cached_star_files then
        return m_cached_star_files
    end

    if read_directory or not m_cached_star_files then
        m_cached_star_files = {}

        local path = m_config.upload_dir

        for file in lfs.dir(path) do
            if file ~= "." and file ~= ".." then
                m_cached_star_files[file] = get_file_entry_data(file)
            end
        end
    else
        if new_file_name then
            m_cached_star_files[new_file_name] = get_file_entry_data(new_file_name)
        end
    end

    return m_cached_star_files
end

-- helper function to update odu info into star files list
local function update_star_files_data_with_odu(odu_info)
    for _, file_data in pairs(m_cached_star_files) do
        if not odu_info.interface_connect or not odu_info.phy_connect then
            file_data.compat_with_odu = nil
            file_data.already_updated_odu = nil
        else
            file_data.compat_with_odu = false
            file_data.already_updated_odu = false
            if file_data.metadata.type == "firmware" then
                file_data.compat_with_odu = test_compat_variants(odu_info.model,
                    file_data.metadata.compatVariants)
                if file_data.compat_with_odu then
                    if odu_info.firmware_version == file_data.metadata.fwVersion then
                        file_data.already_updated_odu = true
                    end
                end
            else
                if test_compat_variants(odu_info.model, file_data.metadata.compatVariants)
                    and test_compat_version(odu_info.firmware_version,
                        file_data.metadata.compatSwVersion)
                    and test_compat_version(odu_info.hardware_version,
                        file_data.metadata.compatHwVersion, true) then
                    file_data.compat_with_odu = true
                end

                if file_data.compat_with_odu then
                    local conf_types = file_data.metadata.type:explode(",")
                    local num_conf_types = 0
                    local num_already_updated = 0
                    for _, conf_t in pairs(conf_types) do
                        num_conf_types = num_conf_types + 1
                        if odu_info.config_update_status
                                and type(odu_info.config_update_status[conf_t]) == "table" then
                            for _, conf_id in pairs(odu_info.config_update_status[conf_t]) do
                                if conf_id == file_data.metadata.configId then
                                    num_already_updated = num_already_updated + 1
                                    break
                                end
                            end
                        end
                    end
                    if num_conf_types > 0 and num_conf_types == num_already_updated then
                        file_data.already_updated_odu = true
                    end
                end
            end
        end
    end
end

-- get list of uploaded star files and process ODU data
-- @param read_directory (boolean) false: return cache if possible; true: always read storage
-- @param new_file_name update specific new file
local function get_star_files_with_odu(read_directory, new_file_name)
    get_star_files(read_directory, new_file_name)

    local odu_info = m_odu_entry.get_odu_info()
    update_star_files_data_with_odu(odu_info)

    return m_cached_star_files
end

local function delete_file(file_name)
    if not validate_uploaded_file_name(file_name) then
        return false
    end

    local path = m_config.upload_dir.."/"..file_name
    local status = os.remove(path)

    if status and m_cached_star_files then
        m_cached_star_files[file_name] = nil
    end

    return status
end

local m_monitored_file_name
local m_monitored_file_progress_status
local m_monitored_file_error_msg

-- This function should be called in handling monitoring events after pushing file to ODU.
-- So monitored file (and file list cache) must be existing.
local function on_odu_reboot_event(odu_reboot_status_t, odu_info)
    if odu_reboot_status_t == "rebooting" then
        m_monitored_file_progress_status = "ODU-REBOOTING"
    elseif odu_reboot_status_t == "rebooted" then
        if odu_info then
            update_star_files_data_with_odu(odu_info)
            if m_cached_star_files[m_monitored_file_name]
                    and m_cached_star_files[m_monitored_file_name].already_updated_odu then
                m_monitored_file_progress_status = "SUCCESS"
            else
                m_monitored_file_progress_status = "FAILED"
                m_monitored_file_error_msg = "ODU processed and rebooted, but update is not confirmed"
            end
        else
            -- should not happen; just in case
            m_monitored_file_progress_status = "FAILED"
            m_monitored_file_error_msg = "ODU processed and rebooted, but unable to confirm"
        end
    elseif odu_reboot_status_t == "disconnected" then
        m_monitored_file_progress_status = "FAILED"
        m_monitored_file_error_msg = "ODU disconnected"
    elseif odu_reboot_status_t == "timeout" then
        m_monitored_file_progress_status = "FAILED"
        m_monitored_file_error_msg = "Timeout waiting for ODU reboot"
    end
end

local function setup_monitor_odu_processed_file(file_name)
    m_monitored_file_name = file_name
    m_monitored_file_progress_status = "ODU-PROCESSED"
    m_monitored_file_error_msg = nil
end

local function mark_pushing_file_to_odu(file_name)
    m_monitored_file_name = file_name
    m_monitored_file_progress_status = "PUSHING-TO-ODU"
    m_monitored_file_error_msg = nil
end

local function mark_odu_processed_file_success(file_name)
    m_monitored_file_name = file_name
    m_monitored_file_progress_status = "SUCCESS"
    m_monitored_file_error_msg = nil
end

local function mark_odu_processed_file_failed(file_name, err_msg)
    m_monitored_file_name = file_name
    m_monitored_file_progress_status = "FAILED"
    m_monitored_file_error_msg = err_msg
end

-- assume that given file must be existing (i.e relevant check is already done by user code)
-- @return file progress status, error message (if any)
local function get_file_progress_status(file_name)
    if file_name == m_monitored_file_name then
        return m_monitored_file_progress_status, m_monitored_file_error_msg
    else
        return "UPLOADED"
    end
end

-- check if file is in progress of updating to ODU
-- @param file_name file name to check
-- @param reset status true: if the file was busy, but not now, reset the state
-- @return true/false
local function is_file_busy(file_name, reset_status)
    if m_monitored_file_name == file_name then
        if m_monitored_file_progress_status == "SUCCESS" or m_monitored_file_progress_status == "FAILED" then
            if reset_status then
                m_monitored_file_name = nil
                m_monitored_file_progress_status = nil
                m_monitored_file_error_msg = nil
            end
            return false
        else
            return true
        end
    end

    return false
end

return {
    validate_uploaded_file_name = validate_uploaded_file_name,
    get_metadata = get_metadata,
    get_star_files_with_odu = get_star_files_with_odu,
    delete_file = delete_file,
    get_file_path = get_file_path,
    mark_pushing_file_to_odu = mark_pushing_file_to_odu,
    setup_monitor_odu_processed_file = setup_monitor_odu_processed_file,
    on_odu_reboot_event = on_odu_reboot_event,
    mark_odu_processed_file_success = mark_odu_processed_file_success,
    mark_odu_processed_file_failed = mark_odu_processed_file_failed,
    get_file_progress_status = get_file_progress_status,
    is_file_busy = is_file_busy
}

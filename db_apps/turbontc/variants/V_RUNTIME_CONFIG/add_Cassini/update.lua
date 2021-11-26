--[[
    Late config interface

    Copyright (C) 2021 Casa Systems.
--]]

require 'stringutil'
local bit = require 'bit'
local common = require 'common'
local httpserver = require 'turbo.httpserver'
local httputil = require 'turbo.httputil'

local module = {}

local upload_dir = '/usrdata/cache'

local basepath = '/api/v2/update'

-- return full path for file_name, error_code, error_msg
local function upload_path_callback(file_name, uri, method)
    logDebug(string.format('[upload_path_callback] file_name=%s, uri=%s, method=%s', file_name, uri, method))
    local path = uri:match('^([^?]+)') -- get rid of query params
    logDebug(string.format('[upload_path_callback] path=%s', path))
    if method ~= 'PUT' then
        return false, 405, 'Method not allowed'
    end
    if path == basepath .. '/config' then
        return upload_dir .. '/config.star'
    end
    if path == basepath .. '/firmware' then
        return upload_dir .. '/upgrade.star'
    end
    return false, 404, 'Not found'
end

-- save a upload file
-- @param self A web.RequestHandler instance
local function save_small_file(self)
    local file = self:get_argument('file', '')
    if not file or #file == 0 then
        -- no file to be saved (likely has been done as a large file)
        return
    end

    local file_path, code, msg = upload_path_callback(nil, self.request.uri, self.request.method)
    if not file_path then
        self:send_error(code, msg)
    end
    logDebug(string.format('[save_small_file] file_path=%s, file-len=%d', file_path, #file))
    local file_obj = io.open(file_path, 'wb')
    if not file_obj then
        self:send_error(500, 'Failed to store file: ' .. file_path)
    end
    local status = file_obj:write(file)
    file_obj:close()
    if not status then
        os.remove(file_path)
        self:send_error(500, 'Failed to write file: ' .. file_path)
    end
end

-- apply an uploaded config file
--
-- @param self A web.RequestHandler instance
local function apply_config(self)
    -- when we reach here, authorisation has passed, large file has been saved in upload_dir .. '/config.star'
    -- for small file, save it to the same dir as large file
    save_small_file(self)
    local cmd = 'apply_late_configs.sh ' .. upload_dir .. '/config.star'
    logInfo(string.format('[apply_config] execute cmd=%s', cmd))
    local rcode = os.execute(cmd)
    rcode = rcode / 256
    logInfo(string.format('[apply_config] cmd returns %d', rcode))
    if rcode == 0 then
        self:set_status(200)
        return
    end

    -- parse the return code and construct the json response
    local resp = {}
    if bit.band(rcode, 1) ~= 0 then
        resp[#resp + 1] = 'rdb'
    end
    if bit.band(rcode, 2) ~= 0 then
        resp[#resp + 1] = 'cert'
    end
    if bit.band(rcode, 4) ~= 0 then
        resp[#resp + 1] = 'mbn'
    end
    if bit.band(rcode, 8) ~= 0 then
        resp[#resp + 1] = 'efs'
    end
    self:set_status(500)
    self:write({error = resp})
end

-- apply an uploaded firmware image
--
-- @param self A web.RequestHandler instance
local function apply_firmware(self)
    save_small_file(self)
    -- do not reboot yet so we can send response etc
    local cmd = 'flashtool ' .. upload_dir .. '/upgrade.star'
    logInfo(string.format('[apply_firmware] execute cmd=%s', cmd))
    local rcode = os.execute(cmd) / 256
    logInfo(string.format('[apply_firmware] cmd returns %d', rcode))
    if rcode == 0 then
        self:set_status(200)
        luardb.set('service.system.reset.delay', 30)
        luardb.set('service.system.reset_reason', 'Firmware Upgrade')
        luardb.set('service.system.reset', 1)
        return
    end

    self:set_status(500)
    self:write({error = {'firmware'}})
end

-- execute a command and return its stdout output, first line only
--
-- @param cmd The command to be executed
-- @return A string containing the first line of command output, trimmed.
-- If anything fails, return an empty string.
local function execute_cmd(cmd)
    logInfo(string.format('execute_cmd: cmd=%s', cmd))
    local p = io.popen(cmd)
    local r = ''
    if p then
        r = p:read('*line') or ''
        logInfo(string.format('execute_cmd: read=%s', r))
        p:close()
        r = r:trim()
    end
    return r
end

-- split a comma separated string into an array
--
-- @param str The string to be splitted
-- @return An array of strings; an empty array if input is not a string or an empty string
local function split_filter(str)
    if type(str) ~= 'string' or #str == 0 then
        return {}
    end
    return str:explode(',')
end

-- get the config status as an array of IDs
--
-- @param pURI The container for the instance value representing the config type
-- @return An array of IDs in chronological order
local function get_config_status(pURI)
    local config_type = pURI.instance
    local cmd = string.format('environment -r CONFIG_%s', config_type:upper())
    local ids = execute_cmd(cmd)
    return split_filter(ids)
end

-- get the firmware versions
--
-- @return An array of [inactive version, active version]
local function get_fw_version()
    -- running SW version
    local ver_active = luardb.get('sw.version.digits') or ''
    -- inactive partition SW version
    local ver_inactive = execute_cmd('abctl --get_fw_version inactive')
    return {ver_inactive, ver_active}
end

-- This is invoked before any (large) files are written to device.
-- We have to do authentication/authorisation here.
-- It is too late to do that in request handler.
local function headers_callback(headers, request)
    logDebug(string.format('[headers_callback] method=%s, uri=%s', request.method, request.uri))
    -- only authenticate for PUT
    if request.method ~= 'PUT' then
        return true
    end
    local path = headers:get_url_field(httputil.UF.PATH)
    logDebug(string.format('[headers_callback] path=%s', path))
    local succ, msg
    if path:endsWith('/config') then
        succ = common.da_authorize_request(request, 'config')
        msg = 'Not authorised to apply late configs'
    elseif path:endsWith('/firmware') then
        succ = common.da_authorize_request(request, 'upgrade')
        msg = 'Not authorised to upgrade firmware'
    else
        return false, httpserver.HTTPRequestError:new(404, 'Not Found')
    end
    if succ then
        logInfo('headers_callback: return true')
        return true
    end
    return false, httpserver.HTTPRequestError:new(403, msg)
end

function module.init(maps, _, util, authorizer, headers_callbacks, upload_path_callbacks)
    logDebug('init late config (update) api')
    headers_callbacks[basepath] = headers_callback
    upload_path_callbacks[basepath] = upload_path_callback
    maps[basepath .. '/rdb'] = {
        get = {
            code = '200',
        },
        model = {
            type = util.map_fix('rdb'),
            ids = util.map_run('r', 'environment -r CONFIG_RDB', false, split_filter),
        },
    }
    maps[basepath .. '/cert'] = {
        get = {
            code = '200',
        },
        model = {
            type = util.map_fix('cert'),
            ids = util.map_run('r', 'environment -r CONFIG_CERT', false, split_filter),
        },
    }
    maps[basepath .. '/mbn'] = {
        get = {
            code = '200',
        },
        model = {
            type = util.map_fix('mbn'),
            ids = util.map_run('r', 'environment -r CONFIG_MBN', false, split_filter),
        },
    }
    maps[basepath .. '/efs'] = {
        get = {
            code = '200',
        },
        model = {
            type = util.map_fix('efs'),
            ids = util.map_run('r', 'environment -r CONFIG_EFS', false, split_filter),
        },
    }
    maps[basepath .. '/config'] = {
        get = {
            code = '200',
        },
        model = {
            INSTANCES = {'rdb', 'cert', 'mbn', 'efs'},
            ITEMS = {
                type = util.map_instance('&I'),
                ids = util.map_instance(get_config_status),
            }
        },
        put = {
            code = '200',
            trigger = function() return apply_config end,
        },
    }
    maps[basepath .. '/firmware'] = {
        get = {
            code = '200',
        },
        model = {
            type = util.map_fix('firmware'),
            ids = util.map_fn('r', get_fw_version)
        },
        put = {
            code = '200',
            trigger = function() return apply_firmware end,
        },
    }
end

return module

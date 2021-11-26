#!/usr/bin/env lua

--[[
    NTC webif server based on turbo.lua
    This is based on V_WEBIF_VERSION v2b

    Copyright (C) 2020 Casa Systems Inc.
--]]

TURBO_SSL = true  -- Secure Cookie and Hash need this flag
baseDir = "/www/"
local cgiDir = baseDir .. "cgi-bin/"
local g_turbo_dir = "/usr/share/lua/5.1/webif"
local turbo = require("turbo")
package.path = package.path .. ";" .. cgiDir .. "?.lua;" .. g_turbo_dir .. "/?.lua"
require('srvrUtils')
require('luardb')
require('lfs')
require("support")
require("stringutil")
require("math")
local csrf_session = require("csrf_session")

-- Loads configuration before starting any applications
config = load_overriding_module(g_turbo_dir, 'config')

local function read_file(path)
    local file = io.open(path, "r") -- r read mode and b binary mode
    if not file then return nil end
    local content = file:read "*a" -- *a or *all reads the whole file
    file:close()
    return content
end

-- substitute RDB tags in a string with their respective RDB values
local function substRdb(s)
    local words = {}
    for word in string.gmatch(s, "<<<[%w%.]+>>>") do
        words[word] = true
    end

    for k, _ in pairs(words) do
        --print(k)
        s = string.gsub(s, k, luardb.get(k:sub(4,-4)) or '')
        --print(s)
    end
    return s
end

-- create upload folders if not exists
local upload_path_allowed = {"/tmp/upload/", "/mnt/emmc/upload/"}
if config.UPLOAD_DIR then
    upload_path_allowed = { config.UPLOAD_DIR }
end
for _, path in pairs(upload_path_allowed) do
    if not lfs.attributes(path) then
        lfs.mkdir(path)
    end
end

-- validate whether the given path is allowed for uploading files
function validate_upload_file_path(path)
    if type(path) ~= "string" then
        return false
    end

    -- relative path is disallowed
    if nil ~= path:find("%.%.") then
        turbo.log.error("relative path is not allowed")
        return false
    end

    for _, allowed in pairs(upload_path_allowed) do
        if allowed == path:sub(1, allowed:len()) then
            return true
        end
    end

    return false
end

-- login credentials are stored in RDB:
-- admin.user.<username>.encrypted = <password hash>
-- or (for backward compatibility)
-- admin.user.<username> = <clear text password>
local passwordRdbPrefix = "admin.user."

local formUserName = "username"
local formUserPass = "password"

local defaultPageAfterLogin = config.DEFAULT_PAGE

local loginAccounts

-- get stored password (hash) for a user
-- multiple login names are supported that is stored in RDB variable
local function getStoredPassword(username)
    for _, v in ipairs(loginAccounts) do
        if username == v then
            local passwd = luardb.get(passwordRdbPrefix .. username .. '.encrypted')
            if passwd and #passwd > 0 then
                return true, passwd
            end
            -- for backward compatibility, we still accept clear text passwd
            return false, luardb.get(passwordRdbPrefix .. username)
        end
    end
    return nil
end

-- validate if current session is logged in
-- TODO: add support of authorisation
function validateLogin(appHandler)
    local session = appHandler.session
    if not csrf_session.isSessionValid(session) then
        return false
    end
    if session.user then
        turbo.log.debug("Login validated")
        return true
    end
    turbo.log.debug("Login validation failed")
    return false
end

-- verify a cleartext password against an encrypted one
local function verifyPassword(password, encryptedPassword)
    -- encryptedPassword format: $<algorithm>$<salt>$<hash>
    if not encryptedPassword then
        turbo.log.error('encryptedPassword does not exist')
        return false
    end
    local bits = encryptedPassword:explode('$')
    local alg = bits[2]
    if alg ~= '1' and alg ~= '5' and alg ~= '6' then
        -- only support MD5, SHA256 and SHA512
        turbo.log.error(string.format('unsupported passwd algorithm: %s', alg))
        return false
    end
    local salt = bits[3]
    if not salt or #salt == 0 then
        turbo.log.error(string.format('invalid passwd salt: %s', salt))
        return false
    end
    local b64pw = turbo.escape.base64_encode(password, #password, true)
    local cmd = string.format("echo -n '%s' | base64 -d | openssl passwd -stdin -%s -salt '%s'",
                              b64pw, alg, salt)
    local handle = io.popen(cmd, 'r')
    if not handle then
        turbo.log.error('failed to run openssl passwd')
        return false
    end
    local calculated = handle:read('*line')
    handle:close()
    if encryptedPassword == calculated then
        return true
    end
    turbo.log.error('password mismatch')
    return false
end

-- validate login credential in userInfo against RDB
local function validateUser(userInfo)
    local userName = userInfo[formUserName]
    local userPasswd = turbo.escape.base64_decode(userInfo[formUserPass])
    local encrypted, storedPasswd = getStoredPassword(userName)
    turbo.log.debug("validating user: " .. userName)
    if userName and userPasswd then
        if encrypted then
            -- verify encrypted password
            return verifyPassword(userPasswd, storedPasswd)
        else
            -- clear text password
            return userPasswd == storedPasswd
        end
    end
    return false
end

-- Create login handler to handle all login logout related stuff from POST request only
local LoginHandler = class("LoginHandler", turbo.web.RequestHandler)

function LoginHandler:post(url)
    turbo.log.debug("Entering Login service: URL -- "..url)
    local response = {}
    response.text=""

    if (url ~= "login") and (url ~= "logout") then
        error(turbo.web.HTTPError(404, "Not found"))
    end

    csrf_session.prepareSession(self)

    -- handle logout post request
    if url == "logout" then
        -- clear session
        self.session = {}
        -- logout post can include a redirect param
        local redirect = self:get_argument('redirect', '')
        response.result = 0
        response.text = "See you soon"
        self:write(response)
        csrf_session.updateSession(self)
        if #redirect > 0 then
            self:redirect(redirect)
        end
        return
    end

    -- handle login request
    local loginMsg = self:get_json(true)

    local session = self.session
    local csrfToken = session.csrf
    if not csrf_session.verifyCsrfToken(loginMsg["csrfToken"], csrfToken) then
        response.result = 1
        response.text = "Invalid csrfToken"
        turbo.log.error("Invalid csrfToken")
        self:write(response)
        csrf_session.updateSession(self)
        return
    end

    local userIsValid = validateUser(loginMsg)
    if userIsValid then
        local userName = loginMsg[formUserName]
        -- save username in session, this serves as the flag for logged in
        self.session.user = userName
        -- once logged in, old token needs to be obsoleted, ensuring each session has a unique token
        csrf_session.updateCsrfToken(self)
        response.result=0
        response.text="Welcome , enjoy"
        local needRedirect = self:get_cookie("redirect_url", "None")

        if needRedirect ~= "None" then
            self:clear_cookie("redirect_url")
            response.url = needRedirect
        else
            response.url = defaultPageAfterLogin
        end
        self:write(response)
    else
        response.result = 1
        response.text = "User name or password doesn't match"
        self:write(response)
    end
    csrf_session.updateSession(self)
end

-- Create network status handler to check current network status
-- No authentification is requested nor csrf token check as well.
local NetworkStatusHandler = class("NetworkStatusHandler", turbo.web.RequestHandler)

function NetworkStatusHandler:get(url)
    turbo.log.debug("Entering Network Status handler : URL -- "..url)
    csrf_session.prepareSession(self)
    local response = {}
    response.result = 0
    response.data = {
      registered = luardb.get("wwan.0.system_network_status.registered"),
      mcc = luardb.get("wwan.0.system_network_status.MCC"),
      mnc = luardb.get("wwan.0.system_network_status.MNC"),
      rat = luardb.get("wwan.0.system_network_status.service_type")
    }
    self:write(response)
end

-- Create web server status handler to check the web server status
-- The purpose is a command to check and wait until webserver launches after reboot.
-- So no authentification is requested nor csrf token check as well.
local StatusHandler = class("StatusHandler", turbo.web.RequestHandler)

function StatusHandler:get(url)
    turbo.log.debug("Entering Web Server Status handler : URL -- "..url)
    csrf_session.prepareSession(self)
    local response = {}
    response.result = 0
    response.data = {
      status = "running",
      loginStatus = validateLogin(self) and 'login'
    }

    -- Send status response as JSONP in order to avoid
    -- CORS(Cross Origin Resource Sharing) issue which happens
    -- when current WEBUI domain address is different from the default
    -- address and browser is trying to get status response on
    -- the default address during after factory reset.
    -- Ex) Set LAN IP to 192.168.4.1 while default LAN IP is 192.168.1.1
    --     then start factory reset.
    --
    -- statusJsonpParser is the callback function name which should be
    -- defined in client side. Real JSON object is embraced with the callback
    -- function name.
    self:write("statusJsonpParser(")
    self:write(response)
    self:write(");")
end

-- Clear log message files
local ClearLogHandler = class("ClearLogHandler", turbo.web.RequestHandler)

function ClearLogHandler:post(url)
    local response = {}

    csrf_session.prepareSession(self)
    -- If not logged in then do not allow log clear action
    if not validateLogin(self) then
        turbo.log.error("not logged in, reject log clear cgi call")
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end

    csrfToken = self:get_argument('csrfToken', '')
    if not csrf_session.verifyCsrfToken(csrfToken, self.session.csrf) then
        error(turbo.web.HTTPError(403, "CSRF token error"))
    end

    if config.LOGCLEAR_CMD then
      os.execute(config.LOGCLEAR_CMD)
      response.result = 0
    else
      response.text = 'Log clear command is not defined'
      response.result = 1
    end
    self:write(response)
    csrf_session.updateSession(self)
end

-- Create RDB command handler to send a RDB command and check the command result
local RdbCmdHandler = class("RdbCmdHandler", turbo.web.RequestHandler)
function RdbCmdHandler:post(url)
    turbo.log.debug("RdbCmdHandler:post("..url..")")
    local response = {}
    csrf_session.prepareSession(self)
    -- If not logged in then do not allow RDB command execution
    if not validateLogin(self) then
        turbo.log.error("not logged in, reject RDB command cgi call")
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end
    csrfToken = self:get_argument('csrfToken', '')
    if not csrf_session.verifyCsrfToken(csrfToken, self.session.csrf) then
        error(turbo.web.HTTPError(403, "CSRF token error"))
    end
    local rdbPrefix = self:get_argument('rdbPrefix', '')
    if rdbPrefix == '' then
        error(turbo.web.HTTPError(400, "Invalid RDB prefix"))
    end
    local rdbCmd = self:get_argument('cmd', '')
    local allowedCmds = {get=true, set=true, set_rat=true, verifypin=true, check=true,
                        enable=true, disable=true, verifypuk=true, changepin=true}
    if not allowedCmds[rdbCmd] then
        error(turbo.web.HTTPError(400, "Invalid RDB command"))
    end
    -- set command parameters for set/get command
    if rdbCmd ~= 'check' then
        local rdbParams = self:get_argument('param', 'ERROR')
        local rdbParamsObj = turbo.escape.json_decode(rdbParams)
        if rdbParamsObj then
            for i, v in pairs(rdbParamsObj) do
                local rdbParamVar = rdbPrefix..".cmd.param."..i
                luardb.set(rdbParamVar, v)
            end
        end
    end
    luardb.set(rdbPrefix..'.cmd.status', '')
    luardb.set(rdbPrefix..'.cmd.command', rdbCmd)
    response.result = 0
    self:write(response)
    csrf_session.updateSession(self)
end
function RdbCmdHandler:get(url)
    turbo.log.debug("RdbCmdHandler:get("..url..")")
    csrf_session.prepareSession(self)
    local rdbPrefix = self:get_argument('rdbPrefix', '')
    if rdbPrefix == '' then
        error(turbo.web.HTTPError(400, "Invalid RDB prefix"))
    end
    -- get command parameters for get command
    local param = {}
    local rdbCmd = luardb.get(rdbPrefix..".cmd.command") or ''
    if rdbCmd == 'get' then
        local rdbParams = self:get_argument('param', 'ERROR')
        local rdbParamsObj = turbo.escape.json_decode(rdbParams)
        if rdbParamsObj then
            for i, v in pairs(rdbParamsObj) do
                local rdbParamVar = rdbPrefix..".cmd.param."..i
                param[i] = luardb.get(rdbParamVar)
            end
        end
    end
    local response = {}
    response.result = 0
    response.data = {
      status = luardb.get(rdbPrefix .. '.cmd.status') or '',
      param = param
    }
    self:write(response)
end
-- Create a new requesthandler with a method get() for HTTP GET.
local MustacheHandler = class("ExampleHandler", turbo.web.RequestHandler)

--[[If client has no valid cookie to indicate it has logged in, redirect to index.html,
    once client login sucessfully, server will set the early url to login page to ask redirect
    to right page
--]]
function MustacheHandler:get(url)
    turbo.log.debug("matched url: "..url)
    csrf_session.prepareSession(self)
    local isLogin = validateLogin(self)

    local fileContent = read_file(baseDir..url)
    if nil == fileContent then
        turbo.log.debug("fileContent is nil, redirect to index.html")
        self:redirect("/index.html", true)
        return
    end
    local authCheck = '{{#loggedIn}}'
    -- if a page needs authentication, it starts with authCheck tag
    local authenticatedOnly = (fileContent:sub(1, #authCheck) == authCheck)

    -- allow prelogin access to page with authenticatedOnly=false
    if (not isLogin) and (url ~= "index.html") and authenticatedOnly then
        turbo.log.debug("(not isLogin) and (url ~= index.html), redirect to index.html")
        self:set_cookie("redirect_url", url, "/", -1)  --save url to client cookie
        self:redirect("/index.html")
        return
    end

    if url == "index.html" then
        -- disable cache for login page
        self:add_header("Cache-Control", "no-store")
    end

    if isLogin and (url == "index.html") then  -- for example , access . or / directory after logged in
        turbo.log.debug("isLogin and (url == index.html), redirect to "..defaultPageAfterLogin)
        csrf_session.updateSession(self)
        self:redirect(defaultPageAfterLogin, true)
        return
    end

    -- generate a new CSRF token for each page request
    local csrfToken = csrf_session.getPageCsrfToken(self)
    local loggedInUser = self.session.user or ''
    local loggedInUserGroup = luardb.get("admin.user."..loggedInUser..".groups") or 'guest'
    self:write(turbo.web.Mustache.render(substRdb(fileContent),
        {
            loggedInUser=loggedInUser,
            loggedInUserGroup=loggedInUserGroup,
            loggedIn=isLogin,
            lang="en",
            csrfToken=csrfToken,
        })
    )
    csrf_session.updateSession(self)
end

-- This function validates that user belongs to a group with read/write access to object
-- objHandler - handler of the object
-- groups - list of groups the current user belongs to
-- action - "get" or "set"
local function validateGroupAccess(objHandler, groups, action)
    local allowedGroups = action == "get" and objHandler.readGroups or objHandler.writeGroups
    for _, group in ipairs(groups) do
        if allowedGroups[group] then
            return true
        end
    end
    return false
end

-- This function returns userGroups of the current logged-in user as a list
local function getUserGroups(appHandler)
    local session = appHandler.session
    local loggedInUser = session.user or ''
    local loggedInUserGroup = luardb.get("admin.user."..loggedInUser..".groups") or 'guest'
    return loggedInUserGroup:explode(",")
end

local function getObjectHandler(appHandler, action, objName, queryParams, response)
    local fileName = cgiDir..'obj'..objName..'.lua'
    if not response.text then response.text = '' end
    if file_exists(fileName) then
        dofile(fileName)
        if objHandler then
            local authenticated = validateLogin(appHandler)
            local groups = getUserGroups(appHandler)
            if objHandler.authenticatedOnly and authenticated == false then
                response.access = 'NeedsAuthentication'
                response.text = response.text .. 'Object ' .. objName .. ' denied '
            elseif objHandler.authenticatedOnly and not validateGroupAccess(objHandler, groups, action) then
                response.access = 'NeedsAuthorisation'
                response.text = response.text .. 'Object ' .. objName .. ' needs authorisation '
            else
                -- save queryParams in objHandler so that objHandler.get/set can use it later
                objHandler.queryParams = queryParams
                return objHandler
            end
        else
            response.text = response.text .. 'Unknown Object' .. objName .. ' '
            response.result = 1
        end
    else
        response.text = response.text .. 'Missing ' .. objName .. ' handler '
        response.result = 1
    end
    return nil
end

local function getObject(appHandler, objName, queryParams, response)
    local objHandler = getObjectHandler(appHandler, "get", objName, queryParams, response)
    if objHandler then
        response[objName] = objHandler.get(authenticated, appHandler)
        response.text = response.text .. 'Object ' .. objName .. ' got '
        response.result = 0
    end
end

-- This function validates an object or an array of objects
local function validateObject(objHandler, value)
    local objs = value.objs
    local valid, err
    if objs ~= nil then
        for _, obj in ipairs(objs) do
            valid, err = objHandler.validate(obj, appHandler)
            if valid == false then
                break
            end
        end
    else
        valid, err = objHandler.validate(value, appHandler)
    end
    return valid, err
end

local function postObject(appHandler, value, queryParams, response)
    local objName = value.name
    local objHandler = getObjectHandler(appHandler, "post", objName, queryParams, response)
    if objHandler then
        local valid, err = validateObject(objHandler, value)
        if valid == false then
            response[objName] = objHandler.get(authenticated, appHandler)
            response.text = response.text .. 'Object ' .. objName .. ' invalid because ' .. err
            response.result = 1
        else
            local ret, errorText = objHandler.set(authenticated, value, appHandler)
            response[objName] = objHandler.get(authenticated, appHandler)
            response.text = response.text .. 'Object ' .. objName .. ' set '
            response.result = ret or 0
            if ret and ret ~= 0 and errorText then
                response.errorText = errorText
            end
        end
    end
end

-- Create a new requesthandler with a method get() for HTTP GET.
local CgiHandler = class("CGIHandler", turbo.web.RequestHandler)

function CgiHandler:get(url)
    csrf_session.prepareSession(self)
    local query_string = self:get_argument("req")
    local queryObj = turbo.escape.json_decode(query_string)
    local response = {text = ''}

    if queryObj then
        if queryObj.getObject then
            -- we have an array of objects names getObject
            -- for each use the appropriate helper lua snippet
            for key,value in pairs(queryObj.getObject) do
                local status, err = pcall(getObject, self, value, queryObj.queryParams, response)
                if status == false then
                    response.text = string.format('Failed to getObject %s, err = %s', value, err)
                    response.result = 1
                    break
                end
                if response.result ~= 0 then
                    break
                end
            end
        else
            response.text = 'Unknown GET request'
            response.result = 1
        end
    else
        response.text = 'Invalid GET request'
        response.result = 1
    end

    self:write(response)
    -- do not call updateSession for cgi get
end

function CgiHandler:post(url)
    csrf_session.prepareSession(self)
    local data_string = self:get_argument("data")
    print("CGI ", url, data_string)
    local postObj = turbo.escape.json_decode(data_string)
    local response = {text=''}
    if postObj then
        local session = self.session

        if not csrf_session.verifyCsrfToken(session.csrf, postObj.csrfToken) then
            response.text = 'Invalid csrfToken'
            response.result = 1
        elseif postObj.setObject then
            -- we have an array of objects to setObject
            -- for each use the appropriate helper lua snippet
            for key,value in pairs(postObj.setObject) do
                local status, err = pcall(postObject, self, value, postObj.queryParams, response)
                if status == false then
                    response.text = string.format('Failed to postObject %s, err = %s', value.name, err)
                    response.result = 1
                    break
                end
                if response.result ~= 0 then
                    break
                end
            end
        else
            response.text = 'Unknown POST request'
            response.result = 1
        end
    else
        response.text = 'Invalid POST request'
        response.result = 1
    end

    self:write(response)
    csrf_session.updateSession(self)
end

local UploadHandler = class("UploadHandler", turbo.web.RequestHandler)

-- only for status checking after fw upgrade
-- do not check cookies/csrf here
function UploadHandler:get(url)
    self:add_header('Content-Type', 'application/json')
    local script = "/usr/share/lua/5.1/webif/" .. url .. ".lua"
    if not file_exists(script) then
        error(turbo.web.HTTPError(404, string.format("Script %s not found", script)))
    end
    local succ, mod = pcall(dofile, script)
    if not succ then
        error(turbo.web.HTTPError(500, "Internal server error"))
    end
    local processed_message, extra_messages = mod.get()
    -- Send fw upgrade response as JSONP in order to avoid
    -- CORS(Cross Origin Resource Sharing) issue which happens
    -- when current WEBUI domain address is different from the default
    -- address and browser is trying to get fw upgrade response on
    -- the default address during after f/w upgradinng with factory reset option.
    -- Ex) Set LAN IP to 192.168.4.1 while default LAN IP is 192.168.1.1
    --     then start fw upgrading with factory reset option.
    --
    -- upgradeJsonpParser is the callback function name which should be
    -- defined in client side. Real JSON object is embraced with the callback
    -- function name.
    self:write("upgradeJsonpParser(")
    self:write(string.format('{"result":"0", "message":"%s", "extras":"%s"}',
                             processed_message, extra_messages))
    self:write(");")
end

function UploadHandler:post(url)
    csrf_session.prepareSession(self)

    -- login is required for uploading
    if not validateLogin(self) then
        error(turbo.web.HTTPError(401, "Unauthorised"))
    end

    local script = "/usr/share/lua/5.1/webif/" .. url .. ".lua"
    if not file_exists(script) then
        error(turbo.web.HTTPError(404, string.format("Script %s not found", script)))
    end

    self:add_header('Content-Type', 'application/json')
    local file = self:get_argument("file", "ERROR")
    local name = self:get_argument("name", "ERROR")
    local target = self:get_argument("target", "ERROR")
    local commit = self:get_argument("commit", "ERROR")

    local session = self.session
    local csrfTokenPost = self:get_argument("csrfTokenPost", "")
    if not csrf_session.verifyCsrfToken(session.csrf, csrfTokenPost) then
        error(turbo.web.HTTPError(403, "Invalid csrfToken"))
    end

    if ("ERROR" == file and "0" == commit) or "ERROR" == name or "ERROR" == target then
        error(turbo.web.HTTPError(400, "File not uploaded"))
    end

    local succ, mod = pcall(dofile, script)
    if not succ then
        error(turbo.web.HTTPError(500, "Internal server error"))
    end

    -- write file content to disk
    if file and "ERROR" ~= file then

        if not validate_upload_file_path(name) then
            error(turbo.web.HTTPError(403, "File not stored"))
        end

        local fobj = io.open(name, "wb")
        if nil == fobj then
            error(turbo.web.HTTPError(500, "File not stored"))
        end
        fobj:write(file)
        fobj:close()
        file = nil
    end

    local processed_message, extra_messages, ping_url, ping_delay = mod.post(name, tonumber(commit))
    if not processed_message then
        error(turbo.web.HTTPError(403, string.format("Illegal file name %s", name)))
    end
    self:write(string.format('{"result":"0", "message":"%s", "ping_url":"%s", "ping_delay":%d, "extras":"%s"}',
                             processed_message, ping_url, ping_delay or 0, extra_messages))

    csrf_session.updateSession(self)
end

-- disable .. in the path
local DotHandler = class("DotHandler", turbo.web.RequestHandler)
function DotHandler:prepare()
    turbo.log.error(".. is not allowed in url path")
    error(turbo.web.HTTPError(404, "Not Found"))
end

-- Load plugins
local handlers = {}

-- Read in and execute a *.lua plugin.
-- These should modify handlers[].
-- filename - file to read
local function load_lua_object(filename, handlers, ...)
    -- filename is fully qualified name, so get module name from that
    local module_name = string.match(filename, "^.+/(.+)%.lua$")
    turbo.log.notice("loading "..module_name.." module")
    local res = package.loaded[module_name]
    if not res then
        local chunk, result = loadfile(filename)
        if not chunk then
            turbo.log.error('Error compiling Turbo object '..filename..' result '..(result or '' ))
            return nil
        end
        result, res = pcall(chunk, handlers, ...)
        if not result then
            turbo.log.error('Error loading Turbo object '..filename..' result '..(res or '' ))
            return nil
        end
        package.loaded[module_name] = res
    else
        turbo.log.notice(module_name.." already loaded")
    end
    if res and res.init then
        local res, err = pcall(res.init, handlers, ...)
        if not res then
            turbo.log.error('Error in initialisation of '..filename..', result '..(err or ''))
            return nil
        end
    end
end

--- Look in a directory and execute a function on each file matching prefix/suffix
local function load_object_dirs(dirlist, prefix, suffix, loadfunc, ...)
    local pattern = '^'..prefix..'.*'..suffix
    for _, dirname in pairs(dirlist) do
        local atts, err = lfs.attributes(dirname)
        if atts and atts.mode == "directory" then
            for f in lfs.dir(dirname) do
                -- f is file name without directory component
                if f:match(pattern) then
                    loadfunc(dirname..'/'..f, ...)
                end
            end
        end
    end
end


-- url path -> handler map
local handlers = {
    {"%.%.", DotHandler},
    {"^/(login)$",LoginHandler},
    {"^/(logout)$",LoginHandler},
    {"^/(web_server_status)$",StatusHandler},
    {"^/(network_status)$",NetworkStatusHandler},
    {"^/(clear_log)$",ClearLogHandler},
    {"^/(rdb_command)$",RdbCmdHandler},
    {"^/cgi%-bin/(.*)$", CgiHandler},
    {"^/upload/(.*)$", UploadHandler},
    {"^/$", MustacheHandler},
    {"^/(.*html)$", MustacheHandler}
}

-- Load additional V variable specific handlers
load_object_dirs({g_turbo_dir}, 'handler_', '%.lua$', load_lua_object, handlers)

-- Last entry for all static resources.
table.insert(handlers, {"^/(.*)$", turbo.web.StaticFileHandler, baseDir})

-- get CSRF session
function getCsrfSession()
    return csrf_session
end

-- get cookie secret
local function getCookieSecret()
    return csrf_session.cryptoSecureRandomBytes(32)
end

-- Create two applications for HTTP & HTTPS
local httpApp = turbo.web.Application:new(
    handlers,
    {cookie_secret = getCookieSecret()}
)
local httpsApp = turbo.web.Application:new(
    handlers,
    {cookie_secret = getCookieSecret()}
)

-- set cookie secret for HTTP & HTTPS web servers
local function setCookieSecret()
    httpApp:set_cookie_secret(getCookieSecret())
    httpsApp:set_cookie_secret(getCookieSecret())
end

local ioloopInst = turbo.ioloop.instance()
-- change cookie secret periodically
local COOKIE_SECRET_UPDATE_PERIOD = 24 * 3600 * 1000 -- once a day
ioloopInst:set_interval(
        COOKIE_SECRET_UPDATE_PERIOD,
        setCookieSecret
    )

-- record system up time to turbontc launching so WEBUI rebooting mechanism
-- can estimate booting time. Ignore uptime longer than 120 to record
-- the first uptime only.
local uptimeFile = io.open("/proc/uptime", "rb")
local uptimestr = uptimeFile:read "*a"
local uptimes = uptimestr:explode(" ")
local uptime = math.floor(uptimes[1])
if uptime < 120 then
    luardb.set("system.startup_duration", uptime)
end

turbo.log.debug("turbontc start")

-- get WEBUI login account names
local accountsRdbVal = luardb.get("admin.user.accounts") or "root"
loginAccounts = accountsRdbVal:explode(',')

local httpPort

-- Start HTTP server first & by default
httpPort = tonumber(luardb.get("service.webserver.http_port")) or 80
httpApp:listen(httpPort, nil, {max_body_size=200*1024*1024, max_non_file_size=512*1024, file_upload_validator=validate_upload_file_path})

-- Start HTTPS server if enabled
if luardb.get("service.webserver.https_enabled") == "1" then
    local httpsPort = tonumber(luardb.get("service.webserver.https_port")) or 443
    local key_file = luardb.get("service.webserver.https_key") or "/usr/local/cdcs/webserver/httpswebserver.key"
    local cert_file = luardb.get("service.webserver.https_cert") or "/usr/local/cdcs/webserver/httpswebserver.crt"
    if file_exists(key_file) and file_exists(cert_file) then
        local sslOptions = nil
        sslOptions = {
            key_file = key_file,
            cert_file = cert_file
        }
        httpsApp:listen(httpsPort, nil, {max_body_size=200*1024*1024, max_non_file_size=512*1024, file_upload_validator=validate_upload_file_path, ssl_options=sslOptions})
    else
        turbo.log.error("Can't find key file or certi file")
    end
end

ioloopInst:start()

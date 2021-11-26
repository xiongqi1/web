#!/usr/bin/env lua

--[[
    NTC webif server based on turbo.lua
    This is based on V_WEBIF_VERSION v2b

    Copyright (C) 2019 NetComm Wireless Limited.
--]]

TURBO_SSL = false  -- Secure Cookie and Hash need this flag, but not working right now
baseDir = "/www/"
local cgiDir = baseDir .. "cgi-bin/"
local turbo = require("turbo")
package.path = package.path .. ";" .. cgiDir .. "?.lua"
require('srvrUtils')
require('luardb')
require('lfs')
require("support")
require("stringutil")
require("math")
local g_turbo_dir = "/www/cgi-bin"

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
    turbo.log.debug("test has failed")
    return false
  end

  for _, allowed in pairs(upload_path_allowed) do
    if allowed == path:sub(1, allowed:len()) then
      return true
    end
  end

  return false
end

-- validate user based on cookie
local userCookieKey="session"        -- the id in client side use to indicate the user logging states
local userLevelCookieKey = "userlevel"  -- can be put togehter as json like structure
local sessionStatus = nil            -- global variable to use indicate userlogin status??

-- two type of ways to store: admin.user.name/admin.user.password or just store password under admin.user.[user] in old version.
--rootNameRDB = "admin.user.user"
local rootPasswordRDB = "admin.user.root"
local passwordRdbPrefix = "admin.user."

local formUserName = "username"
local formUserPass = "password"
local securityCookieKey = "this is my secrete"
local defaultPageAfterLogin = config.DEFAULT_PAGE
local cookieExpireTime = 300  -- cookie valid time in seconds

--[[ simple session management
        use struct table to track and record session per user
        { session_id0: { username: xx, csrf:xx, time:xx, logincounts:xx, loginstatus:xx},
          session_id1: { xxx },
        }
--]]
-- session ID 0 is reserved for pre-login session
local presessionId = 0
local sessionTable = {[presessionId] = {id=presessionId}}

local function GetPassword(username)
    --TODO: add check up table to support more user
    local userPassword
    if username == "root" then
        userPassword = luardb.get(rootPasswordRDB)
    end
    turbo.log.debug("User: "..username.." Pass: "..tostring(userPassword))
    return userPassword
end

-- get session cookie as a json object from a request handler
local function getSessionCookie(appHandler)
    local sessionCookie
    if _G.TURBO_SSL then
        sessionCookie = appHandler:get_secure_cookie(userCookieKey, nil, cookieExpireTime)
    else
        sessionCookie = appHandler:get_cookie(userCookieKey)
    end
    turbo.log.debug("sessionCookie: " .. tostring(sessionCookie))
    if not sessionCookie then
        return
    end
    return turbo.escape.json_decode(sessionCookie)
end

-- get the session ID of a given request handler
local function getSessionId(handler)
    local sessionCookie = getSessionCookie(handler)
    if not sessionCookie then
        return
    end
    return sessionCookie["id"]
end

-- clean up the session of a request handler
local function CleanUpSession(appHandler)
    local sessionId = getSessionId(appHandler)
    turbo.log.debug("clean up sessin ID: ".. tostring(sessionId))
    if sessionId then
        sessionTable[sessionId]=nil
    else
        turbo.log.debug("id not found in session table, could be because server was dead seconds ago")
    end
    appHandler:clear_cookie(userCookieKey)  -- delete cookie from client
end

-- seed the random generator once
math.randomseed(turbo.util.gettimeofday())

local function GenerateSession(username)
    turbo.log.debug ("in gen session id")
    local userPassword = luardb.get(rootPasswordRDB)
    local sessionInst = {}
    local sessionCookie = {}
    local id
    sessionInst["username"] = username
    sessionInst["userlevel"] = "admin"
    sessionInst["time"] = turbo.util.gettimeofday()
    sessionInst["loginstatus"] = "invalid"
    sessionInst["logincounts"] = 0
    repeat
        id = math.random(1, 10000)
    until not sessionTable[id]
    sessionInst["id"] = id
    sessionTable[id] = sessionInst
    for k, v in pairs(sessionInst) do
        turbo.log.debug(k..": "..v)
    end
    sessionCookie["id"] = id
    sessionCookie["username"] = sessionInst["username"]
    sessionCookie["userlevel"] = sessionInst["userlevel"]
    -- we should not include csrf token in cookie
    return sessionCookie
end

local function GenerateCookieKey(username)
	return turbo.escape.json_encode(GenerateSession(username))
end

local function SetCookieSession(appHandler,username)
    turbo.log.debug ("in set Cookie")
    local sessionSet = GenerateCookieKey(username)
    if  _G.TURBO_SSL then
        appHandler:set_secure_cookie(userCookieKey, sessionSet,"/", 4, 4*3600)
    else
        appHandler:set_cookie(userCookieKey, sessionSet, "/", 4, 4*3600)
    end
end

local function ValidateUserByCookie(appHandler)
    local sessionId = getSessionId(appHandler)
    if sessionId then
        local session = sessionTable[sessionId]
        if session then
            turbo.log.debug("Valid User: "..session["username"])
            return true
        end
    end
    turbo.log.debug("need clean")
    CleanUpSession(appHandler)    -- server could be just dead and lost everything
    return false   --if no cookie exists or cookie not matched
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
    local cmd = string.format("echo '%s' | openssl enc -base64 -d | openssl passwd -stdin -%s -salt '%s'",
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

-- get stored password (hash) for a user
-- TODO: add other users besides root
local function getStoredPassword(username)
    if username == "root" then
        local passwd = luardb.get(passwordRdbPrefix .. username .. '.encrypted')
        if passwd and #passwd > 0 then
            return true, passwd
        end
        -- for backward compatibility, we still accept clear text passwd
        return false, luardb.get(passwordRdbPrefix .. username)
    end
    return nil
end

-- userInfo has user name and password from login form post message
local function ValidateUser(userInfo)
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
        turbo.web.HTTPError:new(400, "Missing required argument.")
        return
    end
    -- handle logout post request
    if url == "logout" then
        --clear sessionid from cookie
        CleanUpSession(self)
        response.result = 0
        response.text = "See you soon"
        self:write(response)
        return
    end

    -- handle login request
    local loginMsg = self:get_json(true)
    --for k, v in pairs(loginMsg) do
    --        turbo.log.debug(k,v)
    --end

    local session = sessionTable[presessionId]
    local csrfToken = session["csrfToken"]
    local csrfToken_for = session["csrfToken_for"]
    if not loginMsg["csrfToken"] or loginMsg["csrfToken"] ~= csrfToken or csrfToken_for ~= "/index.html" then
        response.result = 1
        response.text = "Invalid csrfToken"
        self:write(response)
        return
    end

    local userIsValid = ValidateUser(loginMsg)
    if userIsValid then
        local userName = loginMsg[formUserName]
        SetCookieSession(self, userName)
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
end


-- Create a new requesthandler with a method get() for HTTP GET.
local MustacheHandler = class("ExampleHandler", turbo.web.RequestHandler)

--[[
    function check if use is logged and render the page with logged result
    and will be exdended to support CSRF token, user level etc to reuse early
    web pages
--]]
local function loggedInCheck(appHandler)
    local validUser = ValidateUserByCookie(appHandler)
    return validUser
end

local ffi = require "ffi"
ffi.cdef[[
int RAND_bytes(unsigned char *buf, int len);
]]
local lssl = ffi.load('ssl')
-- generate a string of crypto secure pseudorandom numbers
local function cryptoSecureRandomBytes(len)
    local buf = ffi.new("char[?]", len)
    if lssl.RAND_bytes(buf, len) == 0 then
        return
    end
    return ffi.string(buf, len)
end

local function fakeRandomBytes(len)
    local rints = {}
    for _ = 1, len do
        rints[#rints + 1] = math.random(0, 255)
    end
    return string.char(unpack(rints))
end

local genRandomBytes = config.fake_random_bytes and fakeRandomBytes or cryptoSecureRandomBytes

-- generate a CSRF token for a session ID and a given page
local function generateCsrfToken(sessionId, pageURL)
    if not sessionId then
        sessionId = 0
    end
    local session = sessionTable[sessionId]
    if not session then
        return
    end
    local token = turbo.escape.base64_encode(genRandomBytes(32))
    -- '/' and '+' are special html chars and will confuse browser and turbo
    token = token:gsub('/', '-'):gsub('+', '_')
    session["csrfToken"] = token
    session["csrfToken_for"] = pageURL
    return token
end

--[[If client has no valid cookie to indicate it has logged in, redirect to index.html,
    once client login sucessfully, server will set the early url to login page to ask redirect
    to right page
--]]
function MustacheHandler:get(url)
    turbo.log.debug("matched url: "..url)
    local isLogin = loggedInCheck(self)
    --[[ notice browser not cache any html file, any new request needs apply whole
         request to server instead of using local whole cache or partial cache content
         mainly to sovle probelm of browser not sending "/html?logoff" instead of not sending
         at all but using local disk cache only.
    --]]
    self:add_header("Cache-Control", "no-store")

    local fileContent = read_file(baseDir..url)
    if nil == fileContent then
        self:redirect("/index.html", true)
        return
    end
    local authCheck = '{{#loggedIn}}'
    -- if a page needs authentication, it starts with authCheck tag
    local authenticatedOnly = (fileContent:sub(1, #authCheck) == authCheck)

    -- allow prelogin access to page with authenticatedOnly=false
    if (not isLogin) and (url ~= "index.html") and authenticatedOnly then
        self:set_cookie("redirect_url", url, "/", 1, true)  --save url to client cookie
        self:redirect("/index.html",true)
        return
    end

    --[[ as self.get_argument() can't get any none value query string, but our current logoff
     is just using /index.html?off as log off request, we have to use lower level header api
     to search the whole url string to find the 'logoff' string
    --]]
    local logoff_query = string.find(self.request.headers:get_url(),"?logoff")
    turbo.log.debug("get_url:"..self.request.headers:get_url())
    if (url == "index.html") and logoff_query then
        turbo.log.debug("logoff query from index get "..logoff_query)
        CleanUpSession(self)
        local response = {}
        response.result = 0
        response.text = "see you"
        self:redirect("/index.html",true)
        return
    end
    if isLogin and (url == "index.html") then  -- for example , access . or / directory after logged in
        self:redirect(defaultPageAfterLogin, true)
        return
    end

    local sessionId = getSessionId(self)
    -- generate a new CSRF token for each page request
    local csrfToken = generateCsrfToken(sessionId, "/" .. url)
    self:write(turbo.web.Mustache.render(substRdb(fileContent),
        {
            loggedIn=isLogin,
            lang="en",
            csrfToken=csrfToken,
        })
    )
end

local response = {}
response.text = ''
local function csrfValid(objHander, csrfToken_for)
  if objHander.pageURL then
    return csrfToken_for and csrfToken_for == objHander.pageURL
  end
  return true
end

local function getObjectHandler(objName, csrfToken_for)
  local fileName = cgiDir..'obj'..objName..'.lua'
  if file_exists(fileName) then
    dofile(fileName)
    if objHander then
      if csrfValid(objHander, csrfToken_for) == false then
        response.text = 'Invalid csrfToken'
        response.result = 1
      elseif objHander.authenticatedOnly and (authenticated == false) then
        response.access = 'NeedsAuthentication'
        response.text = response.text .. 'Object ' .. objName .. ' denied '
      else
        return objHander
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

local function getObject(objName, csrfToken_for)
  local objHander = getObjectHandler(objName, csrfToken_for)
  if objHander then
    response[objName] = objHander.get(authenticated)
    response.text = response.text .. 'Object ' .. objName .. ' got '
    response.result = 0
  end
end

-- This function validates an object or an array of objects
local function validateObject(objHander, value)
  local objs = value.objs
  local valid, err
  if objs ~= nil then
    for _, obj in ipairs(objs) do
      valid, err = objHander.validate(obj)
      if valid == false then
        break
      end
    end
  else
    valid, err = objHander.validate(value)
  end
  return valid, err
end

local function postObject(value, csrfToken_for)
  local objName = value.name
  local objHander = getObjectHandler(objName, csrfToken_for)
  if objHander then
      local valid, err = validateObject(objHander, value)
      if valid == false then
        response[objName] = objHander.get(authenticated)
        response.text = response.text .. 'Object ' .. objName .. ' invalid because ' .. err
        response.result = 1
      else
        local ret = objHander.set(authenticated,value)
        response[objName] = objHander.get(authenticated)
        response.text = response.text .. 'Object ' .. objName .. ' set '
        response.result = ret or 0
      end
  end
end

-- Create a new requesthandler with a method get() for HTTP GET.
local CgiHandler = class("CGIHandler", turbo.web.RequestHandler)

function CgiHandler:get(url)
    if url == 'usermenu.cgi' then
      self:write("")
      return
    end
    local query_string = self:get_argument("req")
    --print("CGI ", url, query_string)
    local queryObj = turbo.escape.json_decode(query_string)
    --for k, v in pairs(queryObj) do
      --  print(k,v)
    --end
    response = {}
    response.text = ''

    if queryObj then
        local sessionId = getSessionId(self) or presessionId
        local csrfToken, csrfToken_for
        local session = sessionTable[sessionId]
        if session then
          csrfToken = session["csrfToken"]
          csrfToken_for = session["csrfToken_for"]
        end
        if (queryObj.csrfToken == nil) or (csrfToken == nil) or (queryObj.csrfToken ~= csrfToken) then
          response.text = 'Invalid csrfToken'
          response.result = 1
        elseif queryObj.getObject then
          -- we have an array of objects names getObject
          -- for each use the appropriate helper lua snippet
          for key,value in pairs(queryObj.getObject) do
            local status, err = pcall(getObject, value, csrfToken_for)
            if status == false then
              response.text = 'Error ' .. err
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
end

function CgiHandler:post(url)
  if url == 'usermenu.cgi' then
    self:write("")
    return
  end
  local data_string = self:get_argument("data")
  print("CGI ", url, data_string)
  local postObj = turbo.escape.json_decode(data_string)
  --for k, v in pairs(postObj) do
    --  print(k,v)
  --end
  response = {}
  response.text = ''
  if postObj then
    local sessionId = getSessionId(self) or presessionId
    local csrfToken, csrfToken_for
    local session = sessionTable[sessionId]
    if session then
      csrfToken = session["csrfToken"]
      csrfToken_for = session["csrfToken_for"]
    end

    if (postObj.csrfToken == nil) or (csrfToken == nil) or (postObj.csrfToken ~= csrfToken) then
      response.text = 'Invalid csrfToken'
      response.result = 1
    elseif postObj.setObject then
      -- we have an array of objects to setObject
      -- for each use the appropriate helper lua snippet
      for key,value in pairs(postObj.setObject) do
        local status, err = pcall(postObject, value, csrfToken_for)
        if status == false then
          response.text = 'Error ' .. err
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
end

local UploadHandler = class("UploadHandler", turbo.web.RequestHandler)

function UploadHandler:post(url)
  self:add_header('Content-Type', 'application/json')
  local file = self:get_argument("file", "ERROR")
  local name = self:get_argument("name", "ERROR")
  local target = self:get_argument("target", "ERROR")
  local commit = self:get_argument("commit", "ERROR")

  local sessionId = getSessionId(self) or presessionId
  local csrfToken, csrfToken_for
  local session = sessionTable[sessionId]
  if session then
    csrfToken = session["csrfToken"]
    csrfToken_for = session["csrfToken_for"]
  end
  local csrfTokenPost = self:get_argument("csrfTokenPost", "ERROR")
  if csrfTokenPost == "ERROR" or csrfTokenPost ~= csrfToken or csrfToken_for ~= "/" .. url .. ".html" then
    error(turbo.web.HTTPError(403, "Invalid csrfToken"))
    return nil
  end

  if ("ERROR" == file and "1" ~= commit) or "ERROR" == name or "ERROR" == target then
    error(turbo.web.HTTPError(400, "File not uploaded"))
    return nil
  end

  -- write file content to disk
  if file and "ERROR" ~= file then

    if not validate_upload_file_path(name) then
      error(turbo.web.HTTPError(403, "File not stored"))
      return nil
    end

    local fobj = io.open(name, "wb")
    if nil == fobj then
      error(turbo.web.HTTPError(500, "File not stored"))
      return nil
    end
    fobj:write(file)
    fobj:close()
    file = nil
  end

  local script = baseDir .. "cgi-bin/" .. url .. ".lua"
  if not file_exists(script) then
    os.remove(name)
    error(turbo.web.HTTPError(404, "File not processed"))
    return nil
  end

  -- lua handlers recieve the uploaded file path from uploaded_file
  -- in case for multiple handlers in one lua file, use uploaded_target to distinguish
  -- lua handlers change processed_message to return a message to web service callers
  uploaded_file = name
  uploaded_target = target
  uploaded_commit = commit=="1"

  processed_message = "processing"
  extra_messages = ""

  -- a test url to check if the upgrade is done and web is back
  ping_url = "/"
  -- a best estimate when the web would be back after flashing
  ping_delay = 0

  if not pcall(dofile, script) then
    error(turbo.web.HTTPError(500, "Internal server error"))
    return nil
  end

  self:write(string.format('{"result":"0", "message":"%s", "ping_url":"%s", "ping_delay":%d, "extras":"%s"}',
    processed_message, ping_url, ping_delay, extra_messages))

  -- close the current socket: this is a workaround for the problem that the underlying
  -- socket stops working after a large file is being transferred - although the
  -- browser trace clearly shows the post is sent, it never reaches the handler on server side.
  self:flush(function(stream) stream:close() end, self.request.connection.stream)
end

-- Create an Application object and bind our HelloWorldHandler to the route '/hello'.
local app = turbo.web.Application:new({
    {"/vdf%-lib/css/(.*)$", turbo.web.StaticFileHandler, baseDir.."/vdf-lib/css/"},
    {"^/css/(.*)$", turbo.web.StaticFileHandler, baseDir.."css/"},
    {"^/img/(.*)$", turbo.web.StaticFileHandler, baseDir.."img/"},
    {"^/js/(.*)$", turbo.web.StaticFileHandler, baseDir.."js/"},
    {"^/download/(.*)$", turbo.web.StaticFileHandler, baseDir.."download/"},
    {"^/(favicon.ico)$", turbo.web.StaticFileHandler, baseDir},
    {"^/(login)$",LoginHandler},
    {"^/(logout)$",LoginHandler},
    {"^/cgi%-bin/(.*)$", CgiHandler},
    {"/upload/(.*)$", UploadHandler},
    {"^/$", MustacheHandler},
    {"^/(.*html)$", MustacheHandler}
    },
    {cookie_secret = securityCookieKey}
)

-- use default port number 80 or the number given by the first command line parameter
local server_port = tonumber(arg[1]) or 80

-- indicate turbontc start event so WEBUI can detect it while waiting for boot up
luardb.set("service.turbontc.status", "started")

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

app:listen(server_port, nil, {max_body_size=200*1024*1024, max_non_file_size=512*1024, file_upload_validator=validate_upload_file_path})
turbo.ioloop.instance():start()
luardb.set("service.turbontc.status", "stopped")


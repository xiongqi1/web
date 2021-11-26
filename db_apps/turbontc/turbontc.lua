#!/usr/bin/lua
-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2018 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------
--- turbontc.lua is an executable script that parses a whole directory of files (and configures
--  the Turbo webserver with the info it finds.

local luardb = require("luardb")
local lfs = require("lfs")
local util = require("lwm2m_util")
require('stringutil')

luasyslog.close() -- Close luasyslog since it has been opened by lwm2m_util
luasyslog.open('turbontc', 'LOG_DAEMON')

--redfine a print function
function print(str)
	luasyslog.log('LOG_INFO', str)
end

function logInfo(str)
	luasyslog.log('LOG_INFO', str)
end

function logDebug(str)
	luasyslog.log('LOG_DEBUG', str)
end

function logErr(str)
	luasyslog.log('LOG_ERR', str)
end

function logWarn(str)
	luasyslog.log('LOG_WARNING', str)
end

function logNotice(str)
	luasyslog.log('LOG_NOTICE', str)
end

-- Disable signal mode for RDB so that it can be selected on file descriptor.
local cb, rdbFD = luardb.synchronousMode(true)

_G.TURBO_SSL=true
-- Turbo must NOT be local or ioloop breaks.
turbo = require("turbo")

local history
local serverAddress, serverPort
local serverKeyFile, serverCertFile, clientCaFile
local dirlist = {}
local maxBodySize, maxNonFileSize
local ignoreSecurityKey
local wikitable, luatemplate

-- callback function when verifying client certificate
local verify_callback

-- process command line arguments
-- -h enable history
-- -s <server address> : default from vlan.admin.address
-- -p <server port> : default from service.turbontc.port
-- -k <key> : default from service.turbontc.serverkeyfile
-- -c <cert> : default from service.turbontc.servercrtfile
-- -a <ca> : default from service.turbontc.cafile
-- -d <plugin dir> : default /usr/share/turbontc
-- -b <max body size>
-- -n <max non file size>
-- -i ignore SecurityKey query string
-- -w generate all APIs in mediawiki table format. server will not be launched

local ix = 1
while arg and arg[ix] do
	if arg[ix] == '-h' then
		-- historyDB library, should be loaded after _G.TURBO_SSL setting since it loads turbo inside
		-- and _G.TURBO_SSL should be set before turbo loading
		history = require("historyDB")
	elseif arg[ix] == '-s' then
		ix = ix + 1
		serverAddress = arg[ix]
	elseif arg[ix] == '-p' then
		ix = ix + 1
		serverPort = tonumber(arg[ix])
	elseif arg[ix] == '-k' then
		ix = ix + 1
		serverKeyFile = arg[ix]
	elseif arg[ix] == '-c' then
		ix = ix + 1
		serverCertFile = arg[ix]
	elseif arg[ix] == '-a' then
		ix = ix + 1
		clientCaFile = arg[ix]
	elseif arg[ix] == '-d' then
		ix = ix + 1
		dirlist[#dirlist + 1] = arg[ix]
	elseif arg[ix] == '-b' then
		ix = ix + 1
		maxBodySize = tonumber(arg[ix])
	elseif arg[ix] == '-n' then
		ix = ix + 1
		maxNonFileSize = tonumber(arg[ix])
	elseif arg[ix] == '-i' then
		ignoreSecurityKey = true
	elseif arg[ix] == '-w' then
		wikitable = ''
		luatemplate = ''
	end
	ix = ix + 1
end

-- needed for swagger-UI if it's installed on a different server
function turbo.web.RequestHandler:set_default_headers()
	self:add_header("Access-Control-Allow-Origin", "*")
end

local generic_model = {}
local maps = {}

--- Dereference a section of a Swagger document tree by replacing "$ref" with the referenced subtree.
-- obj - the section of the tree to dereference
-- def - the 'definitions' section of the Swagger document
-- returns the result, but obj may be modified too
local function swagger_dereference(obj, def)
	if type(obj) ~= 'table' then
		return obj
	end
	if obj["$ref"] then
		local ref = def[string.gsub(obj["$ref"], "^#/definitions/", '')]
		if ref then
			return swagger_dereference(ref, def)
		end
		-- if lookup fails then remove it and continue
		obj["$ref"] = nil
	end
	local k,v
	for k,v in pairs(obj) do
		-- Lua5.1 Manual says writing or clearing a field during iteration is OK
		obj[k] = swagger_dereference(v, def)
	end
	return obj
end

--- Check model against Swagger schema
local function check_schema(schema, model, path)

	if not schema then return end
	if schema.type == 'array' then
		if model then
			if model.INSTANCES and model.ITEMS then
				model = model.ITEMS
			else
				logWarn("WARNING: schema for "..path.." is an array, but model isn't")
			end
		end
		schema = schema.items
	end
	if (schema.type == nil or schema.type == 'object') and schema.properties then
		schema = schema.properties
	else
		logInfo("WARNING: generic_model used for  "..path..", which is not an object")
		return false
	end

	local has_error, k, v = false, nil, nil
	local function do_error(pre, key)
		has_error = true
		logErr(pre .. path.." on key "..key)
	end
	for k, v in pairs(schema) do
		if model and not model[k] then do_error("WARNING schema item missing from model in ", k) end
		--table of all APIs in mediawiki format
		if wikitable then
			wikitable = wikitable.."|-\n|"..path.."||"..k.."||"..(v.description ~= '' and v.description or ' ').."||\n"
		end
		if luatemplate then
			luatemplate = luatemplate.."\t\t\t"..k.. '= util.map_fix("448848000"),\n'
		end
	end
	if model then
		for k in pairs(model) do
			if not schema[k] then do_error("WARNING model item missing from schema in ", k) end
		end
	end
	return true, schema, model
end

-- generic handler for a JSON api - there's one instance for each Swagger file
-- The reason for this is so that the total number of handlers is minimised.
-- This way if the basePath doesn't match then it skips all of the APIs in it instantly.
local turbontc_api_handler = class("turbontc_api_handler", turbo.web.RequestHandler)
--- Print error and send to client
function turbontc_api_handler:doError(code, message)
	logErr(message)
	self:set_status(code)
	self:write(message)
	return code
end

--- Find the appropriate function for the given method/path and execute it.
-- The function would normally be one of the generic_model_*().
-- But the lua files that define the model can override that if need be.
function turbontc_api_handler:run_func(method, path, ...)
	logInfo("turbontc_api_handler:"..method.." "..path)
	local k,v
	for k,v in ipairs(self.options) do
		--logInfo("checking: "..v[1])
		if string.match(path, v[1]) then
			if not v[2][method] then
				return self:doError(404, "Method "..method.." not found for path "..path)
			end
			return v[2][method](self, path, ...)
		end
	end
	return self:doError(404, "Path "..path.." not found")
end

-- these all share the same base code
function turbontc_api_handler:get(...)
	--logInfo("turbontc_api_handler:get")
	self:run_func("get", ...)
end
function turbontc_api_handler:put(...)
	--logInfo("turbontc_api_handler:put")
	self:run_func("put", ...)
end
function turbontc_api_handler:post(...)
	--logInfo("turbontc_api_handler:post")
	self:run_func("post", ...)
end
function turbontc_api_handler:delete(...)
	--logInfo("turbontc_api_handler:delete")
	self:run_func("delete", ...)
end

lua_object_dir = {} -- record the directory where a lua object is loaded from

--- Read in and execute a *.lua plugin.
-- These should modify either maps[] or handlers[].
-- filename - file to read
local function load_lua_object(filename, maps, handlers, ...)
	-- filename is fully qualified name, so get module name from that
	local dirname, module_name = string.match(filename, "^(.+)/(.+)%.lua$")
	local res = package.loaded[module_name]
	if not res then
		local chunk, result = loadfile(filename)
		if not chunk then
			logErr('Error compiling Turbo object '..filename..' result '..(result or '' ))
			return nil
		end
		lua_object_dir[module_name] = dirname
		result, res = pcall(chunk, maps, handlers, ...)
		if not result then
			logErr('Error loading Turbo object '..filename..' result '..(res or '' ))
			return nil
		end
		package.loaded[module_name] = res
	else
		logDebug(module_name.." already loaded")
	end
	if res and res.init then
		local res, err = pcall(res.init, maps, handlers, ...)
		if not res then
			logErr('Error in initialisation of '..filename..', result '..(err or ''))
			return nil
		end
	end
	if res and res.verify_callback then
		-- only one verify_callback is supported for now.
		-- could be extended to a callback chain if necessary.
		verify_callback = res.verify_callback
	end
end

--- Read in and parse a Swagger schema in JSON format
-- Will create a handler and use the maps[] to hook it up.
-- filename - file to read
local function load_json_object(filename, maps, handlers, intervalHandlers)
	local fh = io.open(filename, 'r')
	local doc=fh:read("*all")
	fh:close()

	local jsonspec = turbo.escape.json_decode(doc)
	local thisapi = {}
--	if luatemplate then
--		luatemplate = luatemplate..'local module = {}\nfunction module.init(maps, _, util)\n\tlocal basepath = "'..jsonspec.basePath..'"\n'
--	end
	for path,v1 in pairs(jsonspec.paths) do
		local wholepath = jsonspec.basePath .. path
		local pathfuncs = {}
		local h
		for method,v2 in pairs(v1) do
			if luatemplate then
				luatemplate = luatemplate..'\t\t}\n\t}\n\tmaps[basepath.."'..path..'"] = {\n\t\tget = {code = "200"},\n\t\tmodel = {\n'
			end
			local schemas = {}
			for code,v3 in pairs(v2.responses) do
				schemas[code] = swagger_dereference(v3.schema, jsonspec.definitions)
			end
			if maps[wholepath] and maps[wholepath][method] then
				-- turbo.log.notice(">>>>>> "..wholepath.." method "..method.." function is added!")
				if method == "get" and maps[wholepath].history then -- There is a history section defined, fill in poll handler for this target
					local limit = maps[wholepath].history.limit or 97 -- Default item number is 24 hours' entries in 15 minutes' interval
					local compressed = maps[wholepath].history.compressed or false
					if maps[wholepath].history.poll then -- Poll mode
						local ih = {}
						ih.interval = maps[wholepath].history.poll.interval or tonumber(luardb.get("service.turbontc.history.poll_interval")) or 15*60 -- Default interval is 15 minutes
						ih.func = generic_model.poll(wholepath, "history", schemas, maps, jsonspec)
						intervalHandlers[wholepath] = ih
					elseif maps[wholepath].history.watch then -- Event tirggering mode
						if maps[wholepath].history.watch.rdbName == nil then
							error("An RDB should be defined in watch section!")
						end
						if type(maps[wholepath].history.watch.rdbName) == "string" then
							luardb.watch(maps[wholepath].history.watch.rdbName, generic_model.poll(wholepath, "history", schemas, maps, jsonspec))
							logInfo(string.format("RDB %s is being watched for target %s", maps[wholepath].history.watch.rdbName, wholepath))
						elseif type(maps[wholepath].history.watch.rdbName) == "table" then
							for _, v in ipairs(maps[wholepath].history.watch.rdbName) do
								luardb.watch(v, generic_model.poll(wholepath, "history", schemas, maps, jsonspec))
								logInfo(string.format("RDB %s is being watched for target %s", v, wholepath))
							end
						end
					end
					local count = maps[wholepath].history.limitCount or 2
					history:registerTarget(wholepath, limit, count, compressed) --Register target in historyDB
					h = generic_model.historyGet -- Get method would be historyDB:get
				elseif method == "delete" and maps[wholepath].history then -- Special delete model for history delete
					h = generic_model.historyDelete
				else
					-- this allows the model to override the handler function
					h = maps[wholepath][method].handler or generic_model[method]
				end
				-- this allows the handler function to do some checking during startup
				pathfuncs[method] = h(wholepath, method, schemas, maps, jsonspec)
			else
				logWarn("WARNING: No handler defs for "..method.." "..wholepath)
		-- this isn't actually needed but it allows printing out all models
				check_schema(schemas['200'], nil, wholepath)
			end
		end
		if maps[wholepath] then
			-- Note: old turbo (v1.1.3) uses lower case in request path while later version is case sensitive.
			local path = turbo.MAJOR_VERSION >= 2 and wholepath or string.lower(wholepath)
			table.insert(thisapi, {maps[wholepath].pattern or ('^'..path..'$'), pathfuncs})
		end
	end

	-- make the swagger schema available at /<BasePath>/swagger.json
	local basePath = turbo.MAJOR_VERSION >= 2 and jsonspec.basePath or string.lower(jsonspec.basePath)
	table.insert(handlers, {'^'..basePath..'/swagger.json', turbo.web.StaticFileHandler, filename})
	table.insert(handlers, {'^('..basePath..'.*)$', turbontc_api_handler, thisapi})
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
					logInfo("Loading "..f)
					loadfunc(dirname..'/'..f, ...)
				end
			end
		end
	end
end

local function no_op()
	return {}
end

--- Convert anything to boolean - nil, false, 0, '' => false, anything else => true
local function fbool(val)
	local t = type(val)
	if t == 'string' then
		return val ~= ''
	elseif t == 'number' then
		return val ~= 0
	else
		return val and true or false
	end
end

-- Convert any number string to number and anything else to 0
local function fnumber(val)
	local ret = tonumber(val)
	if not ret then
		ret = 0
	end
	return ret
end

--- Like atoi() - convert anything to number even if it's stupid.
-- E.g. "123crap" => 123, ""/false/nil -> 0, anything else -> 1
local function fint(val)
	local t = type(val)
	if t == 'number' then
		return math.floor(val+0.5)
	elseif t == 'string' then
		local str = string.match(val, "^[1234567890+-]*")
		if str == "" then
			return (val ~= "") and 1 or 0
		else
			return tonumber(str) or 0
		end
	end
	return 0
end

--- Like tostring(), except it returns "" for nil
local function fstr(val)
	return (val == nil) and "" or tostring(val)
end

--- Enforce array type returned
local function farray(val)
	-- While we could use the same metatable approach that turbo's encode library uses to signal
	-- an array, or enforce that all keys are numeric in ranged 1..#, for simplicity we'll just
	-- enforce that val is a table, otherwise return an empty table (discarding val).
	return (type(val) == 'table') and val or {}
end

--- This is used to enforce that the data types in a JSON object match the schema
-- We're currently only using a couple of these, so it could be simplified.
local force_json_type = { integer = fint, number = fnumber, string = fstr, boolean = fbool, array = farray }
-- might want to expand the above to deal with Swagger's "format" field:
-- int32, int64, float, double, byte, binary, date, date-time, password

-- handle types not found by bit changing the value - this happens if the schema is faulty
-- e.g. someone writes 'type: "midget"' or something
setmetatable(force_json_type, { __index = function(_, key)
	logErr("ERROR: BAD TYPE "..key)
	return function(val)
		return val
	end
end })

function generic_model.observe_callback(path, value)
	logInfo("got observe", path, value)
	local fields = path:split("#")
	local path = fields[1]
	local prop = fields[2]
	if maps[path].observe and maps[path].observe[prop] then -- There is defined callback for this property
		maps[path].observe[prop](value)
	end
end

function generic_model._do_model(maps, path, schema, model, observe, ot)
	local k,v
	local ret, extra = {}, {}
	if maps[path].get.extra then
		extra = maps[path].get.extra(ot)
	end
	for k,v in pairs(model) do
		-- get the value, force it into the right type for the schema and assign to result
		if not schema[k] then
			-- if not in schema then ignore it (we already logInfoed an warning)
		elseif observe then
			if v.observe then v.observe(ot, path..'#'..k, generic_model) end
		elseif type(v) == 'string' then
			ret[k] = force_json_type[schema[k].type](extra[v])
		elseif v.get then
			ret[k] = force_json_type[schema[k].type](v.get(ot))
		end
	end
	return ret
end

--- Uses the model to generate the response by executing each map function in it one at a time
-- and then combining the result.
-- When run it does checks the model against the schema then returns a function to do the actual work.
function generic_model.get(path, method, schemas, maps, jsonspec)
	local code = maps[path].get.code or '200'
	local schema = schemas[code]
	local desc = path..'$'..code
	local model = maps[path].model
	local ok

	-- First check that the model makes sense against the schema
	if not model then
		logErr("ERROR: no model for "..desc)
		return no_op
	end
	if not schema then
		logErr("ERROR: no schema for "..path)
		return no_op
	end

	ok, schema, model = check_schema(schema, model, desc)
	if not ok then
		return no_op
	end

	-- get one object, by calling each map in the model

	local function choose_func(method)
		-- now we need to return a function that will actually send the data
		if schemas[code].type == 'array' then
			-- for an array we need to do that once per instance, then write the whole array
			return function(self)
				local ret = {}
				for _,v in pairs(maps[path].model.INSTANCES) do
					table.insert(ret, generic_model._do_model(maps, path, schema, model, method, {instance=v}))
				end
				self:write(ret)
			end
		else
			-- otherwise just write it as-is
			return function(self)
				self:write(generic_model._do_model(maps, path, schema, model, method, {instance=''}))
			end
		end
	end

	choose_func(true)({write=function() end})
	return choose_func()
end

-- put model assumes all properties setting made through url parameters
-- and the paramters' names are the same as the properties' names
function generic_model.put(path, method, schemas, maps, jsonspec)
	local code = maps[path].put.code or '200'
	local schema = schemas[code]
	local desc = path..'$'..code
	local model = maps[path].model

	-- Set one object, by calling each map in the model
	local function do_model(ot, args)
		local k,v
		for k,v in pairs(model) do
			if v.put then
				-- Spec defines different character case between parameters and fileds.
				-- For example, in Activation target, "Active" is defined in Parameters
				-- table, while it is "active" in the model. So insensitive case lookup
				-- here to make spec and customer happy
				for key,value in pairs(args) do
					if k:lower() == key:lower() then
						v.put(ot, value)
					end
				end
			end
		end
	end

	return function(self)
		local args = self.request.arguments or {}
		local keyVal = ""
		for k,v in pairs(args) do
			if k:lower() == "securitykey" then -- Case insensive comparision for user's convenience
				keyVal = v
				args[k] = nil -- Delete securityKey since it is not part of model
				break
			end
		end
		if not ignoreSecurityKey and keyVal ~= "OWA" then
			self:set_status(400)
			return
		end
		if maps[path].put and maps[path].put.trigger and type(maps[path].put.trigger) == "function" then -- There is a trigger handler defined, which has a higher priority
			maps[path].put.trigger()(self)
		end
		if model then
			do_model({instance=""}, args)
		end
	end
end

function generic_model.post(path, method, schemas, maps, jsonspec)
end

function generic_model.delete(path, method, schemas, maps, jsonspec)
	return function(self)
		local args = self.request.arguments or {}
		local keyVal = ""
		for k,v in pairs(args) do
			if k:lower() == "securitykey" then -- Case insensive comparision for user's convenience
				keyVal = v
				args[k] = nil -- Delete securityKey since it is not part of model
				break
			end
		end
		if not ignoreSecurityKey and keyVal ~= "OWA" then
			self:set_status(400)
			return
		end
		if maps[path].delete and maps[path].delete.trigger and type(maps[path].delete.trigger) == "function" then -- There is a trigger handler defined, which has a higher priority
			maps[path].delete.trigger()(self)
		end
	end
end

-- Poll model for history
function generic_model.poll(path, method, schemas, maps, jsonspec)
	local code = maps[path].get.code or '200'
	local schema = schemas[code]
	local model = maps[path].model
	local ok

	ok, schema, model = check_schema(schema, model, path)
	if not ok then
		return no_op
	end

	local function pollFunc(observe, history)
		return function(rdbName)
			if not observe then
				if rdbName then
					logInfo(rdbName.." is triggered for path"..path)
				else
					logInfo("path "..path.." is triggered")
				end
				if maps[path].history.watch then
					if type(maps[path].history.watch.expected) == "string" then -- A pattern match for string
						local res = luardb.get(maps[path].history.watch.rdbName)
						if not res:find(maps[path].history.watch.expected) then
							return
						end
					elseif type(maps[path].history.watch.expected) == "function" then -- A judge function
						if not maps[path].history.watch.expected() then
							return
						end
					end
				end
			end
			if maps[path].history.preAction then -- Do something before poll starts
				if not maps[path].history.preAction(rdbName) then -- No poll if preAction fails.
					if not observe then
					    logDebug("not polling because preAction failed")
					    return
					end
				end
			end
			if schemas[code].type == 'array' and #maps[path].model.INSTANCES > 0 then
				for _,v in pairs(maps[path].model.INSTANCES) do
					local ret = generic_model._do_model(maps, path, schema, model, observe, {instance=v})
					history:save(path, ret)
				end
			else
				local ret = generic_model._do_model(maps, path, schema, model, observe, {instance=''})
				history:save(path, ret)
			end
			if maps[path].history.postAction then -- Do something after poll finished
				maps[path].history.postAction(rdbName)
			end
		end
	end

	pollFunc(true, {save=function()end})() -- call pollfunc with a dummy history:save to observe properties
	return pollFunc(false, history)
end

-- History get model
function generic_model.historyGet(path, method, schemas, maps, jsonspec)

	return function(self)
		local ret = {}
		if maps[path].get and maps[path].get.compare then
			ret = history:getLatestFirst(path, maps[path].get.compare.func()(self), maps[path].get.compare.field)
		else
			ret = history:getLatestFirst(path)
		end
		if maps[path].history and maps[path].history.currentStatusIncluded then -- Include the current status if it is required
			local code = maps[path].get.code or '200'
			local model = maps[path].model
			local schema = schemas[code]
			local current
			_, schema, model = check_schema(schema, model, desc)
			if schemas[code].type == 'array' and #maps[path].model.INSTANCES > 0 then
				for _,v in pairs(maps[path].model.INSTANCES) do
					current = generic_model._do_model(maps, path, schema, model, false, {instance=v})
					table.insert(ret, 1, current)
				end
			else
				current = generic_model._do_model(maps, path, schema, model, false, {instance=''})
				table.insert(ret, 1, current)
			end
		end
		if maps[path].history and maps[path].history.append then -- Append other data if it is required
			if maps[path].history.append.func then
				local tmp =  maps[path].history.append.func()
				if tmp and tmp ~= {} then
					if maps[path].history.append.beginning then
						table.insert(ret, 1, tmp)
					else
						table.insert(ret, tmp)
					end
				end
			end
		end

		if maps[path].history and maps[path].history.entryNumber then -- Entry number is required, fill in entry number
			for i, v in ipairs(ret) do
				v.NumberofEntries = i
			end
		end
		self:write(ret)
	end
end

-- History delete model
function generic_model.historyDelete(path, method, schemas, maps, jsonspec)
	return function(self)
		local args = self.request.arguments or {}
		local keyVal = ""
		for k,v in pairs(args) do
			if k:lower() == "securitykey" then -- Case insensive comparision for user's convenience
				keyVal = v
				args[k] = nil -- Delete securityKey since it is not part of model
				break
			end
		end
		if not ignoreSecurityKey and keyVal ~= "OWA" then
			self:set_status(400)
			return
		end
		if maps[path].delete and maps[path].delete.compare then -- There is a delete filter
			history:delete(path, maps[path].delete.compare.func()(self), maps[path].delete.compare.field)
		else
			history:delete(path)
		end
	end
end

if history then
	local historyLocation = luardb.get('service.turbontc.historyrepo')
	local r = turbo.fs.stat(historyLocation, nil)
	if r == -1 then -- If not existing, create it
		os.execute("mkdir "..historyLocation) -- No LuaFileSystem module available, so use "execute" system call here which is not safe though.
	end
	-- Initialize historyDB
	history:initialize(luardb.get('service.turbontc.historyrepo'))
end

-- Load plugins
local handlers = {}
local intervalHandlers = {}
-- authorization object
local authorizer = {}

-- A dictionary of headers_callback functions
--   {basepath: headers_callback}
-- Each module can register its own headers_callback with its basepath.
-- When a request is received, the request uri will be matched against basepath,
-- and the first matched headers_callback will be invoked.
-- This can be used to reject a request at the early stage of request processing.
-- If there is no match, the request is allowed.
local headers_callbacks = {}

-- A dictionary of upload_path_callback (build_file_upload_full_path) functions
--  {basepath: upload_path_callback}
-- Each module can register its own upload_path_callback with its basepath.
-- When a request is received, the request uri will be matched against basepath,
-- and the first matched upload_path_callback will be invoked.
-- This is used to construct the full path to save uploaded file in the case of
-- a large file, whose size is larger than max_non_file_size. This is also where
-- invalid file or filename should be rejected.
-- For each basepath where a file upload is expected, there should be one entry
-- in this dictionary.
-- If no match is found, the requested file upload will be rejected.
local upload_path_callbacks = {}

-- Find and run a matched headers_callback
--
-- @param headers An httputil.HTTPParser instance
-- @param request An httpserver.HTTPRequest instance
-- @return true if the request is allowed;
--   false, request_error (an httpserver.HTTPRequestError instance) if the request is denied
local function headers_callback(headers, request)
	-- invoke the callback according to basepath
	for basepath, cb in pairs(headers_callbacks) do
		if request.uri:startsWith(basepath) then
			return cb(headers, request)
		end
	end
	return true -- no callback matches, green light
end

-- Find and run a matched upload_path_callback
--
-- @param file_name The 'name' form field value of the request
-- @param uri URI of the request
-- @param method HTTP method of the request
-- @return The full path where the uploaded file should be saved;
--   false, error_code, error_message if uploade file should be rejected
local function upload_path_callback(file_name, uri, method)
	-- invoke the callback according to basepath
	for basepath, cb in pairs(upload_path_callbacks) do
		if uri:startsWith(basepath) then
			return cb(file_name, uri, method)
		end
	end
	return false -- no callback matches, red light
end

if #dirlist == 0 then
	dirlist = {"/usr/share/turbontc"}
end

for _, dirname in pairs(dirlist) do
	package.path = dirname.."/?.lua;"..package.path
end
load_object_dirs(dirlist, '', '%.lua$', load_lua_object, maps, handlers, util, authorizer, headers_callbacks, upload_path_callbacks)
load_object_dirs(dirlist, '', '%.json$', load_json_object, maps, handlers, intervalHandlers)

-- setup handler for static files
table.insert(handlers, {'^/(.*)$', turbo.web.StaticFileHandler, '/var/www/'})


if wikitable then
	logInfo(wikitable)
	logInfo(luatemplate)
	os.exit()
end


-- Assemble turbo.web.Application
local serverPrefix = "http://"

if not serverAddress then
	serverAddress = luardb.get('vlan.admin.address')
end
if not serverPort then
	serverPort = tonumber(luardb.get('service.turbontc.port'))
end

if not serverKeyFile and luardb.get("service.turbontc.httpenabled") == "1" then
	turbo.web.Application(handlers):listen(serverPort, serverAddress)
else
	serverPrefix = "https://"
	if not serverKeyFile then
		serverKeyFile = luardb.get('service.turbontc.serverkeyfile')
	end
	if not serverCertFile then
		serverCertFile = luardb.get('service.turbontc.servercrtfile')
	end
	if not clientCaFile then
		clientCaFile = luardb.get('service.turbontc.cafile')
	end
	turbo.web.Application(handlers):listen(serverPort, serverAddress, {
		headers_callback = headers_callback,
		build_file_upload_full_path = upload_path_callback,
		max_body_size = maxBodySize,
		max_non_file_size = maxNonFileSize,
		ssl_options = {
			key_file=serverKeyFile,
			cert_file=serverCertFile,
			verify_ca=true,
			ca_path=clientCaFile,
			verify_callback=verify_callback,
		}})
end
local instance = turbo.ioloop.instance()
instance:add_handler(rdbFD, turbo.ioloop.READ, cb) -- To add RDB fd handler
-- Adding interval handlers
for target, v in pairs(intervalHandlers) do
	instance:set_interval(v.interval*1000, v.func)
end
-- Start the service
logNotice("Starting service on "..serverPrefix..serverAddress..":"..serverPort)
instance:start()

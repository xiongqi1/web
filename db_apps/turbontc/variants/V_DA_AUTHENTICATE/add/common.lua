-----------------------------------------------------------------------------------------------------------------------
-- Common mappings for OWA-NIT APIs
--
-- It must add an entry to the 'maps' table for each path.
-- This entry itself is a table that contains:
-- 1. for each HTTP method used a default HTTP status code and an optional Lua handler function
-- 2. A model that describes how to get/set each item in the matching schema
--
-- Copyright (C) 2019 NetComm Wireless Limited.
-----------------------------------------------------------------------------------------------------------------------

local ffi = require('ffi')
local lssl = ffi.load("ssl")
local DA = require("da")
require("luardb")

ffi.cdef [[
	typedef void X509_STORE_CTX;
	X509 *X509_STORE_CTX_get_current_cert(X509_STORE_CTX *ctx);
	X509_NAME *X509_get_issuer_name(const X509 *x);
	X509_NAME *X509_get_subject_name(const X509 *x);
	char * X509_NAME_oneline(const X509_NAME *a, char *buf, int size);

	int X509_NAME_get_index_by_NID(X509_NAME *name,int nid,int lastpos);
	X509_NAME_ENTRY *X509_NAME_get_entry(X509_NAME *name, int loc);
	ASN1_STRING * X509_NAME_ENTRY_get_data(const X509_NAME_ENTRY *ne);
	int ASN1_STRING_length(ASN1_STRING *x);
	const unsigned char * ASN1_STRING_get0_data(const ASN1_STRING *x);

	int setlogmask(int mask);
]]
local NID_organizationalUnitName = 18

-----------------------------------------------------------------------------------------------------------------------
-- Local functions
-----------------------------------------------------------------------------------------------------------------------

-- Extract DA from Organizational Units of X509Name
-- @param name An X509Name instance <cdata>
-- @param strict A boolean. If false, any strings not starting with a recognised
-- prefix will be ignored; otherwise, extraction fails.
-- @return A DA object extracted from name
local function DA_X509_extract(name, strict)
	local pos = -1
	local da = DA.new()
	while true do
		pos = lssl.X509_NAME_get_index_by_NID(name, NID_organizationalUnitName, pos)
		if pos < 0 then break end
		local entry = lssl.X509_NAME_get_entry(name, pos)
		local asn1str = lssl.X509_NAME_ENTRY_get_data(entry)
		local str = ffi.string(lssl.ASN1_STRING_get0_data(asn1str))
		logDebug('OU: ' .. str)
		local ret = da:parse_orgUnit(str, strict)
		if ret ~= 0 then
			logErr("Failed to parse OU '" .. str .. "': " .. DA.errorstr(ret))
		end
	end
	return da
end

-- activate/deactivate RDB service
-- @param enable A boolean to indicate activate or deactivate
-- @return true for success; nil or false for failure
local function activateRdb(enable)
	luardb.set('service.rdb_bridge.enable', enable and 1 or 0)
	return true
end

-- activate/deactivate firmware upgrade service
-- @param enable A boolean to indicate activate or deactivate
-- @return true for success; nil or false for failure
local function activateUpgrade(enable)
	luardb.set('service.upgrade.trigger', enable and 1 or 0)
	return true
end

-- activate/deactivate debug service
-- @param enable A boolean to indicate activate or deactivate
-- @return true for success; nil or false for failure
local function activateDebug(enable)
	local ssh_enabled = luardb.get('service.ssh.enable')
	luardb.set('service.ssh.enable', enable and 1 or 0)
	luardb.set('service.ssh.timer_trigger', 0)
	if ssh_enabled ~= '1' and enable then
		-- set up a one shot timer to disable ssh in 5 minutes
		luardb.set('service.ssh.timer_trigger', 30000) -- time in unit of 10 ms
		os.execute('one_shot_timer -t service.ssh.timer_trigger -o service.ssh.enable -w 0 -d')
	end
	if enable then
		-- set QxDM server IP
		luardb.set('service.qxdm_ethernet.server_ip', '169.254.251.2')
	end
	-- enable/disable QxDM over Ethernet
	luardb.set('service.qxdm_ethernet.enable', enable and 1 or 0)
	return true
end

-- map service to roles and activation function
-- if a client has a permit with any of the roles in the map, it will be able to
-- activate the corresponding service
local serviceMap = {
	rdb = {roles = {'install', 'support'}, func = activateRdb},
	upgrade = {roles = {'firmware', 'support'}, func = activateUpgrade},
	config = {roles = {'config', 'support'}, func = nil},
	-- TODO: implement different services for support and debug
	support = {roles = {'support', 'develop'}, func = activateDebug},
	debug = {roles = {'support', 'develop'}, func = activateDebug},
	-- TODO: more to be added
}

-- Load all serviceMap overwrites
--
-- This function loads every lua file in service dir and runs its init function.
-- @param serviceMap The base serviceMap table to be overwritten.
-- It will be the first argument when invoking each lua module's init function.
local function loadServiceOverwrite(serviceMap)
	local dirname = lua_object_dir.common .. '/service'
	logInfo('loading service overwrite from ' .. dirname)
	local attr, err = lfs.attributes(dirname)
	if attr and attr.mode == "directory" then
		for f in lfs.dir(dirname) do
			local module_name = f:match('^(.+)%.lua$')
			if module_name then
				local filename = dirname .. '/' .. f
				logInfo("Loading " .. filename)
				local res = package.loaded[module_name]
				if not res then
					local chunk, result = loadfile(filename)
					if not chunk then
						logErr('Error compiling ' .. filename .. ' result ' .. tostring(result))
					else
						result, res = pcall(chunk)
						if not result then
							logErr('Error loading ' .. filename .. ' result ' .. tostring(res))
							res = nil
						else
							package.loaded[module_name] = res
						end
					end
				else
					logInfo(module_name .. " is already loaded")
				end
				if res and res.init then
					local res, err = pcall(res.init, serviceMap)
					if not res then
						logErr('Failed to initialise %s, result %s', filename, err)
					end
				end
			end
		end
	end
end

loadServiceOverwrite(serviceMap)

-- authorize a service request
--
-- @param request An httpserver.HTTPRequest instance
-- @param service The service name to be requested
-- @return true if the request is granted; false, http_code otherwise
local function da_authorize_request(request, service)
	local entry = serviceMap[service]
	if not entry then
		logErr('invalid service ' .. tostring(service))
		return false, 400
	end

	local cert = request.connection.stream:get_peer_certificate()
	local subject = lssl.X509_get_subject_name(cert)
	local subjectStr = lssl.X509_NAME_oneline(subject, nil, 0)

	logDebug("<subject>: \n" .. ffi.string(subjectStr))
	ffi.C.free(subjectStr)

	local da = DA_X509_extract(subject, false)
	if da:validate(true) ~= DA.OK then
		logErr("client is not a valid DA leaf")
		return false, 401
	end

	local targetModel = luardb.get("system.product")
	local isEngVariant = targetModel:match("^%w+eng_%w+$")

	local owner = luardb.get("system.owner")
	if not owner or owner == "" then
		owner = "generic"
	end

	for _, role in ipairs(entry.roles) do
		local permitted = da:isPermitted(role, targetModel)
			or da:isPermitted(role, "odu")
			or da:isPermitted(role, owner .. "_odu")
			or (isEngVariant and da:isPermitted(role, "eng_odu"))
			or (isEngVariant and da:isPermitted(role, owner .. "_eng_odu"))

		if permitted then
			logNotice('accepted: client is granted role - ' .. role)
			return true
		end
	end

	logErr('rejected: client is refused role - ' ..  table.concat(entry.roles, ','))
	return false, 401
end

-- authorize a service request
--
-- @param self A RequestHandler instance
-- @param service The service name it requires
-- @return true if request is granted; false otherwise
local function da_authorize(self, service)
	local succ, code = da_authorize_request(self.request, service)
	if succ then
		return true
	end
	self:set_status(code)
	return false
end

-- activate a service per request
-- @param self A RequestHandler instance
local function activate(self)
	local service = self:get_argument("service")
	local enable = (self:get_argument("enable") == '1')

	logNotice((enable and '' or 'de') .. 'activating service ' .. tostring(service))
	if not da_authorize(self, service) then
		return
	end

	local entry = serviceMap[service]
	if not entry.func then
		logNotice('invalid to ' .. (enable and '' or 'de') .. 'activat service ' .. service)
		self:set_status(400)
	elseif entry.func(enable) then
		logNotice('successfully ' .. (enable and '' or 'de') .. 'activated service ' .. service)
	else
		logErr('Failed to ' .. (enable and '' or 'de') .. 'activate service ' .. service)
		self:set_status(500)
	end
end

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(maps, _, util, authorizer)
	local basepath = "/api/v1"
	maps[basepath.."/Hello"] = {
		get = {code = '200'},
		model = {
			Manufacturer = util.map_fix("NetComm Wireless Limited"),
			ManufacturerOUI = util.map_fs('r', '/sys/class/net/eth0/address', false, util.filt_pat("(%x%x):(%x%x):(%x%x).*", "%1%2%3")),
			ModelName = util.map_rdb('r', 'system.product'),
			GenericModel = util.map_rdb('r', 'system.product.generic'),
			Description = util.map_rdb('r', 'system.product.title'),
			ProductClass = util.map_rdb('r', 'system.product.class'),
			SerialNumber = util.map_rdb('r', 'system.product.sn'),
			HardwareVersion = util.map_rdb('r', 'system.product.hwver'),
			SoftwareVersion = util.map_rdb('r', 'sw.version'),
			SoftwareVersionBuildDate = util.map_rdb('r', 'sw.date'),
			UpTime = util.map_fs('r', '/proc/uptime', false, util.filt_pat("^(%d+)%..*")),
			MacAddress = util.map_fs('r', '/sys/class/net/eth0/address'),
			Capabilities = util.map_rdb('r', 'system.capabilities'),
		}
	}

	maps[basepath .. "/Activate"] = {
		put = {
			code = '200',
			trigger = function () return activate end
		},
	}

	authorizer.da_authorize = da_authorize

	-- turbo sets the logmask to LOG_INFO or higher
	-- reset to allow all levels
	ffi.C.setlogmask(255)
	logDebug("log mask set to 255")
end

-- this will be hooked up into tls handshake
function module.verify_callback(preverify_ok, x509_ctx)
	if preverify_ok == 0 then
		return 0
	end

	local cert = lssl.X509_STORE_CTX_get_current_cert(x509_ctx)
	local subject = lssl.X509_get_subject_name(cert)
	local issuer = lssl.X509_get_issuer_name(cert)
	local subjectStr = lssl.X509_NAME_oneline(subject, nil, 0)
	local issuerStr = lssl.X509_NAME_oneline(issuer, nil, 0)

	logDebug("[issuer]: \n" .. tostring(issuerStr))
	logDebug("[subject]: \n" .. tostring(subjectStr))
	ffi.C.free(issuerStr)
	ffi.C.free(subjectStr)

	local daSubject = DA_X509_extract(subject, false)
	local daIssuer = DA_X509_extract(issuer, false)

	logDebug("validating DA rules")
	local ret = daSubject:validate(false)
	if ret ~= DA.OK then
		logErr("failed to validate DA subject: " .. DA.errorstr(ret))
		return 0
	end

	ret = daIssuer:validate(false)
	if ret ~= DA.OK then
		logErr("failed to validate DA issuer: " .. DA.errorstr(ret))
		return 0
	end

	ret = daIssuer:validateChild(daSubject)
	if ret ~= DA.OK then
		logErr("subject is not a child of issuer: " .. DA.errorstr(ret))
		return 0
	end

	logDebug("validation succeeded")
	return 1
end

-- exposed to other modules
module.da_authorize = da_authorize
module.da_authorize_request = da_authorize_request

return module

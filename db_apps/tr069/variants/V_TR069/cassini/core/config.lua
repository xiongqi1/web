local v_vars = require('variants')
local topRoot
if not v_vars.V_TR181 or v_vars.V_TR181 == '0' then
    topRoot = 'InternetGatewayDevice'
else
    topRoot = 'Device'
end

-- Prohibit profile2 (IMS) and profile3(SOS) setting except for APN name
local lockImsSosProfile = false
if v_vars.V_LOCK_IMS_SOS_PROFILE == 'y' then
	lockImsSosProfile = true
end

-- Prefix of vendor extended parameter name.
xVendorPrefix = "X_CASASYSTEMS"
if v_vars.V_TR069_XVENDOR and v_vars.V_TR069_XVENDOR ~= "" then
	xVendorPrefix = v_vars.V_TR069_XVENDOR
end

-- Bulk data collection(TR-157)
local enabledBulkDataCollection = false
if v_vars.V_TR069_BULK_DATA_COLLECTION == 'y' then
	enabledBulkDataCollection = true
end

-- TR069 client reports reboot reason with "1 BOOT" inform session.
local enableRebootReasonReport = false
if v_vars.V_TR069_REBOOT_REASON == 'y' then
	enableRebootReasonReport = true
end

-- Enable binding tr-069 network traffice to specific APN
local enableManagementAPN = false
if v_vars.V_TR069_MANAGEMENT_APN == 'y' then
	enableManagementAPN = true
end

-- Spport reverting band setting with "Normal" BandLockingType.
local enableRevertBand = false
if v_vars.V_CUSTOM_FEATURE_PACK == "bellca" then
	enableRevertBand = true
end

local conf = {
	----
	-- TR-069 Core Configuration
	----
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc/tr-069.conf',
		syncScript = 'killall -HUP rdb_manager',
		randomSource = '/dev/urandom',
	},
	transfer = {
		dir = '/usrdata/cache',
		curlBinary = 'curl',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					['star'] = {
						file = 'upgrade.star', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1',
					},
				},
				['2 Web Content'] = {
					['zip'] = {
						file = 'tr069-temp.zip', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = '/usr/lib/tr-069/scripts/tr069_importConfFile.sh', reboot = true, keeptempfile = '0',
					},
				},
				['3 Vendor Configuration File'] = {
					['zip'] = {
						file = 'tr069-temp.zip', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = '/usr/lib/tr-069/scripts/tr069_importConfFile.sh', reboot = true, keeptempfile = '0',
					},
				},
			},
			upload = {
				['1 Vendor Configuration File'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua CfgFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportConfFile.sh',
				},
				['2 Vendor Log File'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua LogFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportLogFile.sh',
				},
				['3 Vendor Configuration File 1'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua CfgFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportConfFile.sh',
				},
				['4 Vendor Log File 1'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua LogFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportLogFile.sh',
				},
			},
		},
		supportedTransports = {
			'https',
			'http',
			'ftp',
		}
	},
	gc = {
		pause = 50,
		stepmul = 200,
	},
	net = {
		httpDebug = false,
		keep_ssl_session= false,
		connectionRequestPort = 7547,
		connectionTimeout = 30, -- set with minimum value on the standard
		ioTimeout = 60, -- set with minimum value on the standard
		https_only=false,
		ca_path = '/etc/ssl/certs', -- path for CA certificates
		ssl_verify_host = true,
		ssl_version = 6, -- TLS v1.2 or later; Reference: cwmpd/src/classes/HTTP/Client.lua
		download_redirects = false, -- Enable following HTTP 302 redirects on "Download" method
	},
	cwmp = {
		defaultVersion = '1.1',
		defaultBackoffM = 5,
		defaultBackoffK = 2000,
		defaultPeriodicInterval = 60,
		defaultPeriodicPhase = 0,
	},
	log = {
		debug = false,
		level = 'info',
		stdoutLevel = 'error',
	},
	rdb = {
		enable = 'service.tr069.enable',
		pause = 'service.tr069.pause',
		status = 'service.tr069.status',
		version = 'tr069.version',
		bootstrap = 'tr069.bootstrap',
		eventPrefix = 'tr069.event',
		transferPrefix = 'tr069.transfer',
		requestPrefix = 'tr069.request',
		tr143Prefix = 'tr069.diagnostics',
		deviceResetReason = 'service.system.reset_reason',
		deviceReset = 'service.system.reset',
		factoryReset = 'tr069.factory.trigger',
		serivcePulse = 'tr069.running.pulse',
	},

	----
	-- Platform/Application Specific Configuration
	----
	-- Prohibit profile2 (IMS) and profile3(SOS) setting except for APN name
	lockImsSosProfile = lockImsSosProfile,

	-- supplemental services
	reportInformStatus = true,

	-- top level root object
	topRoot = topRoot,

	-- Prefix of vendor extended parameter name.
	xVendorPrefix = xVendorPrefix,

	-- Bulk data collection(TR-157)
	enabledBulkDataCollection = enabledBulkDataCollection,

	-- Reboot reason report with "1 BOOT" inform session
	enableRebootReasonReport = enableRebootReasonReport,

	-- Enable binding tr-069 network traffice to specific APN
	enableManagementAPN = enableManagementAPN,

	numOfApnProfiles = 6,

	-- Spport reverting band setting with "Normal" BandLockingType.
	enableRevertBand = enableRevertBand,
}

-- Reboot reason code list.
-- The list of "service.tr069.lastreboot" value and code pairs
if enableRebootReasonReport then
    conf.rebootReasonCodeList = {
        ["ACS System Upgrade"] = "0001",
        ["Acs Request"] = "0002",
        ["Factory Reset"] = "0003",
        ["Webui Request"] = "0004",
        ["Warm-restart"] = "0005",
        ["Power loss"] = "0006",
        ["Firmware Upgrade"] = "0007",
        ["_unknown"] = "9999", -- this is for internal use.
    }
end


---- The following implements conf overwrites mechanism where conf is
---- overwritten by RDBs if the respective RDBs exist and contain valid values.

--[[
	RDB overwrites table
	It must have the same hierachy as the items to be overwritten in conf.
	If an item does not need to be overwritten, it should not present here.
	The whole table can be absent if nothing is to be overwritten.
	There is no limitation on the depth of the item to be overwritten (at which
	level the item resides) as long as the same hierachy is used.
	Note: The RDB keys must start with 'tr069.conf.' prefix and followed by all
	the intermediate and item keys of the corresponding conf items.
	e.g. The RDB to overwrite conf.net.ioTimeout must be named:
	tr069.conf.net.ioTimeout (case sensitive).

	Currently, only items that are likely to be customised are included, while
	this table can be augmented if needed in the future.
--]]
local rdbOverwrites = {
	-- intermediate key
	net = {
		-- <item key in conf> = <value type>
		httpDebug = 'boolean',
		keep_ssl_session = 'boolean',
		ssl_verify_host = 'boolean',
		ssl_verify_peer = 'boolean',
		ssl_version = 'number',
		ca_cert = 'string',
		ca_path = 'string',
		ssl_cipher_list = 'string',
		connectionTimeout = 'number',
		ioTimeout = 'number',
		download_redirects =  'boolean',
	},
	log = {
		debug = 'boolean',
		level = 'string',
		stdoutLevel = 'string',
	},
}

--[[
	Overwrite a single conf item from an RDB variable

	@param dict The conf sub-dict that directly contains the item of interest
	@param path An array that lists keys that lead to this dict
	@param key The key that identifies the item of interest
	@param rdbType The type of the RDB variable

	e.g. To overwrite conf.log.debug, the argument should be:
	dict = conf.log, path = {'log'}, key = 'debug', rdbType = 'boolean'
	To overwrite conf.reportInformStatus, the argument should be:
	dict = conf, path = {}, key = 'reportInformStatus', rdbType = 'boolean'
--]]
require('luardb')
local function overwrite(dict, path, key, rdbType)
	local rdbKey = 'tr069.conf.' -- all overwrite RDBs are under this prefix
	if #path == 0 then
		rdbKey = rdbKey .. key
	else
		rdbKey = rdbKey .. table.concat(path, '.') .. '.' .. key
	end
	local rdbVal = luardb.get(rdbKey)
	if rdbType == 'boolean' then
		if rdbVal == '0' or rdbVal == '1' then
			dict[key] = (rdbVal == '1')
		end
	elseif rdbType == 'number' then
		rdbVal = tonumber(rdbVal)
		if rdbVal then
			dict[key] = rdbVal
		end
	elseif rdbType == 'string' then
		if rdbVal and rdbVal ~= '' then
			dict[key] = rdbVal
		end
	else
		error('Illegal rdb type: ' .. rdbType)
	end
end

--[[
	Recursive overwrite confDict from rdbDict

	@param confDict A dict that is a subtree of conf
	@param rdbDict A dict that is a subtree of rdbOverwrites
	@param path An array that lists keys that lead to the above dicts

	e.g. For the top level call, the argument should be:
	confDict = conf, rdbDict = rdbOverwrites, path = {}
	For conf.log level overwrite, the argument should be:
	confDict = conf.log, rdbDict = rdbOverwrites.log, path = {'log'}
--]]
local function recurse(confDict, rdbDict, path)
	for key, val in pairs(rdbDict) do
		if type(val) == 'table' then
			if confDict[key] == nil then
				confDict[key] = {}
			end
			if type(confDict[key]) ~= 'table' then
				error('conf.' .. key .. ' must be a table')
			end
			path[#path + 1] = key
			recurse(confDict[key], val, path)
			path[#path] = nil
		else
			-- key = rdb key; val = rdb type
			overwrite(confDict, path, key, val)
		end
	end
end

-- overwrite conf with RDBs
local function confOverwrite()
	-- nothing to overwrite if rdbOverwrites is not a table (including nil)
	if type(rdbOverwrites) ~= 'table' then
		return
	end
	recurse(conf, rdbOverwrites, {})
end

confOverwrite()

return conf

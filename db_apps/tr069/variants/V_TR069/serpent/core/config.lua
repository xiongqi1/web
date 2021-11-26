local function cdiReboot (transfer)
	if transfer.url then
		local ret = string.match(transfer.url, '.*r%.cdi%s*$')
		if not ret then
			return true
		else
			return false
		end
	end
end

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
		dir = '/opt/cdcs/upload',
		-- wgetBinary = 'wget',
		curlBinary = 'curl',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					['cdi'] = {
						file = 'tr069-temp.cdi', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1', rebootHandler=cdiReboot,
					},
					['zip'] = {
						file = 'tr069-temp.zip', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1',
					},
					['star'] = {
						file = 'tr069-temp.star', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1',
					},
					['conf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['crt'] = { -- this only supports "server.crt"
						file = nil, script = 'install_file', reboot = false,
					},
					['pem'] = { -- CA certificate
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['nvu'] = {
						file = 'tr069-temp.nvu', script = 'true', reboot = false, keeptempfile = '1',
					},
					['spk'] = {
						file = 'tr069-temp.spk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = true, keeptempfile = '0',
					},
					['ipk'] = {
						file = 'tr069-temp.ipk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['key'] = { -- this only supports "server.key"
						file = nil, script = 'install_file', reboot = false,
					},
					['pdf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['sb'] = {
						file = 'uboot.sb', script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['bin'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['usf'] = {
						file = 'tr069-temp.usf', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['sfp'] = {
						file = 'tr069-temp.sfp', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['xml'] = {
						file = 'apnList.xml', script = 'install_file', reboot = false,
					},
					['pub'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['img'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
				},
				['2 Web Content'] = {
					['cdi'] = {
						file = 'tr069-temp.cdi', script = 'install_file', reboot = true, keeptempfile = '1', rebootHandler=cdiReboot,
					},
					['conf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['crt'] = { -- this only supports "server.crt"
						file = nil, script = 'install_file', reboot = false,
					},
					['pem'] = { -- CA certificate
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['nvu'] = {
						file = 'tr069-temp.nvu', script = 'true', reboot = false, keeptempfile = '1',
					},
					['spk'] = {
						file = 'tr069-temp.spk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = true, keeptempfile = '0',
					},
					['ipk'] = {
						file = 'tr069-temp.ipk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['key'] = { -- this only supports "server.key"
						file = nil, script = 'install_file', reboot = false,
					},
					['pdf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['sb'] = {
						file = 'uboot.sb', script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['bin'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['usf'] = {
						file = 'tr069-temp.usf', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['sfp'] = {
						file = 'tr069-temp.sfp', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['xml'] = {
						file = 'apnList.xml', script = 'install_file', reboot = false,
					},
					['pub'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['img'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
				},
				['3 Vendor Configuration File'] = {
					['cdi'] = {
						file = 'tr069-temp.cdi', script = 'install_file', reboot = true, keeptempfile = '1', rebootHandler=cdiReboot,
					},
					['conf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['crt'] = { -- this only supports "server.crt"
						file = nil, script = 'install_file', reboot = false,
					},
					['pem'] = { -- CA certificate
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['nvu'] = {
						file = 'tr069-temp.nvu', script = 'true', reboot = false, keeptempfile = '1',
					},
					['spk'] = {
						file = 'tr069-temp.spk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = true, keeptempfile = '0',
					},
					['ipk'] = {
						file = 'tr069-temp.ipk', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['key'] = { -- this only supports "server.key"
						file = nil, script = 'install_file', reboot = false,
					},
					['pdf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['sb'] = {
						file = 'uboot.sb', script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['bin'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
					['usf'] = {
						file = 'tr069-temp.usf', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['sfp'] = {
						file = 'tr069-temp.sfp', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['xml'] = {
						file = 'apnList.xml', script = 'install_file', reboot = false,
					},
					['pub'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['img'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
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
		factoryReset = 'tr-069.factory.trigger',
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
}


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

	21/4/21 new json config type that can overwrite/add multi-level table items.
	eg. with tr069.conf.transfer.types.download value of:
	{"3 Vendor Configuration File":{"star":{"script":"apply_rtconf.sh","keeptempfile":"1"}}}
	will overwrite script & keeptempfile if they exist in the config table or add if not.

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
	},
	log = {
		debug = 'boolean',
		level = 'string',
		stdoutLevel = 'string',
	},
	rdb = {
		enable = 'string',
	},
	transfer = {
		types = {
			download = 'json',
		},
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
	elseif rdbType == 'json' then
		if rdbVal and rdbVal ~= '' then
			local json = require('turbo.3rdparty.JSON')
			local table = json:decode(rdbVal)
			if type(table) == 'table' then
				local util = require('turbo.util')
				util.tablemerge(dict[key], table)
			end
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

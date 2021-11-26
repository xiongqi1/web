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

return {
	----
	-- TR-069 Core Configuration
	----
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc/tr-069.conf',
		syncScript = 'killall -HUP rdb_manager',
		randomSource = '/dev/urandom',
	},
	----
	-- @TODO - Has to be changed to upgrade based on banks
	----
	transfer = {
		dir = '/opt/cdcs/upload',
		curlBinary = 'curl',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					['img'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '1',
					},
				},
			},
			upload = {
			},
		},
		supportedTransports = {
			'https',
		}
	},
	gc = {
		pause = 50,
		stepmul = 200,
	},
	net = {
		httpDebug = false,
                keep_ssl_session= true,
		connectionRequestPort = 7547,
		connectionTimeout = 60,
		ioTimeout = 180,
                ssl_version = 1, -- TLSv1
                ca_cert = '/etc/ssl/certs/ca-certificates.crt',
                --ssl_cipher_list = 'DES-CBC3-SHA',
                client_cert = '/etc/ssl/certs/client.crt',
                client_key = '/etc/ssl/certs/client.key',
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
		downloadMaxRequests = 'tr069.download.maxrequests',
		downloadTimeout = 'tr069.download.timeout',
	},

	----
	-- Platform/Application Specific Configuration
	----

	-- supplemental services
	reportInformStatus = true,

	-- top level root object
	topRoot = topRoot,

}

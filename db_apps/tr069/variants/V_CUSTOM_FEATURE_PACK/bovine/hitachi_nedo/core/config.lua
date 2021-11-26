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
	transfer = {
		dir = '/opt/cdcs/upload',
		wgetBinary = 'wget',
		curlBinary = 'curl',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					['cdi'] = {
						file = 'tr069-temp.cdi', script = 'foreground=1 install_file', reboot = true, keeptempfile = '1', rebootHandler=cdiReboot,
					},
					['conf'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['crt'] = { -- this only supports "server.crt"
						file = nil, script = 'install_file', reboot = false,
					},
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = false, keeptempfile = '0',
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
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = false, keeptempfile = '0',
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
					['csv'] = {
						file = nil, script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['cwe'] = {
						file = 'tr069-temp.cwe', script = 'install_file', reboot = false, keeptempfile = '0',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = false, keeptempfile = '0',
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
					['properties'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['NOEXTENSION'] = {
						file = nil, script = 'install_file', reboot = false,
					},
					['CBQ'] = {
						file = nil, script = 'install_file', reboot = false,
					},
				},
			},
			upload = {
				['1 Vendor Configuration File'] = {
					variableFileName = true,
--					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getConfFileName.lua',
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua CfgFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportConfFile.sh',
				},
				['2 Vendor Log File'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua LogFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportLogFile.sh',
					matchInUrl = {
						XEMScore='/usr/lib/tr-069/scripts/tr069_exportXEMSLogFile.sh',
						XEMSapp='/usr/lib/tr-069/scripts/tr069_exportXEMSLogFile.sh',
					}
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
		connectionTimeout = 60,
		ioTimeout = 180,
		https_only=false,
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
		deviceResetReason = 'service.system.reset_reason',
		deviceReset = 'service.system.reset',
		factoryReset = 'tr-069.factory.trigger',
		serivcePulse= 'tr069.running.pulse',
	},

	----
	-- Platform/Application Specific Configuration
	----

	-- supplemental services
	reportInformStatus = true,

}

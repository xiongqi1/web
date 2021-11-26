require('luardb')

local unids = 4
if luardb.get('link.profile.0.enable') == '1' and string.sub(luardb.get('link.profile.0.dev') or '', 0, 4) == 'eth0' then unids = 3 end
--[[
 return status of firmware installation procedure.
 - "success" ==> completed installation procedure with success.
 - "failure" ==> completed installation procedure with failure.
 - "inprogress" ==> In process
--]]
local function fw_dload_status_cb ()
	local status = luardb.get("service.updating_software.updating_status")

	if status == "done" then
		return "success"
	elseif status == "fail" then
		return "failure"
	else
		return "inprogress"
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
		systemLog = '/opt/messages.log',
	},
	transfer = {
		dir = '/cache/upload',
		--wgetBinary = '/usr/bin/wget',
		curlBinary = 'curl',
		prepareCommand = 'rdb_set service.updating_software.command prepare; rdb_wait service.updating_software.command_status',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					-- file: archived filename on transfer.dir
					-- script: CLI command to install archived file.
					-- reboot: boolean
					--         true  => If completing "script" command with success,
					--               transfer state machine resets the system.
					--               The success completion report is transfered after system reboot.
					--               If completing "script" command with failure,
					--               the state machine reports failure completion
					--               and terminate download precedure.
					--         false => Transfer state machine report completion to ACS
					--               after completing "script" command without system reboot.
					-- rebootWaitCb: callback function pointer.
					--         "reboot" flag should be true to use this callback.
					--         But transfer state machine does not trigger system reboot.
					--         It just polls rebootWaitCb to get the status of "script" command.
					--         And "script" command handles system reboot.
					--         rebootWaitCb has three return status.
					--         1. "success" ==> Transfer state machine reports success and terminates file download process.
					--         2. "failure" ==> Transfer state machine reports failure and terminates file download process.
					--         3. "inprogress" ==> Transfer state machine keeps polling rebootWaitCb.
					['star'] = {
						file = 'tr069-firmware.star',
						script = 'foreground=1 install_file',
						reboot = true,
						rebootWaitCb = fw_dload_status_cb,
						keeptempfile = '1',
					},
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = true, keeptempfile = '0',
					},
				},
				['3 Vendor Configuration File'] = {
					['gz'] = {
						file = 'tr069-temp.cfg.tar.gz', script = 'install_file', reboot = true, keeptempfile = '0',
					},
				},
			},
			upload = {
				['1 Vendor Configuration File'] = {
					variableFileName = true,
					getFileNamescript = '/usr/lib/tr-069/scripts/tr069_getUploadFileName.lua CfgFile',
					script = '/usr/lib/tr-069/scripts/tr069_exportConfFile.sh',
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
		keep_ssl_session= true,
		connectionRequestPort = 8082,
		connectionTimeout = 30, -- set with minimum value on the standard
		ioTimeout = 60, -- set with minimum value on the standard
		ssl_version = 1, -- TLSv1
		ca_cert = '/etc/ssl/certs/ca-certificates.crt',
		ssl_cipher_list = 'DES-CBC3-SHA',
		rate_limit = '2048k',
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
		stdoutLevel = 'critical',
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
		factoryReset = 'service.system.factory',
		serivcePulse= 'tr069.running.pulse',

	},

	----
	-- Platform/Application Specific Configuration
	----
	wntd = {
		debug = 0,
		unidCount = unids,
		unidPrefix = 'unid',
		avcPrefix = 'avc',
		alarmClass = 'alarms',
		alarmdEndpoint = 'alarmd',
		dot1agPrefix='dot1ag',
		y1731Prefix='y1731',
		dot1ag_max_session=4,
		switchIface = 'eth0',
		grePrefix = 'gre',
		bridgePrefix = 'br',
		tr143Prefix = 'diagnostics',
		avc_dot1ag_sync = 'dot1ag.mda.changedif',
		wanProfile = '2',		-- profile 2:  access-seeker
		switchConfig = 'eth-reg-set eth0.4000 -a -',
		switchLinkStatus = 'eth-reg-set eth0.4000 -b -r 1 -t 1 -a get 0x84 4',
		prefixSetSize = 'andor 0x78 0x3fff ',
		set_unid = '/usr/bin/set_unid.sh',
		mibDaemon = '/usr/lib/tr-069/scripts/mibd.lua',
		tc1_cir= 150000,
		tc2_pir = 10000000,
		tc4_pir= 1000000,
	},
	stats = {
		mibListener = 'eth-reg-set eth0.4000 -t 0 -r 0 -a mib',
		unidMibPollInterval = 5,
		dot1agPollInterval = 30,
		rfStatsPollInterval = 30,
		unidSwitchStatsPrefix  = 'swStats',
		avcQdiscStatsPrefix  = 'qdStats',
		trunkStatsPrefix  = 'switch.trunk',
		cpuStatsPrefix  = 'switch.cpu',
		avcIfGreStatsPrefix = 'greStats',
		avcIfSwitchStatsPrefix = 'swStats',
		statsmonitoringPrefix= 'statsmonitoring',
		statsmonitoringSamplePrefix= 'statsmonitoring.sample',
		statsMonitoringConfigurationPrefix= 'statsmonitoring.configuration',
		statsmonitoringAvcPrefix= 'statsmonitoring.sample.avc',
		statsmonitoringUnidPrefix= 'statsmonitoring.sample.unid',
		statsmonitoringfile ="/opt/statsmonitoring.out",

		fileRollBaseName = "/opt/stats/file-roll-stats.",
		fileRollPeriod = 60 * 60,
		fileRollCount  = 25,
		fileRollPort  = 2220,
		fileRollSize  = 512000,
	},

	-- top level root object
	topRoot = 'InternetGatewayDevice',

}

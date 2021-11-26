require('luardb')

local unids = 4
if luardb.get('link.profile.0.enable') == '1' and string.sub(luardb.get('link.profile.0.dev') or '', 0, 4) == 'eth0' then unids = 3 end

return {
	----
	-- TR-069 Core Configuration
	----
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc/tr-069.conf',
		syncScript = 'killall -HUP rdb_manager',
		randomSource = '/dev/urandom',
		systemLog = '/NAND/messages.log',
	},
	transfer = {
		dir = '/NAND/tr-069',

		--wgetBinary = '/usr/bin/wget',
		curlBinary = '/bin/curl',
		types = {
			download = {
				['1 Firmware Upgrade Image'] = {
					['tgz'] = {
						file = 'tr069-firmware.tgz',
						script = '/usr/bin/updnand.sh',
						reboot = true,
					},
					['cdi'] = {
						file = 'tr069-firmware.cdi',
						script = '/usr/bin/updnand.sh',
						reboot = true,
					},
					['gz'] = {
						file = 'tr069-firmware.tgz',
						script = '/usr/bin/updnand.sh',
						reboot = true,
					},
				},
			},
		},
	},
	gc = {
		pause = 50,
		stepmul = 200,
	},
	net = {
		httpDebug = false,
		keep_ssl_session= true,
		connectionRequestPort = 8082,
		connectionTimeout = 60,
		ioTimeout = 180,
		https_only=true,
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
		switchIface = 'eth0',
		grePrefix = 'gre',
		bridgePrefix = 'br',
		tr143Prefix = 'diagnostics',
		wanProfile = '2',
		switchConfig = 'eth-reg-set eth0.4000 -a -',
		switchLinkStatus = 'eth-reg-set eth0.4000 -b -r 1 -t 1 -a get 0x84 4',
		prefixSetSize = 'andor 0x78 0x3fff ',
		set_unid = '/usr/bin/set_unid.sh',
		mibDaemon = '/usr/lib/tr-069/scripts/mibd.lua',
		tc1_cir= 150000,
		tc4_pir= 1000000,
	},
	stats = {
		mibListener = 'eth-reg-set eth0.4000 -t 0 -r 0 -a mib',
		unidMibPollInterval = 30,
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
		statsmonitoringfile ="/NAND/statsmonitoring.out",

		fileRollBaseName = "/NAND/stats/file-roll-stats.",
		fileRollPeriod = 60 * 60,
		fileRollCount  = 25,
		fileRollPort  = 2220,
		fileRollSize  = 512000,
	}
}

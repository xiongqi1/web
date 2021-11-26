require('luardb')

local unids = 4
if luardb.get('link.profile.0.enable') == '1' and luardb.get('link.profile.0.dev') == 'eth0.4' then unids = 3 end

return {
	----
	-- TR-069 Core Configuration
	----
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc/tr-069.conf',
		generatedConfig = '/etc/dimclient.conf',
		transferDir = '/NAND/tr-069/',
		firmwareFile = '/NAND/tr-069/firmware.tgz',
		upgradeScript = '/usr/bin/updnand.sh',
	},
	rdb = {
		informTrigger = 'service.tr069.trigger.inform',
		eventPrefix = 'tr069.event',
		transferPrefix = 'tr069.transfer',
		eventBootstrap = 'tr069.event.bootstrap',
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
		switchIface = 'eth0',
		grePrefix = 'gre',
		bridgePrefix = 'br',
		tr143Prefix = 'diagnostics',
		wanProfile = '2',
		switchConfig = 'eth-reg-set eth0.4000 -a -',
		switchLinkStatus = 'eth-reg-set eth0.4000 -b -r 1 -t 1 -a get 0x84 4',
		set_unid = '/usr/bin/set_unid.sh',
		mibDaemon = '/usr/lib/tr-069/scripts/mibd.lua',
		tc1_rate = 150000,
		tc4_rate = 1000000,
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
		fileRollBaseName = "/NAND/stats/file-roll-stats.",
		fileRollPeriod = 60 * 60,
		fileRollCount  = 25,
		fileRollPort  = 2220,
	}
}

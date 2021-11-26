require('luardb')

local unids = 4
if luardb.get('link.profile.0.enable') == '1' and luardb.get('link.profile.0.dev') == 'eth0.4' then unids = 3 end

return {
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc/tr-069.conf',
		generatedConfig = '/etc/dimclient.conf',
		transferDir = '/tmp/tr-069/',
		firmwareFile = '/tmp/tr-069/firmware.img',
		upgradeScript = '/usr/bin/updgrade.sh',
		set_unid = '/usr/bin/set_unid.sh',
		mibDaemon = '/usr/lib/tr-069/scripts/mibd.lua',
	},
	rdb = {
		informTrigger = 'service.tr069.trigger.inform',
		eventPrefix = 'tr069.event',
		transferPrefix = 'tr069.transfer',
		eventBootstrap = 'tr069.event.bootstrap',
	},
	wntd = {
		unidCount = unids,
		unidPrefix = 'unid',
		avcPrefix = 'avc',
		debug = 0,
		switchIface = 'eth0', 
		grePrefix = 'gre',
		bridgePrefix = 'br',
		wanProfile = '2',
		switchConfig = 'eth-reg-set eth0.4000 -a -',
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
		fileRollBaseName = "/tmp/file-roll-stats.",
		fileRollPeriod = 60 * 60,
		fileRollCount  = 25,
		fileRollPort  = 2220,
	}
}

return {
	fs = {
		code = '/usr/lib/tr-069',
		config = '/etc_ro/tr-069.conf',
		generatedConfig = '/var/tr069/dimclient.conf',
		transferDir = '/opt/cdcs/upload/',
		firmwareFile = '/opt/cdcs/upload/tr069-firmware.cdi',
		upgradeScript = '/usr/sbin/flashtool'
	},
	rdb = {
		informTrigger = 'service.tr069.trigger.inform',
		eventPrefix = 'tr069.event',
		transferPrefix = 'tr069.transfer',
		eventBootstrap = 'tr069.event.bootstrap',
	},
	nvram = {
	},
	platypus = {
	}
}

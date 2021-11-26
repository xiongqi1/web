return {
	name = 'tr069',
	description = 'The tr069 client subsystem daemon manager.',
	type = 'rdbmanaged',

	cmd = '/usr/bin/dimclient',
	args = {},
	env = {
		PATH = '/bin:/sbin:/usr/sbin:/usr/bin'
	},
	stdin = '/dev/null',
	stdout = '/NAND/tr-069/dimclient.out',
	stderr = '/NAND/tr-069/dimclient.err',
	
	respawn = true,
	rdbEnable = 'service.tr069.enable',
	rdbStatus = 'service.tr069.status',

}

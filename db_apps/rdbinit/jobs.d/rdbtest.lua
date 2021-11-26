local function _generateConfig(self)
	luasyslog.log('info', 'test job making its config')
end

return {
	name = 'rdbtest',
	description = 'Test job for rdbmanaged type jobs.',
	type = 'rdbmanaged',

	cmd = '/bin/sleep',
	args = { '1000' },
	env = {
		PATH = '/bin:/sbin:/usr/sbin:/usr/bin',
	},
	stdin = '/dev/null',
	stdout = '/dev/null',
	stderr = '/dev/null',
	
	respawn = true,
	rdbEnable = 'test.enable',
	rdbStatus = 'test.status',
	rdbConfig = {
		'test.param1',
		'test.param2',
	},
	cfgFile = '/tmp/test.cfg',
	config = _generateConfig,
}

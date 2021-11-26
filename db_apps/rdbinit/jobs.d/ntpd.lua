local function _generateConfig(self)
	local cfg, err = io.open(self.cfgFile, 'w')
	if not cfg then error('Error opening config file: ' .. err) end
	cfg:write('server 127.127.1.0\n')
	cfg:write('fudge 127.127.1.0 stratum 12\n')
	cfg:write('restrict default noquery nomodify\n')
	cfg:write('restrict 127.0.0.1\n')
	for i = 1,5 do
		local server = rdb.get('service.ntp.server_' .. i)
		if server and server:len() > 0 then
			cfg:write('server ' .. server .. '\n')
		end
	end
	cfg:close()
end

return {
	name = 'ntpd',
	description = 'The ntp daemon subsystem manager.',
	type = 'rdbmanaged',

	cmd = '/bin/ntpd',
	args = { '-g', '-c', CONFIG_FILE },
	env = {
		PATH = '/bin:/sbin:/usr/sbin:/usr/bin',
		LD_PRELOAD = '/lib/libgcc_s.so.1'
	},
	stdin = '/dev/null',
	stdout = '/dev/null',
	stderr = '/dev/null',
	
	respawn = true,
	rdbEnable = 'service.ntp.enable',
	rdbStatus = 'service.ntp.status',
	rdbConfig = {
		'service.ntp.server_1',
		'service.ntp.server_2',
		'service.ntp.server_3',
		'service.ntp.server_4',
		'service.ntp.server_5'
	},
	cfgFile = '/etc/ntp.conf',
	config = _generateConfig,
}

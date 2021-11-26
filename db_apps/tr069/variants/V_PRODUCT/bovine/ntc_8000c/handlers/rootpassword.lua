require('Daemon')
require('Logger')

Logger.addSubsystem('RootPassword')
local subROOT = conf.topRoot .. '.X_NETCOMM.'

return {
	[subROOT .. 'PasswordManagement.UserRootPass'] = {
		set = function(node, name, value)
			Logger.log('RootPassword', 'info', 'password has been set by TR-069')
			luardb.set("telnet.passwd.new", value)
			luardb.set("telnet.passwd.trigger", '1')
			return 0
		end
	}
}

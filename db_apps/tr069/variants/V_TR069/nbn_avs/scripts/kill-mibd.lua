#!/usr/bin/env lua
require('luasyslog')
require('stringutil')
require('tableutil')

-- setup for our local includes
package.path = debug.getinfo(1, "S").source:match[[^@?(.*[\/])[^\/]-$]] .."?.lua;".. package.path 

-- our config file is shared with the TR-069 hostbridge
conf = dofile('/usr/lib/tr-069/config.lua')

-- setup syslog facility
luasyslog.open('mib-vars', 'LOG_DAEMON')

if not conf.stats.mibListener then
   luasyslog.log('LOG_INFO', 'Can\'t kill  mibd.lua because conf.stats.mibListener not set')
else
  local cmd = 'pkill -f "'..conf.stats.mibListener..'"'
  luasyslog.log('LOG_INFO', 'About to '..cmd)
  os.execute(cmd)
end


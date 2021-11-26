#!/usr/bin/env lua
----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----

-- we source our basic layout info from /usr/lib/tr-069/config.lua
conf = dofile('/usr/lib/tr-069/config.lua')

if conf.gc.pause then collectgarbage('setpause', conf.gc.pause) end
if conf.gc.stepmul then collectgarbage('setstepmul', conf.gc.stepmul) end

-- file/class loading paths
package.path = conf.fs.code .. '/?.lua;' .. conf.fs.code .. '/classes/?/?.lua;' .. conf.fs.code .. '/classes/?.lua;' .. conf.fs.code .. '/utils/?.lua;' .. package.path
package.cpath = conf.fs.code .. '/?.so;' .. conf.fs.code .. '/utils/?.so;' .. package.cpath

-- Logging system init
require('Logger')
require('luasyslog')
luasyslog.open('cwmp-xfrd', 'LOG_DAEMON')
local oldSink = Logger.sink
Logger.sink = function(subsystem, level, msg)
	if Logger.compareLevels(level, conf.log.stdoutLevel) >= 0 or Logger.debug then
		oldSink(subsystem, level, msg)
	end
	luasyslog.log(level, subsystem .. ': ' .. msg)
end
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem('cwmp-xfrd')

require('Daemon.Host')
require('Transfer.Daemon')

function runIt()
	host = Daemon.Host.new()
	daemon = Transfer.Daemon.new(host)
	daemon:init()
	while daemon.running do
		daemon:poll()
	end
end

-- daemon logic
Logger.log('cwmp-xfrd', 'notice', 'Transfer daemon start-up.')

local ret, msg = xpcall(runIt, debug.traceback)
if not ret then
	Logger.log('cwmp-xfrd', 'error', 'Client crashed: ' .. msg)
	os.exit(2)
end


Logger.log('cwmp-xfrd', 'notice', 'Transfer daemon finished.')
os.exit(0)

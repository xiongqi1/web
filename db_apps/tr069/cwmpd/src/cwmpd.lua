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
require('rdbevent')
require('luardb')
luasyslog.open('cwmpd', 'LOG_DAEMON')
local oldSink = Logger.sink
Logger.sink = function(subsystem, level, msg)
	if Logger.compareLevels(level, conf.log.stdoutLevel) >= 0 or Logger.debug then
		oldSink(subsystem, level, msg)
	end
	luasyslog.log(level, subsystem .. ': ' .. msg)
end
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem('cwmpd')

require('Parameter.Tree')
require('Daemon.Client')
require('Daemon.ACS')
require('Daemon.Host')

-- This is to support compatibility with luasocket v2 and v3.
-- In v2, luasocket module is defined with "module" function. So luasocket module is allocated to the global variable "socket", when the module is loaded on the process.
-- However, in v3, the global definition for the "socket" variable have been taken out, so existing source, which uses global "socket" variable, can cause run-time failure.
socket=require('socket')

function runIt()
	local running = true
	-- This rdb notification handler is to monitor status change of 'conf.rdb.enable'
	-- until real handler is registered.
	-- In the original design, the handler is registered in client:init().
	-- But it has big timing gap(it is variant-dependent, though. It can be several dozen seconds)
	-- between launching the daemon and registering handler.
	rdbevent.onChange(conf.rdb.enable, function(key, value)
		if value == '0' then
			running = false
		end
	end)
	-- Host binding implementation
	host = Daemon.Host.new()

	local isEnable = luardb.get(conf.rdb.enable)
	-- Final check for the case missing rdb notification.
	-- In very rare case, tr069 client daemon could be executed
	-- after conf.rdb.enable is disabled.
	-- In that case, rdb notification handler could not detect the changes.
	-- So here, check the status of conf.rdb.enable
	-- and if it is disabled, terminate tr069 daemon.
	if isEnable ~= '1' then
		return
	end

	host:setStatus('starting')

	-- ACS connection model
	acs = Daemon.ACS.new()

	-- Config parse (parameter tree/data model)
	Logger.log('cwmpd', 'info', 'Parsing Configuration.')
	local ret, treeOrError = pcall(Parameter.Tree.parseConfigFile, conf.fs.config)
	if not ret then
		host:setStatus('config-error')
		Logger.log('cwmpd', 'error', 'Configuration parsing failure: ' .. treeOrError)
		os.exit(1)
	end
	paramTree = treeOrError
	Logger.log('cwmpd', 'info', 'Configuration parsed.')

	host:setStatus('config-ok')

	-- Client main logic loop
	client = Daemon.Client.new(acs, paramTree, host)
	client:init()
	host:setStatus('initialised-ok')
	while client.running and running do
		client:poll()
	end
end

Logger.log('cwmpd', 'notice', 'Client start-up.')

local ret, msg = xpcall(runIt, debug.traceback)
if not ret then
	host:setStatus('crashed')
	Logger.log('cwmpd', 'error', 'Client crashed: ' .. msg)
	os.exit(2)
end

host:setStatus('finished-ok')
Logger.log('cwmpd', 'notice', 'Client finished gracefully.')
os.exit(0)

#!/usr/bin/env lua
require('luardb')
require('luasyslog')
require('stringutil')
require('rdbqueue')

-- setup syslog facility
luasyslog.open('unidq', 'LOG_DAEMON')

conf = dofile('/usr/lib/tr-069/config.lua')

local vlanConsume = rdbqueue.new('UNID', conf.wntd.vlanq)
-- we will need the lock

local action = { }

function action.CODES(v)
	luasyslog.log('LOG_INFO', 'UNID-Q codes :' .. v)
	os.execute('ar_codes -f ' ..v.. '|' .. conf.wntd.switchConfig)
end

function action.MODE(v)
	luasyslog.log('LOG_INFO', 'UNID-Q setunid :' .. v)
	os.execute(conf.wntd.set_unid..' do '..conf.wntd.switchConfig..' '..v)
end

function action.SHELL(v)
	luasyslog.log('LOG_INFO', 'UNID-Q shell :' .. v)
	os.execute('sh ' ..v.. '|'.. conf.wntd.switchConfig)
end

-- wait for changes
while true do
	data = vlanConsume:dequeue(true)
	for k, v in pairs(data) do
		luasyslog.log('LOG_INFO', 'UNID-Q :' .. k .. ' -> ' .. v)
		if not action[k] then
			error('Do not know how to do action "' .. k .. '".')
		end
		action[k](v)
	end
end

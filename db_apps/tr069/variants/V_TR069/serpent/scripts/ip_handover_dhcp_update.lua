#!/usr/bin/env lua

--[[
This script is invoked by dnsmasq for IP handover mode DHCP lease.

Copyright (C) 2018 NetComm Wireless Limited.
--]]

syslog = require('luasyslog')
rdb = require('luardb')

syslog.open('ip_handover_dhcp_update', 'LOG_DAEMON')

syslog.log('LOG_DEBUG', 'started')

local event = arg[1]
local updates = {mac = arg[2], ip = arg[3], host = arg[4]}
-- Note according to dnsmasq.conf.example host is optional
if not updates.host then
	updates.host = "unknown"
end
updates.expiry = os.getenv('DNSMASQ_LEASE_EXPIRES')

local msg = 'event = ' .. event
for k, v in pairs(updates) do
	msg = msg .. ', ' .. k .. ' = ' .. v
end

syslog.log('LOG_INFO', msg)

local rdbPrefix = 'tr069.dhcp.ip_handover.'

if event == 'add' or event == 'old' then
	syslog.log('LOG_INFO', 'add/update a lease')
	for k, v in pairs(updates) do
		rdb.set(rdbPrefix .. k, v)
	end
elseif event == 'del' then
	if updates.ip == rdb.get(rdbPrefix .. 'ip') and updates.mac == rdb.get(rdbPrefix .. 'mac') then
		syslog.log('LOG_INFO', 'delete a lease')
		for k, v in pairs(updates) do
			rdb.unset(rdbPrefix .. k)
		end
	end
end

syslog.log('LOG_DEBUG', 'ended')

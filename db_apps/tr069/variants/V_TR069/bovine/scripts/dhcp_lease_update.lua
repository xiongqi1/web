#!/usr/bin/env lua

syslog = require('luasyslog')
rdb = require('luardb')
rdbobj = require('rdbobject')

local dhcpConfig = {
	persist = false,
	idSelection = 'smallestUnused', -- 'nextLargest', 'smallestUnused', 'sequential', 'manual'
}

local class = rdbobj.getClass('tr069.dhcp.eth0', dhcpConfig)

function addLease(mac, ip, host, expiry)
	syslog.log('LOG_INFO', 'adding lease')
	local lease = class:new()
	lease.mac = mac
	lease.ip = ip
	lease.host = host
	lease.expiry = expiry
	lease.id = 0
	lease.interface = 'Ethernet'
	return lease
end

function findLease(mac, ip)
	local leases = class:getAll()
	for _, lease in ipairs(leases) do
		if lease.mac == mac and lease.ip == ip then return lease end
	end
end

function updateLease(lease, expiry)
	syslog.log('LOG_INFO', 'updating lease expiry')
	lease.expiry = expiry
end

function deleteLease(mac, ip)
	syslog.log('LOG_INFO', 'deleting lease')
	local lease = findLease(mac, ip)
	if not lease then
		syslog.log('LOG_ERR', 'could not find lease to delete: mac = ' .. mac .. ', ip = ' .. ip)
	else
		class:delete(lease)
	end
end


syslog.open('dhcp_lease_update', 'LOG_DAEMON')

local event = arg[1]
local mac = arg[2]
local ip = arg[3]
local host = arg[4]
-- Note according to dnsmasq.conf.example host is optional
if not host then
	host = "unknown"
end
local expiry = os.getenv('DNSMASQ_LEASE_EXPIRES')

syslog.log('LOG_INFO', 'event = ' .. event .. ', mac = ' .. mac .. ', ip = ' .. ip .. ', host = ' .. host .. ', expiry = ' .. expiry)

rdb.lock()

if event == 'add' then
	local lease = findLease(mac, ip)
	if lease == nil then
		addLease(mac, ip, host, expiry)
	end
elseif event == 'old' then
	local lease = findLease(mac, ip)
	if lease then
		updateLease(lease, expiry)
	else
		addLease(mac, ip, host, expiry)
	end
elseif event == 'del' then
	deleteLease(mac, ip)
end

rdb.unlock()

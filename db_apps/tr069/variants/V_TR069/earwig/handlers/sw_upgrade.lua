----
-- Handle Bank based Software Upgrade events
--
-- Copyright (C) 2017 NetComm Wireless Limited.
----

require('Daemon')
require("luardb")
require("Logger")

------------------global variables----------------------------
local logSubsystem = 'sw_upgrade'
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)

local subRoot = conf.topRoot .. '.X_NETCOMM.SoftwareImage.'

-- convert A/B to 1/2 with 1 being the default
local function convertBankToIndex(bank)
	if bank == 'B' then
		return 2
	else
		return 1
	end
end

-- get the active/running bank as 1 (A) and 2 (B)
local function getActiveBank()
	local line = Daemon.readStringFromFile('/proc/cmdline') or ''
	local bank = line:match('active_bank=(%a)')
	return convertBankToIndex(bank)
end

-- get a u-boot env variable's value
local function getUbootVar(var)
	local result = Daemon.readCommandOutput('fw_printenv ' .. var)
	result = result:match('=(%S*)')
	if result == nil then result = '' end
	return result
end

-- get the committed bank as 1 (A) and 2 (B)
local function getCommittedBank()
	local result = Daemon.readCommandOutput('fw_printenv active_bank')
	local bank = result:match('active_bank=(%a)')
	return convertBankToIndex(bank)
end

-- commit/make permanent the active/running bank if not already
local function commitBank(bank, value)
	if value and getCommittedBank() ~= bank and getActiveBank() == bank then
		Daemon.readCommandOutput('commit_alt_bank')
	end
end

-- set the bootonce flag for the non active bank
local function bootonceBank(bank, value)
	local committedBank = getCommittedBank()
	if value and committedBank ~= bank then
		-- dont reboot here, wait for the reboot command
		luardb.set('sw.' .. bank .. '.bootonce', 1)
		-- set the committed bank bootonce to 0 as per the rules
		if bank == 2 then
			otherBank = 1
		else
			otherBank = 2
		end
		luardb.set('sw.' .. otherBank .. '.bootonce', 0)
	else
		luardb.set('sw.' .. bank .. '.bootonce', 0)
	end
end

-- supported paths and properties
return {
	-- Bank 1 (A in u-boot)
	[subRoot .. '1.Version'] = {
		get = function(node, name)
			local value = getUbootVar('bank_A_version')
			Logger.log(logSubsystem, 'debug', 'bank_A_version=' .. value)
			return 0, value
		end,
	},
	[subRoot .. '1.Valid'] = {
		get = function(node, name)
			local value = getUbootVar('bank_A_valid')
			Logger.log(logSubsystem, 'debug', 'bank_A_valid=' .. value)
			return 0, value
		end,
	},
	[subRoot .. '1.Active'] = {
		get = function(node, name)
			local value = tostring((getActiveBank() == 1) and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.1.active=' .. value)
			return 0, value
		end
	},
	[subRoot .. '1.Committed'] = {
		get = function(node, name)
			local value = tostring((getCommittedBank() == 1) and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.1.committed=' .. value)
			return 0, value
		end,
		set = function(node, name, value)
			Logger.log(logSubsystem, 'debug', 'set sw.1.committed=' .. value)
			local valid = getUbootVar('bank_A_valid')
			if valid ~= '1' then
				return CWMP.Error.InvalidParameterValue
			end
			commitBank(1, true)
			return 0
		end
	},
	[subRoot .. '1.BootOnce'] = {
		init = function(node, name)
			luardb.set('sw.1.bootonce', 0)
			return 0
		end,
		get = function(node, name)
			local value = tostring((luardb.get('sw.1.bootonce') == '1') and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.1.bootonce=' .. value)
			return 0, value
		end,
		set = function(node, name, value)
			Logger.log(logSubsystem, 'debug', 'set sw.1.bootonce=' .. value)
			bootonceBank(1, value)
			return 0
		end
	},

	-- Bank 2 (B in u-boot)
	[subRoot .. '2.Version'] = {
		get = function(node, name)
			local value = getUbootVar('bank_B_version')
			Logger.log(logSubsystem, 'debug', 'bank_B_version=' .. value)
			return 0, value
		end,
	},
	[subRoot .. '2.Valid'] = {
		get = function(node, name)
			local value = getUbootVar('bank_B_valid')
			Logger.log(logSubsystem, 'debug', 'bank_B_valid=' .. value)
			return 0, value
		end,
	},
	[subRoot .. '2.Active'] = {
		get = function(node, name)
			local value = tostring((getActiveBank() == 2) and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.2.active=' .. value)
			return 0, value
		end
	},
	[subRoot .. '2.Committed'] = {
		get = function(node, name)
			local value = tostring((getCommittedBank() == 2) and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.2.committed=' .. value)
			return 0, value
		end,
		set = function(node, name, value)
			Logger.log(logSubsystem, 'debug', 'set sw.2.committed=' .. value)
			local valid = getUbootVar('bank_B_valid')
			if valid ~= '1' then
				return CWMP.Error.InvalidParameterValue
			end
			commitBank(2, true)
			return 0
		end
	},
	[subRoot .. '2.BootOnce'] = {
		init = function(node, name)
			luardb.set('sw.2.bootonce', 0)
			return 0
		end,
		get = function(node, name)
			local value = tostring((luardb.get('sw.2.bootonce') == '1') and 1 or 0)
			Logger.log(logSubsystem, 'debug', 'sw.2.bootonce=' .. value)
			return 0, value
		end,
		set = function(node, name, value)
			Logger.log(logSubsystem, 'debug', 'set sw.2.bootonce=' .. value)
			bootonceBank(2, value)
			return 0
		end
	}
}

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
require('tableutil')

Logger = {}

Logger.debug = false

Logger.defaultLevel = 'notice'

Logger.levels = {
	'debug',
	'info',
	'notice',
	'warning',
	'error',
	'critical',
	'alert',
	'emergency'
}

Logger.levelIds = {}
for levelId, level in ipairs(Logger.levels) do
	Logger.levelIds[level] = levelId
end

Logger.subsystems = {
}

Logger.subsystemLevels = {
}

local function assertLevel(level)
	assert(table.contains(Logger.levels, level),  'No such logging level "' .. tostring(level) .. '".')
end

local function assertSubsystem(subsystem)
	assert(Logger.subsystems[subsystem], 'Subsystem "' .. subsystem .. '" unknown.')
end

local function getLevelId(level)
	return assert(Logger.levelIds[level], 'Level ID for level "' .. level .. '" not found.')
end

function Logger.compareLevels(level1, level2)
	return (getLevelId(level1) - getLevelId(level2))
end

function Logger.addSubsystem(name, level)
	level = level or Logger.defaultLevel
	assertLevel(level)
	assert(not Logger.subsystems[name], 'Subsystem "' .. name .. '" already registered.')
	Logger.subsystems[name] = level
end

function Logger.setLevel(subsystem, level)
	assertSubsystem(subsystem)
	assertLevel(level)
	Logger.subsystems[subsystem] = level
end

function Logger.log(subsystem, level, msg)
	assertSubsystem(subsystem)
	assertLevel(level)
	local subsystemLevel = Logger.subsystems[subsystem]
	if Logger.debug or Logger.compareLevels(level, subsystemLevel) >= 0 then
		Logger.sink(subsystem, level, msg)
	end
end

function Logger.throw(subsystem, msg)
	Logger.log(subsystem, 'error', msg)
	error(msg, 2)
end

function Logger.assert(bool, subsystem, msg)
	if bool then return bool end
	Logger.log(subsystem, 'error', 'Assertion failure: ' .. msg)
	error(msg, 2)
end

function Logger.sink(subsystem, level, msg)
	print(os.date('%F %T: ') .. subsystem .. '[' .. level .. ']: ' .. msg)
	io.flush()
end

return Logger

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
require('Parameter')

require('Config.Parser')
require('tableutil')

Parameter.Tree = {}

-- This doesn't need due to dynamic forcedInformParameters function
--[[
local forcedInformParameters = {
	'$ROOT.DeviceSummary',
	'$ROOT.DeviceInfo.SpecVersion',
	'$ROOT.DeviceInfo.HardwareVersion',
	'$ROOT.DeviceInfo.SoftwareVersion',
	'$ROOT.DeviceInfo.ProvisioningCode',
	'$ROOT.ManagementServer.ConnectionRequestURL',
	'$ROOT.ManagementServer.ParameterKey',
--	'$ROOT.WANDevice.*.WANConnectionDevice.*.*.ExternalIPAddress',
}
--]]
local forcedInformParameters = {}

-- initialise the parameter tree ready for use
-- calls all init() methods of parameter handlers
local function init(self)
	self.root:recursiveInit()
end

-- mark a node as changed for presentation at the next inform
local function markChanged(self, node)
	if not table.contains(self.changedNodes, node) then
		Logger.log('Parameter', 'debug', 'Tree: Marking changed: ' .. node:getPath())
		table.insert(self.changedNodes, node)
	end
end

-- clear all changed node listings (i.e. after inform success)
local function clearChanged(self)
	self.changedNodes = {}
end

-- fetch all nodes required for inform (changed + forced inform)
local function getInformParameterNodes(self)
	local informNodes = table.copy(self.changedNodes)
	for _, nodeName in ipairs(forcedInformParameters) do
		local node = self:find(nodeName)
		if node then
			if not table.contains(informNodes, node) then
				table.insert(informNodes, node)
			end
		else
			Logger.log('Parameter', 'warning', 'Non-existant forced inform parameter: ' .. nodeName)
		end
	end
	return informNodes
end

-- set the $ROOT.ManagementServer.ParameterKey value
local function setParameterKey(self, value)
	local paramKeyNode = self:find('$ROOT.ManagementServer.ParameterKey')
	assert(paramKeyNode, 'ParameterKey not found in data model.')
	return paramKeyNode:setCWMPValue(value)
end

local function addforcedInformParameter(self, path)
	local node = self:find(path)

	if not node then
		error('Invalid forcedInformParameters Path: (' .. path .. ').')
	else
		if not table.contains(forcedInformParameters, path) then
			table.insert(forcedInformParameters, path)
		end
	end

--[[
	Logger.log('Parameter', 'error', 'addforcedInformParameter path=[' ..  path .. ']')
	Logger.log('Parameter', 'error', 'the number Of entries' .. #forcedInformParameters )
	Logger.log('Parameter', 'error', '-----------------------------------------------')
	for i, entry in ipairs(forcedInformParameters) do
		Logger.log('Parameter', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Parameter', 'error', '-----------------------------------------------')
--]]
end

local function removeforcedInformParameter(self, path)
	local node = self:find(path)

	if not node then
		error('Invalid forcedInformParameters Path: (' .. path .. ').')
	else
		if table.contains(forcedInformParameters, path) then
			table.remove(forcedInformParameters, path)
		end
	end
--[[
	Logger.log('Parameter', 'error', 'removeforcedInformParameter path=[' ..  path .. ']')
	Logger.log('Parameter', 'error', 'the number Of entries' .. #forcedInformParameters )
	Logger.log('Parameter', 'error', '-----------------------------------------------')
	for i, entry in ipairs(forcedInformParameters) do
		Logger.log('Parameter', 'error', '#' .. i .. ': [' .. entry .. ']')
	end
	Logger.log('Parameter', 'error', '-----------------------------------------------')
--]]
end

-- fetch the name of the root object of the tree
local function getRootName(self)
	return self.root.children[1].name
end

-- your basic find a parameter node method
-- returns the node or nil if it doesn't exist
-- $ROOT subs the name returned by getRootName
local function find(self, path)
	path = path:gsub('%$ROOT', self:getRootName())
--	Logger.log('Parameter', 'debug', 'Parameter.find("' .. path .. '").')
	local node = self.root
	local pathBits = path:explode('.')
	for _, bit in ipairs(pathBits) do
		if bit == '' then
			if node == self.root then
--				Logger.log('Parameter', 'debug', 'Parameter.find(): found root node.')
				return node.children[1]
			elseif node:isObject() then
--				Logger.log('Parameter', 'debug', 'Parameter.find(): found object.')
				return node
			else
--				Logger.log('Parameter', 'debug', 'Parameter.find(): found non-object?')
				return node
			end
		else
			node = node:getChild(bit)
			if not node then
				Logger.log('Parameter', 'debug', 'Parameter.find("' .. path .. '"): not found.')
				return nil
			end
		end
	end
--	Logger.log('Parameter', 'debug', 'Parameter.find(): found.')
	return node
end

-- to be used to fetch required/well known parameters, such as during inform construction
-- should not be used elsewhere, such as in parameter handlers unless you know what you are doing :)
local function getValue(self, path)
	path = path:gsub('%$ROOT', self:getRootName())
	local node = self:find(path)
	assert(node, 'No such parameter "' .. path .. '".')
	local ret, val = node:getValue(path)
	assert(ret == 0, 'Error reading parameter "' .. path .. '" value: ' .. ret)
	assert(type(val) == 'string', 'Getting value of parameter "' .. path .. '" failed: ' .. val)
	return val
end

-- used by the getParameter* RPCs mainly.
local function getByName(self, path, nextLevel)
	local params = {}
	nextLevel = nextLevel or false
	local node = self:find(path)
	if not node then return nil end
	if nextLevel then
		if path == '' or path == '.' then
			table.insert(params, node)
		else
			for _, child in ipairs(node.children) do
				if child.type ~= 'default' then
					table.insert(params, child)
				end
			end
		end
	else
		node:forAll(function(self)
			if self.type == 'default' then return true end
			table.insert(params, self)
		end)
	end
	Logger.log('Parameter', 'debug', 'Tree.getByName(): ' .. #params .. ' parameters')
	return params
end

-- get Parameters/Objects with wildcards.
--
-- This is for TR-157(TR-157_Amendment-10 : A.3.1).
-- So only wildcard "*" is applicable for instance identifiers.
local function getByWildcards(self, pathWildcards)
	local wildcardPrefix = string.match(pathWildcards, "([^%*]+)%*.*")
	if not wildcardPrefix then
		-- There are no wildcards.
		return self:getByName(pathWildcards, false)
	end

	local params = {}
	local pathPattern = string.gsub(pathWildcards, "*", "(%%d+)") -- Wildcards is only for instance identifiers, so "*" is replaced with %d.
	if not string.match(pathPattern, ".+%.$") then
		-- The path is parameter.
		pathPattern = pathPattern .. "$"
	end
	local allNodes = self:getByName(wildcardPrefix, false)
	for _, node in pairs(allNodes) do
		if string.match(node:getPath(true), pathPattern) then
			table.insert(params, node)
		end
	end
	return params
end

-- test for ACS url
local function isManagementServerURL(self, nodePath)
	local path = self:getRootName() .. '.ManagementServer.URL'
	return nodePath == path
end

-- periodic inform data
local function getPeriodicInformData(self)
	local enabled = self:getValue('$ROOT.ManagementServer.PeriodicInformEnable')
	enabled = (enabled == '1' or enabled == 'true')

	local interval = self:getValue('$ROOT.ManagementServer.PeriodicInformInterval')
	interval = tonumber(interval) or conf.cwmp.defaultPeriodicInterval
	
	local phase = self:getValue('$ROOT.ManagementServer.PeriodicInformTime')
	phase = tonumber(phase) or conf.cwmp.defaultPeriodicPhase

	return {
		enabled = enabled,
		interval = interval,
		phase = phase,
	}
end

-- Validation check for "Partial Path Name".
-- "Partial Path Name": A Path Name that ends with a “.” (dot).
-- References an Object and represents a subset of the name hierarchy.
local function isValidPartialPathName(self, path)
	local node = self:find(path)
	return node and (path == '' or node:isObject() and path:endsWith('.'))
end

function Parameter.Tree.new(rootNode)
	local tree = {
		root = rootNode,
		changedNodes = {},

		init = init,
		markChanged = markChanged,
		clearChanged = clearChanged,
		getInformParameterNodes = getInformParameterNodes,
		setParameterKey = setParameterKey,
		addforcedInformParameter = addforcedInformParameter,
		removeforcedInformParameter = removeforcedInformParameter,

		getRootName = getRootName,
		find = find,
		getByName = getByName,
		getValue = getValue,
		getByWildcards = getByWildcards,

		isManagementServerURL = isManagementServerURL,
		getPeriodicInformData = getPeriodicInformData,
		isValidPartialPathName = isValidPartialPathName,
	}
	return tree
end

function Parameter.Tree.parseConfigFile(configFile)
	local rootNode = Config.Parser.parse(conf.fs.config)
	return Parameter.Tree.new(rootNode)
end

return Parameter.Tree

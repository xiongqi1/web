
local M = {}
hdlerUtil = M

require('lfs')
require('Logger')
Logger.addSubsystem('hdlerUtil')

----------------------------------------------------------------------------
-- Convert External formatted Bool value to string '1' or '0'
-- Possible input: string type 'true'|'false' or '1'|'0' or number type 1|0
-- return: string type '1' or '0', if failed, nil
----------------------------------------------------------------------------
function M.ToInternalBoolean(val)
	local inputStr = val and tostring(val) or ''

	inputStr = (string.gsub(inputStr, "^%s*(.-)%s*$", "%1"))

	if inputStr == '1' or inputStr:lower() == 'true' then
		return '1'
	elseif inputStr == '0' or inputStr:lower() == 'false' then
		return '0'
	end

	return nil
end

----------------------------------------------------------------------------
-- This function can be used to convert External formatted interger
-- (both of string type and number type) to number type intenger, 
-- or to check the input integer is in given range.
--
-- * Usage
--	M.ToInternalInteger{input=number, minimum=0, maximum=50}
--
-- * Argument
--	input: number to convert. both of string type and number type are possible.
--	minimum: minimum value of the range. Default: -2147483648
--	maximum: maximum value of the range. Default: 2147483647
--
-- * Return
-- 	success: return interger type value
-- 	false: return nil
----------------------------------------------------------------------------
function M.ToInternalInteger(arg)
	local convertedInt = arg.input and tonumber(arg.input) or nil

	if convertedInt then
		local minimum = arg.minimum or -2147483648
		local maximum = arg.maximum or 2147483647

		minimum = tonumber(minimum)
		maximum = tonumber(maximum)

		if not minimum or not maximum then return nil end
		if convertedInt < minimum or convertedInt > maximum then return nil end

		return convertedInt

	else
		return nil
	end
end

-- Argument:
--	table:	root table to register the callback variable. The table should be created before use this function
--	name:	variable name. Sould be a string type
--	value:	the value of "name" variable. To unregister the registered variable, set this argument to nil.
--	callbackFunc: callback function
--	callbackTaskType: This argument decides the time to run the callback function. Available values are [preSession|postSession|cleanUp]. Not allowed nil.
-- function M.register_cbVariable (table, name, value, callbackFunc, callbackTaskType)
-- 	if not table or not name or not callbackFunc or not callbackTaskType then return false end
-- 	if type(name) ~= 'string' then return false end
-- 	if type(callbackFunc) ~= 'function' then return false end
-- 	if callbackTaskType ~= 'preSession' and callbackTaskType ~= 'postSession' and callbackTaskType ~= 'cleanUp' then return false end
-- 	if type(table) ~= 'table' then return false end
-- 
-- 	table[name] = value
-- 
-- 	if client:isTaskQueued(callbackTaskType, callbackFunc) ~= true then
-- 		client:addTask(callbackTaskType, callbackFunc, false, table)
-- 	end
-- 
-- 	return true
-- end

function M.IsDirectory(path)
	local attrib = lfs.attributes(path)

	if not attrib or attrib.mode ~= 'directory' then return false end

	return true
end

function M.IsRegularFile(path)
	local attrib = lfs.attributes(path)

	if not attrib or attrib.mode ~= 'file' then return false end

	return true
end

-- usage: traverseRdbVariable{prefix='service.firewall.dnat.', suffix=, startIdx=0}
-- If startIdx is nil, then default value is 1
function M.traverseRdbVariable (arg)
	local i = arg.startIdx or 1;
	local cmdPrefix, cmdSuffix

	cmdPrefix = arg.prefix or ''
	cmdSuffix = arg.suffix or ''
		
	return (function ()
		local index = i
		local v = luardb.get(cmdPrefix .. index .. cmdSuffix)
		i= i + 1
		if v then 
			return index, v
		end
	end)
end

-- usage: getNumOfSubInstance(path_of_targetObject)
-- return value: the number of children instance of targetObject with string type
function M.getNumOfSubInstance(path)
        local targetName = string.trim(path)
        if targetName == '' then return "0" end

        local targetNode = paramTree:find(targetName)
        if not targetNode or not targetNode:isObject() then return "0" end

        return tostring(targetNode:countInstanceChildren())
end

return hdlerUtil




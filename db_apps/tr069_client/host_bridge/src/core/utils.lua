----
-- Node Tree Functions
----

function findNode(node, path)
--	print('findNode', path)
	local pathBits = path:explode('.')
	for _, bit in ipairs(pathBits) do
--		print('findNode', 'step', bit)
		if bit == '' then
			if node:isObject() then
--				print('findNode', 'isObject', node)
				return node
			else
--				print('findNode', 'isNotObject', node)
				return nil
			end
		else
			node = node:getChild(bit)
			if not node then
--				print('findNode', 'noChild', node)
				return nil
			end
		end
	end
--	print('findNode', 'endFound', node)
	return node
end

----
-- Handler Util Functions
----

local function _globMatch(patternBits, pathBits)
	if #patternBits == 0 and #pathBits == 0 then return true end
	if (#patternBits == 0 and #pathBits > 0) or (#pathBits == 0 and #patternBits > 0) then return false end

	local patternBit = table.remove(patternBits, 1)
	local pathBit = table.remove(pathBits, 1)

--	print('match', patternBit, pathBit)

	if patternBit == '*' then
		if _globMatch(patternBits, pathBits) then return true end
	elseif patternBit == '**' then
		if #patternBits == 0 then return true end -- trailing **
		local eatenPathBits = {}
		while #pathBits > 0 do
--			print('try', table.concat(patternBits, '.'), table.concat(pathBits, '.'))
			if _globMatch(patternBits, pathBits) then return true end
			table.insert(eatenPathBits, table.remove(pathBits, 1))
		end
		while #eatenPathBits > 0 do table.insert(pathBits, 1, table.remove(eatenPathBits)) end
	elseif patternBit:find('%|') then
		local alternates = patternBit:explode('|')
		if table.contains(alternates, pathBit) then
			if _globMatch(patternBits, pathBits) then return true end
		end
	elseif patternBit == pathBit then
		if _globMatch(patternBits, pathBits) then return true end
	end

	table.insert(patternBits, 1, patternBit)
	table.insert(pathBits, 1, pathBit)
	return false
end

function pathGlobMatches(pattern, path)
	local patternBits = pattern:explode('.')
	local pathBits = path:explode('.')

	return _globMatch(patternBits, pathBits)
end

function findHandler(handlerTable, paramName)
	if(paramName ~= nil and paramName:endsWith('.')) then paramName = paramName:sub(1, paramName:len() - 1) end
--	dimclient.log('debug', 'findHandler(): paramName = ' .. (paramName or 'nil'))
	local matching = table.filter(handlerTable, function(k, v) return pathGlobMatches(k, paramName) end)
	local numMatching = table.count(matching)
	if numMatching > 1 then error('More than one handler matches parameterName "' .. paramName .. '"') end
--	dimclient.log('debug', 'findHandler(): #matching = ' .. numMatching)
	return matching[next(matching)]
end

function readStringFromFile(filename)
	local file = io.open(filename, 'r')
	local line = file:read('*l')
	file:close()
	return line
end

function readIntFromFile(filename)
	local file = io.open(filename, 'r')
	local n = math.floor(file:read('*n'))
	file:close()
	return n
end

function readEntireFile(filename)
	local file = io.open(filename, 'r')
	local data = file:read('*a')
	file:close()
	return data
end

----
-- Validation Functions
----

function isValidIP4(ip)
	local ret, _, o1, o2, o3, o4 = string.find(ip, '^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
	if ret == nil then return false end
        local octet = { o1, o2, o3, o4 }
	for i = 1,4 do
		if tonumber(octet[i]) > 255 then return false end
        end
	return true
end

local validNetmaskOctets = { 255, 254, 253, 248, 240, 224, 192, 128, 0 }

function isValidIP4Netmask(mask)
	local ret, _, o1, o2, o3, o4 = string.find(mask, '^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
	if ret == nil then return false end
        local octet = { o1, o2, o3, o4 }
	local transition = false
	for i = 1,4 do
		if not table.contains(validNetmaskOctets, tonumber(octet[i])) then
			return false
		end
		if tonumber(octet[i]) < 255 and not transition then
			transition = true
		elseif transition and tonumber(octet[i]) ~= 0 then
			return false
		end
        end
	return true
end

function isBooleanType(val)
	if val == nil or type(val) ~= "string" then return false end
	if val == "1" or val == "0" or val:lower() == "true" or val:lower() == "false" then return true end

	return false
end

----
-- String Functions
----
function trimLRMargin(val)
	if type(val) ~= "string" then return val end

	return string.gsub(val:gsub("^%s+", ""), "%s+$", "")
end

----
-- Type Convertor
----
function convertInternalBoolean(val)
	if not val then return nil end
	val = tostring(val)
	val = trimLRMargin(val)

	if not isBooleanType(val) then return nil end

	if val:lower() == "true" then return "1"
	elseif val:lower() == "false" then return "0"
	else return val end

end

-- usage: convertInternalInteger{input=number, minimum=0, maximum=50}
-- If number doesn't have a specific range, just set "minimum" or "maximum" to nil or omit this argument.
-- success: return interger type value
-- false: return nil
function convertInternalInteger(arg)
	local convertedInt = tonumber(arg.input)

	if type(convertedInt) == 'number'
	then
		local minimum = arg.minimum or -2147483648
		local maximum = arg.maximum or 2147483647

		minimum = tonumber(minimum)
		maximum = tonumber(maximum)

		if minimum == nil or maximum == nil then return nil end
		if convertedInt < minimum or convertedInt > maximum then return nil end

		return convertedInt

	else
		return nil
	end
end

-- usage: convertInternalInteger{input=number, maximum=50}

-- success: return interger type value
-- false: return nil

function convertInternalUnsignedInteger(arg)
	return convertInternalInteger{input=arg.input, minimum=0, maximum=arg.maximum or 4294967295}
end
----
-- 
----
function execute_CmdLine(command)
	local retVal=""
	local filename = os.tmpname ()

	os.execute (command .. " > " .. filename)

	local file = io.open(filename, 'r')
	retVal = file:read('*a')
	file:close()

	os.remove (filename)

	return retVal
end

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
require('Logger')
Logger.addSubsystem('Parameter')

require('CWMP')
require('tableutil')
require('stringutil')

Parameter = {}

local function _globMatch(patternBits, pathBits)
	if #patternBits == 0 and #pathBits == 0 then return true end
	if (#patternBits == 0 and #pathBits > 0) or (#pathBits == 0 and #patternBits > 0) then return false end

	local patternBit = table.remove(patternBits, 1)
	local pathBit = table.remove(pathBits, 1)

--	Logger.log('Parameter', 'debug', 'Parameter._globMatch(): "' .. tostring(patternBit) .. '" match "' .. tostring(pathBit) .. '".')

	if patternBit == '*' then
		if _globMatch(patternBits, pathBits) then return true end
	elseif patternBit == '**' then
		if #patternBits == 0 then return true end -- trailing **
		local eatenPathBits = {}
		while #pathBits > 0 do
--			Logger.log('Parameter', 'debug', 'Parameter._globMatch(): try: "' .. table.concat(patternBits, '.') .. '" match "' .. table.concat(pathBits, '.') .. '".')
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

function Parameter.pathMatchesGlob(pattern, path)
	local patternBits = pattern:explode('.')
	local pathBits = path:explode('.')

	return _globMatch(patternBits, pathBits)
end

function Parameter.findHandler(handlerTable, paramName)
	assert(type(handlerTable) == 'table', 'Expected table of handler entries')
	assert(paramName, 'Parameter name required')

	if(paramName:endsWith('.')) then paramName = paramName:sub(1, paramName:len() - 1) end
--	Logger.log('Parameter', 'debug', 'Parameter.findHandler("' .. paramName .. '")')
	local matching = table.filter(handlerTable, function(k, v) return Parameter.pathMatchesGlob(k, paramName) end)
	if table.count(matching) < 1 then
		Logger.log('Parameter', 'warning', 'Parameter.findHandler("' .. paramName .. '"): No handlers.')
	end
	if table.count(matching) > 1 then
		Logger.error('Parameter', 'error', 'Parameter.findHandler("' .. paramName .. '"): More than one handler.')
	end
	return matching[next(matching)]
end

return Parameter

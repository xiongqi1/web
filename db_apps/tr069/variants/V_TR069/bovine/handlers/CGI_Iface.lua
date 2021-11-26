
require('luacurl')
require('Logger')
Logger.addSubsystem('CGI_Iface')

CGI_Iface = {}


------------------------ local function ------------------------
local search_char_idx
local traverse_element
local parse_array
local parse_javascript
local parse_json
local print_table
----------------------------------------------------------------
local lock_table = {
	['\''] = { unlock_char='\'', priority=1},
	['\"'] = { unlock_char='\"', priority=1},
	['['] =  { unlock_char=']', priority=2},
	['{'] =  { unlock_char='}', priority=2},
}

search_char_idx = function (str, s_idx, div)
	local locked_by = nil
	local strLen = str:len()
	local i = s_idx

	while (i < strLen) do
		local c = str:sub(i,i)

		if lock_table[c] and (not locked_by or lock_table[c].priority < lock_table[locked_by].priority) then
			savedLock = locked_by
			locked_by = c
		elseif locked_by and lock_table[locked_by].unlock_char == c then
			locked_by = savedLock
			savedLock = nil
		end

		if c == div and not locked_by then
			return i
		end
		i = i + 1
	end

	return i
end

traverse_element = function(str, div)
	local pos = 1

	return (function ()
		if not str or pos > str:len() then return end

		local no_blank_idx = str:find("%S", pos)
		local idxS = pos
		local idxL = pos
		pos = no_blank_idx

		if not pos then return end

		local first_char = str:sub(pos,pos)
		idxL = search_char_idx(str, idxL, div)
		pos = idxL + 1
		local last_char = str:sub(idxL,idxL)

		local result
		if last_char == div then
			result = string.sub(str, idxS, idxL-1)
		else
			result = string.sub(str, idxS, idxL)
		end

		result = string.gsub((result:gsub("^%s*(.*)$", "%1") or ''), '%s*$' , '')
		return result
	end)
end

parse_array = function(tbl, str)
	local element = str:match("^%s*%[(.*)%]%s*$");
	local array_idx = 1

	for valueOtter in traverse_element(element, ',') do
		local name = nil
		local value = nil
		local ret = nil
		for valueInner in traverse_element(valueOtter, '=') do
			if not name then 
				ret, name = valueInner:match("^([\"\'])(.-)%1")
				if not ret then name = valueInner end
			else
				ret, value = valueInner:match("^([\"\'])(.-)%1")
				if not ret then value = valueInner end
			end
		end

		if not value then
			local first_char = name:sub(1,1)
			if first_char == '{' then
				tbl[array_idx] = {}
				parse_json(tbl[array_idx], name)
				array_idx = array_idx + 1
			else
				table.insert(tbl,name)
			end
			
		else
			local first_char = value:sub(1,1)
			if first_char == '[' then
				tbl[name] = {}
				parse_array(tbl[name], value)
			elseif first_char == '{' then
				parse_json(tbl, value)
			else
				tbl[name] = value
			end
		end
	end
end

parse_javascript = function(tbl, str)
	local element = str:gsub("^var%s*" , "")

	for valueOtter in traverse_element(element, ',') do
		local name = nil
		local value = nil
		local ret = nil

		for valueInner in traverse_element(valueOtter, '=') do
			if not name then 
				ret, name = valueInner:match("^([\"\'])(.-)%1")
				if not ret then name = valueInner end
			else
				ret, value = valueInner:match("^([\"\'])(.-)%1")
				if not ret then value = valueInner end
			end
		end
		if not value then value = '' end

		local first_char = value:sub(1,1)
		if first_char == '[' then
			tbl[name] = {}
			parse_array(tbl[name], value)
		elseif first_char == '{' then
			parse_json(parsed_kTbl, value)
		else
			tbl[name] = value
		end
	end
end

parse_json = function(tbl, str)
	local element = str:match("^{%s*(.-)%s*}")

	for valueOtter in traverse_element(element, ',') do
			local name = nil
			local value = nil
			local ret = nil
		for valueInner in traverse_element(valueOtter, ':') do
			if not name then 
				ret, name = valueInner:match("^([\"\'])(.-)%1")
			else
				ret, value = valueInner:match("^([\"\'])(.-)%1")
			end
		end

		if not value then value = '' end

		local first_char = value:sub(1,1)
		if first_char == '[' then
			tbl[name] = {}
			parse_array(tbl[name], value)
		elseif first_char == '{' then
			parse_json(parsed_kTbl, value)
		else
			tbl[name] = value
		end
	end
end

print_table = function(tbl, path)
	if not tbl or type(tbl) ~= 'table' then return end
	
	for k, v in pairs(tbl) do
		if type(v) == 'table' then
			local pathname = path and path .. '.' .. k or k
			print_table(v, pathname)
		else
			if path then
				Logger.log('CGI_Iface', 'error', 'key=['  .. path .. '.' .. k .. '], value=[' .. v .. ']')
			else
				Logger.log('CGI_Iface', 'error', 'key=['  .. k .. '], value=[' .. v .. ']')
			end
		end
	end
end

function CGI_Iface.print_table(tbl)
	print_table(tbl)
end

function CGI_Iface.parser(table, str)
	for value in traverse_element(str, ';') do
		local first_char = value:sub(1,1)
		if first_char == '{' then
			parse_json(table, value)
		else
			parse_javascript(table, value)
		end
	end
end

-- This function appends the cgi response to the table.
-- Please verify the table is empty table before calling this function.
-- If the argument table is nil, this function will do nothing and return "false".
function CGI_Iface.getCGIresponse(table, uri)
	if not table or not uri then return false end

	local curlInst = curl.new()

	if not curlInst then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.getCGIresponse: Fail to create CURL instance')
		return false
	end

	curlInst:setopt(curl.OPT_TIMEOUT, 20)
	curlInst:setopt(curl.OPT_CONNECTTIMEOUT, 20)
	curlInst:setopt(curl.OPT_POST, true)
	curlInst:setopt(curl.OPT_URL, 'http://localhost/' .. uri)
	curlInst:setopt(curl.OPT_POSTFIELDS, '')
	curlInst:setopt(curl.OPT_HTTPHEADER,
		'Content-Type:',
		'Expect:'
	)

	local response = {}
	response.buffer = ''

	curlInst:setopt(curl.OPT_WRITEDATA, response)
	curlInst:setopt(curl.OPT_WRITEFUNCTION, function (client, buf)
		client.buffer = client.buffer .. buf
		return #buf
	end)

	local ret, msg, errCode = curlInst:perform()

	if not ret then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.getCGIresponse(): perform() failed: ' .. msg .. ' (' .. errCode .. ')')
		curlInst:close()
		return false
	end

	local code = curlInst:getinfo(curl.INFO_RESPONSE_CODE)

	if code ~= 200 then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.getCGIresponse(): RX: HTTP error: ' .. tostring(code) .. ': ' .. tostring(response.buffer))
		curlInst:close()
		return false
	end
	curlInst:close()

	CGI_Iface.parser(table, response.buffer)
	return true
end

function CGI_Iface.setValueToCGI(uri)
	if not uri then return false end

	local curlInst = curl.new()

	if not curlInst then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.setValueToCGI: Fail to create CURL instance')
		return false
	end

	curlInst:setopt(curl.OPT_TIMEOUT, 20)
	curlInst:setopt(curl.OPT_CONNECTTIMEOUT, 20)
	curlInst:setopt(curl.OPT_POST, true)
	curlInst:setopt(curl.OPT_URL, 'http://localhost/' .. uri)
	curlInst:setopt(curl.OPT_POSTFIELDS, '')
	curlInst:setopt(curl.OPT_HTTPHEADER,
		'Content-Type:',
		'Expect:'
	)

	local response = {}
	response.buffer = ''

	curlInst:setopt(curl.OPT_WRITEDATA, response)
	curlInst:setopt(curl.OPT_WRITEFUNCTION, function (client, buf)
		client.buffer = client.buffer .. buf
		return #buf
	end)

	local ret, msg, errCode = curlInst:perform()

	if not ret then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.getCGIresponse(): perform() failed: ' .. msg .. ' (' .. errCode .. ')')
		curlInst:close()
		return false
	end

	local code = curlInst:getinfo(curl.INFO_RESPONSE_CODE)

	if code ~= 200 then
		Logger.log('CGI_Iface', 'error', 'CGI_Iface.getCGIresponse(): RX: HTTP error: ' .. tostring(code) .. ': ' .. tostring(response.buffer))
		curlInst:close()
		return false
	end
	curlInst:close()
	return true
end

function CGI_Iface.EncodeUrl(Str)
	if not Str then return '' end
	return string.gsub(Str, '%W', function(Str)
			return string.format('%%%02X', string.byte(Str)) 
		end )
end

function CGI_Iface.DecodeUrl(Str)
	if not Str then return '' end
	Str = string.gsub(Str, '%+', ' ')
	Str = string.gsub(Str, '%%(%x%x)', function(Str)
			return string.char(tonumber(Str, 16)) 
		end )
	return Str
end

return CGI_Iface
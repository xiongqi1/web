#!/usr/bin/env lua
require('luardb')
require('rdbobject')
require('stringutil')
require('tableutil')

local query_string_cmd = os.getenv('cmd')

print("Content-type: text/html\n")

if( os.getenv("SESSION_ID")=="nil" or os.getenv("SESSION_ID") ~=  os.getenv("sessionid") ) then
	return
end

-- CSRF token must be valid
local csrfToken = os.getenv('csrfToken')
local csrfTokenGet = os.getenv('csrfTokenGet')
if ( csrfToken == "nil" or csrfTokenGet == "nil" or csrfToken ~= csrfTokenGet ) then
	os.exit(254)
end

local is_valid_instId = function(class, instId)
	if not class or not instId or not tonumber(instId) then return false end

	ids=class:getIds();

	for _, val in ipairs(ids) do
		if tonumber(val) == tonumber(instId) then
			return true
		end
	end
	return false
end

function is_valid_IP4(ip)
	local ret, _, o1, o2, o3, o4 = string.find(ip, '^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
	if ret == nil then return false end
	local octet = { o1, o2, o3, o4 }
	for i = 1,4 do
		if tonumber(octet[i]) > 255 then return false end
	end
	return true
end

function is_valid_IPMaskPair(value)
	if not value then return false end
	if value == '' or value == '/' then return true end
	local pair=value:explode('/')

	if not is_valid_IP4(pair[1]) then return false end
	if not pair[2] or pair[2] == '' then return true end
	local mask_numtype=tonumber(pair[2])
	if not mask_numtype or mask_numtype < 0 or mask_numtype > 32 then return false end

	return true

end

function is_valid_VID(value)
	if not value then return false end
	local numtype = tonumber(value)
	if not numtype then return false end
	if numtype < 1 or numtype > 4095 then return false end

	return true
end

local rdbObjConf = {
	persist = true,
	idSelection = 'smallestUnused'
}

local rdbCirObj = rdbobject.getClass('services.vlan.ipbased', rdbObjConf)

if query_string_cmd=="getList" then

	print("[")

	local allList = rdbCirObj:getAll();
	for idx, inst in ipairs(allList) do
		if idx ~= 1 then print(",") end
		print("{")
		local loop_cnt=1
		for key, val in rdbCirObj:iterator(inst) do
			if loop_cnt ~= 1 then print(",") end
			loop_cnt = loop_cnt+1;
			print('"' .. (key or '') .. '":"' .. (val or '') .. '"')
		end
		print("}")
	end

	print("]")

elseif query_string_cmd=="editEntry" then
	local query_string_index = os.getenv('index')
	local query_string_vlanId = os.getenv('vlanId')
	local query_string_ethPort = os.getenv('ethPort')
	local query_string_destIpMask = os.getenv('destIpMask')
	local query_string_sourIpMask = os.getenv('sourIpMask')
	local result = 1;

	if query_string_index and tonumber(query_string_index) and (tonumber(query_string_index) == -1 or is_valid_instId(rdbCirObj, query_string_index)) then
		if is_valid_VID(query_string_vlanId)
			and is_valid_IPMaskPair(query_string_destIpMask)
			and  is_valid_IPMaskPair(query_string_sourIpMask) then

			local rdbInst;
			if tonumber(query_string_index) == -1 then -- create new instance
				rdbInst = rdbCirObj:new()
				rdbInst.rdbIndex = rdbCirObj:getId(rdbInst)
			else
				rdbInst = rdbCirObj:getById(query_string_index)
			end

			rdbInst.vlanId = query_string_vlanId
			rdbInst.ethPort = query_string_ethPort
			rdbInst.destIpMask = query_string_destIpMask
			rdbInst.sourIpMask = query_string_sourIpMask
			result = 0;
		end
	end

	if result == 0 then
		luardb.set('services.vlan.ipbased.trigger', '1'); -- trigger daemon if list is updated.
	end

	print('{"cgiresult":"' .. result .. '"}')

elseif query_string_cmd=="delEntry" then
	local query_string_index = os.getenv('index')
	local result = 1;
	if query_string_index and tonumber(query_string_index) and is_valid_instId(rdbCirObj, query_string_index) then

		local rdbInst = rdbCirObj:getById(query_string_index)

		if rdbInst then
			rdbCirObj:delete(rdbInst)
			result = 0;
		end
	end

	if result == 0 then
		luardb.set('services.vlan.ipbased.trigger', '1'); -- trigger daemon if list is updated.
	end

	print('{"cgiresult":"' .. result .. '"}')
end


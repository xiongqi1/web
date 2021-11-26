#!/usr/bin/env lua
-------------------------------------------------------------------------------------
-- Copyright (C) 2018 NetComm Wireless limited.
-------------------------------------------------------------------------------------
--
-- this script provide the function call for web gui
-- 1. "getList" get the key list from rdb
-- 2. "delEntry" delete the key from rdb, and remove the key file from
--     /usr/local/cdcs/conf/pubkey
--

require('luardb')
require('rdbobject')
require('stringutil')
require('tableutil')

local query_string_cmd = os.getenv('cmd')

local skip_session_validation = 0

if query_string_cmd and query_string_cmd == 'getList' then
	skip_session_validation = 1
end

print("Content-type: text/html\n")

if skip_session_validation ~= 1 then
	if( os.getenv("SESSION_ID") == nil or os.getenv("SESSION_ID") ~=  os.getenv("sessionid") ) then
		return
	end

	-- CSRF token must be valid
	local csrfToken = os.getenv('csrfToken')
	local csrfTokenGet = os.getenv('csrfTokenGet')
	if ( csrfToken == nil or csrfTokenGet == nil or csrfToken ~= csrfTokenGet ) then
		os.exit(254)
	end
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

local rdbObjConf = {
	persist = true,
	idSelection = 'smallestUnused'
}

local rdbCirObj = rdbobject.getClass('system.firmwarePublicKey', rdbObjConf)

if query_string_cmd=="getList" then
	print("{")
	print('"publicKeyList":[')
	local allList = rdbCirObj:getAll();
	for idx, inst in ipairs(allList) do
		if idx ~= 1 then print(",") end
		print("{")
		local loop_cnt=1
		for key, val in rdbCirObj:iterator(inst) do
                        if (key=="rdbIndex" or key=="fileName") then
                                if loop_cnt ~= 1 then print(",") end
                                print('"' .. (key or '') .. '":"' .. (val or '') .. '"')
                        end
			loop_cnt = loop_cnt+1;
		end
		print("}")
	end

	print("]") -- publicKeyList
	print("}")

elseif query_string_cmd=="delEntry" then
	local query_string_index = os.getenv('index')
	local result = 1;
	if query_string_index and tonumber(query_string_index) and is_valid_instId(rdbCirObj, query_string_index) then

		local rdbInst = rdbCirObj:getById(query_string_index)

		if rdbInst then
			local rm_file_cmd = "rm " .. "/usr/local/cdcs/conf/pubkey/" .. rdbInst.fileName
                        os.execute(rm_file_cmd)

			rdbCirObj:delete(rdbInst)
			result = 0;
		end
	end

	print('{"cgiresult":"' .. result .. '"}')
end

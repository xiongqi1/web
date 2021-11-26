#!/usr/bin/env lua
require('luardb')
require('stringutil')
require('tableutil')
require('variants')

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

if query_string_cmd=="getOperList" then
	os.execute('operator_setting.lua getOperList &')

	print('{"cgiresult":"0"}')
elseif query_string_cmd=="setOperSettings" then
	local query_PLMN_select = os.getenv('PLMN_select')
	local result = 0; -- Success

	if query_PLMN_select == "0" or string.match(query_PLMN_select, '%d+,%d+,%d+') then
		os.execute('operator_setting.lua setOperSettings &')
	else
		result = 1;
	end

	print('{"cgiresult":"' .. result .. '"}')
else
	print('{"cgiresult":"1"}')
end


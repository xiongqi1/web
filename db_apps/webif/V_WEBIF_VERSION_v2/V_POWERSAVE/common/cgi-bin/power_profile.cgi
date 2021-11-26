#!/usr/bin/env lua
require('luardb')
require('stringutil')
require('tableutil')

local script_name = os.getenv('SCRIPT_NAME') or 'power_save.lua'
local query_string_cmd = os.getenv('cmd')


local function cgidecode(str)
	return (str:gsub('+', ' '):gsub("%%(%x%x)", function(xx) return string.char(tonumber(xx, 16)) end))
end


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

if query_string_cmd=="getList" then

	print("var psp_list=[")

	for i=0, 10 do
		cmd = string.format( "%s%i%s", "rdb_get power.profile.", i, ".name")
		local f = io.popen(cmd)
		n = f:read("*a")

		if  string.len(n)==0 then
			f:close()
			break
		end

		cmd = string.format( "%s%i%s", "rdb_get power.profile.", i, ".enable")
		local f1 = io.popen(cmd)
		en = f1:read("*a")

		f1:close()
		cmd = string.format( "%s%i%s", "rdb_get power.profile.", i, ".disable")
		local f1 = io.popen(cmd)
		ds = f1:read("*a")

		f1:close()
		
		if i>0 then
			io.write(",")
		end
		io.write("{'name':", string.format( "'%s", string.gsub(n, "\n", "'")), ",'enable_list':", string.format( "'%s", string.gsub(en, "\n", "")), "','disable_list':", string.format( "'%s", string.gsub(ds, "\n", "")),"'}")
	end
	print("];")

	local f = io.popen("rdb_get power.switch. -L")
	local l = f:read("*a") -- read output of command

	l = string.gsub(l, "\n", "',")
	l = string.gsub(l, " ", "':'")
	l = string.gsub(l, "power%.switch%.", "'")
	--l = string.gsub(l, "power%.switch%.", ";var")
	--l = string.gsub(l, "%.", "")

	print("var pwsw_list={",string.gsub(l, "3G", "threeG"),"};")
	f:close()

elseif query_string_cmd=="setup" then
	os.execute("/usr/sbin/power_profiles.lua")
end

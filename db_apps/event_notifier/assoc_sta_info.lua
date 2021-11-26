#!/usr/bin/lua

require('luardb')

-- dump the content of a table
function _tostring(o)

	if type(o) == "table" then
		local s = ""
		local k, v = next(o,nil)
		while k do

			s = s .. "[" .. _tostring(k) .. "]=" .. _tostring(v)

			k, v = next(o,k)

			-- add a comma if it continues
			if k then
				s = s .. ","
			end
		end

		return "{" .. s .. "}"
	elseif type(o) == "string" then
		return "'" .. tostring(o) .. "'"
	else
		return tostring(o)
	end
end

-- return AP interfaces
function get_wlan_interfaces()
	local line
	local rval = {}
	local intf
	local f = assert(io.popen("iw dev"))
	for line in f:lines() do

		-- read interface
		local _intf = line:match("Interface (%S+)");
		if _intf then
			intf = _intf
		end

		-- read type
		local _type = line:match("type (%S+)");
		if _type and _type == "AP" then
			rval[#rval+1] = intf;
		end

	end

	io.close(f)

	return rval
end

-- return station mac addresses from hostapd_cli
function get_sta_mac_addresses(interface)
	local mac
	local rval = {}

	-- execute hostapd cli
	local f = assert(io.popen("hostapd_cli all_sta -i " .. interface))

	-- get authroized stations
	for line in f:lines(f) do

		-- read mac address
		local _mac = line:match("^(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)$")
		if _mac then
			mac = _mac
		end

		-- read flags
		local _flag = line:match("^flags=(%S+)")
		if _flag and _flag:find("%[AUTHORIZED%]") then
			rval[mac]=true
		end
	end

	-- close
	f:close()

	return rval
end

function print_usage()
	print(
[[
assoc_sta_info.lua
	print all mac addresses of associated stations

usage>
	assoc_sta_info.lua <wlan network interface>

example>
	assoc_sta_info.lua wlan0
]]
	)
end

if #arg ~= 1 then
	print_usage()
	os.exit(1)
end

wlan=arg[1]

-- get mac addresses
mac=get_sta_mac_addresses(wlan)

-- print
for m,v in pairs(mac) do
	print("Station " .. m)
end

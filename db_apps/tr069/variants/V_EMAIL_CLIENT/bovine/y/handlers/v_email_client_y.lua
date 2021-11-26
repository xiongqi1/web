require('CWMP.Error')

local subROOT= conf.topRoot .. '.X_NETCOMM_WEBUI.Services.EmailSettings.'

------------------local function prototype------------------
local convertInternalBoolean
local setIfChanged
local isValidEmailAddr
local escape
local sendTestEmail
------------------------------------------------------------

convertInternalBoolean = function (val)
	local inputStr = tostring(val)

	if not inputStr then return nil end

	inputStr = (string.gsub(inputStr, "^%s*(.-)%s*$", "%1"))

	if inputStr == '1' or inputStr:lower() == 'true' then
		return '1'
	elseif inputStr == '0' or inputStr:lower() == 'false' then
		return '0'
	end

	return nil
end

setIfChanged = function (name, newVal)
	local prevVal = luardb.get(name)

	if not prevVal or tostring(prevVal) ~= tostring(newVal) then
		luardb.set(name, newVal)
	end
end

isValidEmailAddr = function (val)
	if not val then return false end
	val = string.trim(val)

	if (val:match("[A-Za-z0-9%.%%%+%-]+@[A-Za-z0-9%.%%%+%-]+%.%w%w%w?%w?")) then
		return true
	else
		return false
	end
end

local http = require("socket.http")
local ltn12 = require("ltn12")

escape = function (s)
	s = string.gsub(s, "[&=+%%%c]", function (c)
		return string.format("%%%02X", string.byte(c))
	end)
	s = string.gsub(s, " ", "+")
	return s
end

sendTestEmail = function (task)
	local index = task.data
	local path = "http://localhost/cgi-bin/send_test_email.cgi"
	local payload =""
	local b = {}
	local response_body = { }
	local arguments= {
		["recipient"] = "service.email.client.conf.TestRecipient",
		["server"] = "service.email.client.conf.server_addr",
		["port"] = "service.email.client.conf.server_port",
		["username"] = "service.email.client.conf.username",
		["password"] = "service.email.client.conf.password",
		["security"] = "service.email.client.conf.security",
		["useauth"] = "service.email.client.conf.useauth"
	}

	for k, v in pairs(arguments) do
		value=luardb.get(v) or ''
		b[#b + 1] = (escape(k) .. "=" .. escape(value))
	end

	payload = table.concat(b, "&")

	local res, code, response_headers, status = http.request
	{
		url = path,
		method = "POST",
		headers =
		{
			["Content-Type"] = "application/json",
			["Content-Length"] = payload:len()
		},
		source = ltn12.source.string(payload),
		sink = ltn12.sink.table(response_body)
	}

	if not string.find(table.concat(response_body), "\"cgiresult\":%s+1%s+") then
		luardb.set ('service.email.client.conf.TestEmailResult', (index or '') .. ', Failure')
	else
		luardb.set ('service.email.client.conf.TestEmailResult', (index or '') .. ', Success')
	end
end

return {

-- string:readwrite
-- From address list seperated by ';'
-- rdb variable used: service.email.client.conf.addr_fm
	[subROOT .. 'From'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.addr_fm')
			if not result then return CWMP.Error.InternalError end

			return 0, result
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end
			value = string.trim(value)
			local addrList=value:explode(';')

			for _, addr in ipairs(addrList) do
				addr=string.trim(addr)
				if addr ~= '' and not isValidEmailAddr(addr) then return CWMP.Error.InvalidArguments end
			end

			setIfChanged('service.email.client.conf.addr_fm', value)

			return 0
		end
	},

-- string:readwrite
-- CC address list seperated by ';'
-- rdb variable used: service.email.client.conf.addr_cc
	[subROOT .. 'CC'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.addr_cc')
			if not result then return CWMP.Error.InternalError end

			return 0, result
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end
			value = string.trim(value)
			local addrList=value:explode(';')

			for _, addr in ipairs(addrList) do
				addr=string.trim(addr)
				if addr ~= '' and not isValidEmailAddr(addr) then return CWMP.Error.InvalidArguments end
			end

			setIfChanged('service.email.client.conf.addr_cc', value)

			return 0
		end
	},

-- string:readwrite
-- SMTP Server address
-- rdb variable used: service.email.client.conf.server_addr
	[subROOT .. 'EmailServerAddr'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.server_addr')
			if not result then return CWMP.Error.InternalError end

			return 0, result
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end
			setIfChanged('service.email.client.conf.server_addr', value)
			return 0
		end
	},

-- string:readwrite
-- SMTP server port, Available range from webui(email_client.html): 1~65534
-- rdb variable used: service.email.client.conf.server_port
	[subROOT .. 'EmailServerPort'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.server_port') or ''
			if result == '' or not tonumber(result) then result='' end

			return 0, result
		end,
		set = function(node, name, value)
			if not value or not tonumber(value) then return CWMP.Error.InvalidArguments end
			if tonumber(value) < 1 or tonumber(value) > 65535 then return CWMP.Error.InvalidArguments end

			setIfChanged('service.email.client.conf.server_port', value)
			return 0
		end
	},

-- string:readwrite
-- Available Value: None|SSL|STARTTLS, case-insensitive
-- rdb variable used: service.email.client.conf.security  [none|ssl|tls]
	[subROOT .. 'Encryption'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.security') or ''
			local lower_result = result:lower()
			local Encryp={
				['none'] = "None",
				['ssl']  = "SSL",
				['tls']  = "STARTTLS"
			}

			result = Encryp[lower_result] or "None"

			return 0, result
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end
			value = string.trim(value)

			local lower_value = value:lower()
			local Encryp={
				['none'] = "none",
				['ssl']  = "ssl",
				['starttls']  = "tls"
			}

			if not Encryp[lower_value] then return CWMP.Error.InvalidArguments end

			setIfChanged('service.email.client.conf.security', Encryp[lower_value])
			return 0
		end
	},

-- bool:readwrite
-- 
-- rdb variable used: service.email.client.conf.useauth  [1|0]
	[subROOT .. 'EnableAuth'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.useauth')
			if not result or result ~= '1' then 
				return 0, '0'
			end

			return 0, '1'

		end,
		set = function(node, name, value)
			local internalBool = convertInternalBoolean(value)
			if not internalBool then return CWMP.Error.InvalidArguments end

			setIfChanged('service.email.client.conf.useauth', internalBool)
			return 0

		end
	},

-- string:readwrite
-- address list seperated by ';'
-- rdb variable used: service.email.client.conf.TestRecipient
	[subROOT .. 'EmailTestRecipient'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.TestRecipient') or ''
			return 0, result
		end,
		set = function(node, name, value)
			if not value then return CWMP.Error.InvalidArguments end
			local addrList=value:explode(';')

			for _, addr in ipairs(addrList) do
				addr=string.trim(addr)
				if addr ~= '' and not isValidEmailAddr(addr) then return CWMP.Error.InvalidArguments end
			end

			setIfChanged('service.email.client.conf.TestRecipient', value)

			return 0
		end
	},

-- uint:writeonly
-- Test email trigger:If setting to arbitrary index numder, test email is transmitted. And the result is reported to "TestEmailStatus" parameter with its index number.
-- rdb variable used:
	[subROOT .. 'TestEmailTrigger'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, 0
		end,
		set = function(node, name, value)
			if not value or not tonumber(value) or tonumber(value) < 0 then return CWMP.Error.InvalidArguments end

			if client:isTaskQueued('postSession', sendTestEmail, value) ~= true then
				client:addTask('postSession', sendTestEmail, false, value)
			end
			return 0
		end
	},

-- string:readonly
-- Status of last test email formatted "Index number, Success|Failure"
-- rdb variable used:
	[subROOT .. 'TestEmailResult'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('service.email.client.conf.TestEmailResult') or ''
			return 0, result
		end,
		set = function(node, name, value)
			return CWMP.Error.ReadOnly
		end
	},
}

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
Logger.addSubsystem('Daemon')

local urlModule = require('socket.url')
require('variants')

Daemon = {}

function Daemon.readStringFromFile(filename)
	local file, msg, errno = io.open(filename, 'r')
	assert(file, 'Could not open "' .. filename .. '": (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
	local line = file:read('*l')
	file:close()
	return line
end

function Daemon.readIntFromFile(filename)
	local file, msg, errno = io.open(filename, 'r')
	assert(file, 'Could not open "' .. filename .. '": (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
	local n = math.floor(file:read('*n'))
	file:close()
	return n
end

function Daemon.readEntireFile(filename)
	local file, msg, errno = io.open(filename, 'r')
	assert(file, 'Could not open "' .. filename .. '": (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
	local data = file:read('*a')
	file:close()
	return data
end

local function popenAlternative(command)
	local tmpFile = os.tmpname()
	local cmd = command .. ' > ' .. tmpFile
	local ret = os.execute(cmd)
	assert(ret == 0, 'Failed to execute "' .. cmd .. '": ' .. ret)
	local file = io.open(tmpFile, 'r')
	local data = file:read('*a')
	file:close()
	os.remove (tmpFile)
	return data
end

function Daemon.readCommandOutput(cmd)
	local pipe, msg, errno = io.popen(cmd, 'r')
--	assert(pipe, 'Could not execute "' .. cmd .. '": (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
	if not pipe then
		return popenAlternative(cmd)
	end
	local data = pipe:read('*a')
	pipe:close()
	return data
end

function Daemon.getRandomString(len)
	len = len or 16
	local file, msg, errno = io.open(conf.fs.randomSource, 'r')
	assert(file, 'Can not open randomSource "' .. tostring(conf.fs.randomSource) .. '": (' .. tostring(errno) .. ') ' .. tostring(msg) .. '.')
	local data = file:read((len / 2) + 1)
	local ret = ''
	for i = 1, len/2 do
		ret = ret .. string.format('%02X', data:byte(i))
	end
	if ret:len() < len then
		ret = ret .. string.format('%02X', data:byte(len/2 + 1))
	end
	file:close()
	return ret
end

function Daemon.flagOR(flagsA, flagsB)
	local flags = {}
	for c in flagsA:gfind('.') do
		flags[c] = c
	end
	for c in flagsB:gfind('.') do
		flags[c] = c
	end
	ret = ''
	for c in pairs(flags) do
		ret = ret .. c
	end
	return ret
end


function Daemon.fetchURLToFile(url, file, username, password)
	if not url then return true, "Failure on Given URL" end
	url = url:trim()

	local urlTbl = urlModule.parse(url)

	-- If username and password are given as separate parameters,
	-- we need to perform percent encoding on them for reserved characters.
	-- If they are included in url, they should already have been encoded.
	-- Reference: http://www.ietf.org/rfc/rfc3986.txt
	if username then username = urlModule.escape(username) end
	if password then password = urlModule.escape(password) end

	if urlTbl.user and username and urlTbl.user ~= username then
		return true, 'Given Username does not match'
	end

	if urlTbl.password and password and urlTbl.password ~= password then
		return true, 'Given Password does not match'
	end

	if username then urlTbl.user = username end
	if password then urlTbl.password = password end

	local cmdUrl = urlModule.build(urlTbl)

	--[[
		This was here to unescape the escaped URL. See TT#6334.
		However, after some comprehensive testing, this is no longer needed.
		Tested versions: GNU Wget 1.14, curl 7.35.0.
		Tested protocols: HTTP, HTTPS, FTP
		Findings:
		1) wget works for all protocols with both escaped and unescaped url.
		2) curl works for FTP with both escaped or unescaped url.
		3) curl works for HTTP/HTTPS with escaped url, but fails with unescaped url.
		This means if we always properly escape the url before passing to wget and curl, we will be fine.
		So, this part of code is now commented out.

	if urlTbl.user then
		local pre, suf = cmdUrl:match("(.-@)(.*)")
		cmdUrl=pre .. urlModule.unescape(suf)
	else
		cmdUrl=urlModule.unescape(cmdUrl)
	end
	--]]

	-- This is to escape 4 special characters ( ", $, `, \ ) for CLI.
	-- The result must be double quoted before passing to CLI.
	-- Note that $ is a magic character in lua pattern and should be % escaped.
	local specialPattern = '(["%$`\\])'
	cmdUrl = cmdUrl:gsub(specialPattern, '\\%1')
	file = file:gsub(specialPattern, '\\%1')

	if conf.transfer.wgetBinary and variants.V_CUSTOM_FEATURE_PACK ~= "Santos" then
		-- Should use libcurl I think, better error management too...
		local cmd = conf.transfer.wgetBinary
		local option = ' -O "' .. file .. '"'

		if urlTbl.scheme:find('https') then
			option = option .. ' --no-check-certificate'
		end
		cmd = cmd .. option .. ' "' .. cmdUrl .. '"'

		Logger.log('Daemon', 'notice', 'Daemon.fetchURLToFile cmd=[' .. cmd .. ']')

		local ret = os.execute(cmd)
		if ret ~= 0 then
			return true, 'Executing "' .. cmd .. '" failed: ' .. ret
		end

	elseif conf.transfer.curlBinary then
		local cmd = conf.transfer.curlBinary
		if urlTbl.host and urlTbl.host:match("^%[.+%]$") then
			-- host is a numerical IPv6 address, disable URL globbing to avoid curl error 3
			cmd = cmd .. ' -g'
		end

		-- Following HTTP 302 redirects on "Download" method
		if conf.net.download_redirects then
			cmd = cmd .. ' --location'
		end

		if variants.V_CUSTOM_FEATURE_PACK == "Santos" then
			-- force connection from wwanX, skip certificate checking
			local iface = luardb.get('tr069.server.current_interface')
			cmd = cmd .. ' -k --interface ' .. iface  .. ' -s -o "' .. file .. '" "' .. cmdUrl .. '"'
		else
			if conf.net.ssl_version == 6 then
				cmd = cmd .. ' --tlsv1.2' -- tls v1.2 and upper
			elseif conf.net.ssl_version == 7 then
				cmd = cmd .. ' --tlsv1.3' -- tls v1.3 and upper
			else
				-- require: TLSv1 (-1) by default
				cmd = cmd .. ' -1'
			end

			-- require: --noverifyhost (curl patch) unless specified otherwise
			if not conf.net.ssl_verify_host then
				cmd = cmd .. ' --noverifyhost'
			end

			if type(conf.net.rate_limit) == 'string' then
				cmd = cmd .. ' --limit-rate ' .. conf.net.rate_limit
			end

			-- If ca certificate file is specified, append it.
			if type(conf.net.ca_cert) == 'string' then
				cmd = cmd .. ' --cacert ' .. conf.net.ca_cert
			end
			-- CA directory that contains individual CA certs
			if type(conf.net.ca_path) == 'string' then
				cmd = cmd .. ' --capath ' .. conf.net.ca_path
			end
			if type(conf.net.client_cert) == 'string' then
				cmd = cmd .. ' --cert ' .. conf.net.client_cert
			end
			if type(conf.net.client_key) == 'string' then
				cmd = cmd .. ' --key ' .. conf.net.client_key
			end
			-- do not restrain ciphers unless explicitly requested
			if type(conf.net.ssl_cipher_list) == 'string' then
				cmd = cmd .. ' --ciphers ' .. conf.net.ssl_cipher_list
			end
			if type(conf.rdb.downloadTimeout) == 'string' then
				cmd = cmd .. ' --max-time ' .. tonumber(luardb.get(conf.rdb.downloadTimeout))
			end
			cmd = cmd .. ' -s -o "' .. file .. '" "' .. cmdUrl .. '"'
		end

		-- '-f' option will prevent curl from outputting HTTP server error and return error 22 with exit status.
		-- Without this flag, when an HTTP server fails to download target file,
		-- it outputs an HTML document stating the failure to the file given via "-o" option
		-- and returns exit status 0(success).
		--
		-- '-w "%{http_code}"' is to redirect status code returned from file server(FTP/HTTP/HTTPS) to stdout.
		-- To get more details of file server status code, refer to belows
		-- HTTP -> https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
		-- FTP  -> https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes
		local tmpFile = os.tmpname()
		cmd = cmd .. ' -f -w "%{http_code}" > ' .. tmpFile

		Logger.log('Daemon', 'notice', 'Daemon.fetchURLToFile cmd=[' .. cmd .. ']')

		local ret = os.execute(cmd)

		if ret ~= 0 then
			local file = io.open(tmpFile, 'r')
			local errno = file:read('*a')
			file:close()
			return true, 'Executing "' .. cmd .. '" failed: exit status=' .. ret .. ', server error code=' .. (errno or 'NIL')
		end
		os.remove(tmpFile)
--]]
	else
		return true, 'Error: Do not have a handler'
	end
end

function Daemon.uploadFileToURL(url, localFile, username, password)
	if not conf.transfer.curlBinary then return true, "Do not support File uploading" end

	local urlTbl = urlModule.parse(url)

	if urlTbl.user and username and urlTbl.user ~= username then
		return true, 'Given Username does not match'
	end

	if urlTbl.password and password and urlTbl.password ~= password then
		return true, 'Given Password does not match'
	end

	if username then urlTbl.user = username end

	if password then urlTbl.password = password end

	if not urlTbl.path then urlTbl.path = "/" end

	-- username and password have to be passed in via -u option so that special characters are preserved/properly encoded
	local userinfo = ""
	if urlTbl.user then
		userinfo = " -u '" .. urlTbl.user
		if urlTbl.password then
			userinfo = userinfo .. ":" .. urlTbl.password
		end
		userinfo = userinfo .. "'"
	end

	local hostport = urlTbl.host
	if not hostport then
		return true, 'No host is specified'
	end
	if urlTbl.port then
		hostport = hostport .. ":" .. urlTbl.port
	end

	local cmd = conf.transfer.curlBinary
	if urlTbl.host:match("^%[.+%]$") then
		-- host is a numerical IPv6 address, disable URL globbing to avoid curl error 3
		cmd = cmd .. ' -g'
	end

	if variants.V_CUSTOM_FEATURE_PACK == "Santos" then
		-- force connection from wwanX, skip certificate checking
		local iface = luardb.get('tr069.server.current_interface')
		cmd = cmd ..  ' -k --interface ' .. iface .. ' -f -s -T ' .. localFile .. userinfo .. ' ' .. urlTbl.scheme .. '://' .. hostport .. urlTbl.path
	else
		cmd = cmd ..  ' -1'
		if not conf.net.ssl_verify_host then
			cmd = cmd .. ' --noverifyhost'
		end
		-- If ca certificate file is specified, append it.
		if type(conf.net.ca_cert) == 'string' then
			cmd = cmd .. ' --cacert ' .. conf.net.ca_cert
		end
		-- CA directory that contains individual CA certs
		if type(conf.net.ca_path) == 'string' then
			cmd = cmd .. ' --capath ' .. conf.net.ca_path
		end
		if type(conf.net.client_cert) == 'string' then
			cmd = cmd .. ' --cert ' .. conf.net.client_cert
		end
		if type(conf.net.client_key) == 'string' then
			cmd = cmd .. ' --key ' .. conf.net.client_key
		end
		-- do not restrain ciphers unless explicitly requested
		if type(conf.net.ssl_cipher_list) == 'string' then
			cmd = cmd .. ' --ciphers ' .. conf.net.ssl_cipher_list
		end
		cmd = cmd .. ' -f -s -T ' .. localFile .. userinfo .. ' ' .. urlTbl.scheme .. '://' .. hostport .. urlTbl.path
	end

	Logger.log('Daemon', 'notice', 'Daemon.uploadFileToURL cmd=[' .. cmd .. ']')

	local ret = os.execute(cmd)

	if ret ~= 0 then
		return true, 'Executing "' .. cmd .. '" failed: errno=' .. ret
	end
end

local function isLeapYear(year)
	year = tonumber(year)
	return year % 4 == 0 and (year % 100 ~= 0 or year % 400 == 0)
end

-- number of days before a month
local daysToMonth = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 }
-- number of days since January 1 of the year
local function daysOfYear(year, month, day)
	month = tonumber(month)
	day = tonumber(day)
	local days = daysToMonth[month] + day - 1
	if month > 2 and isLeapYear(year) then
		days = days + 1
	end
	return days
end

-- convert a date/time in UTC to epoch seconds as a string
function Daemon.utcTimeToEpoch(year, month, day, hour, minute, second)
	if tonumber(year) < 1970 then return '0' end
	--[[
		We cannot simply use os.time(date) to convert a UTC datetime
		string to epoch seconds, since it always treats date as local
		time rather than UTC.
		e.g. os.time({year=1970, month=1, day=1, hour=0, min=0, sec=0})
		returns -36000 at a timezone of +10:00.
		There are two options to get around this:
		Option 1) add back an offset for a correct UTC conversion:
		e.g.
		local epoch = {
		    year = 1970, month = 1, day = 1,
		    hour = 0, min = 0, sec = 0, isdst = false
		}
		local date = {
		    year = year, month = month, day = day,
		    hour = hour, min = minute, sec = second, isdst = false
		}
		return tostring(os.time(date) - os.time(epoch))
		Note: in the above, isdst=false is necessary to deal with
		daylight saving correctly.
		However, this fails when there is a timezone change between
		epoch and date. For example, Singapore time zone changed on
		1982-01-01 from +7:30 to +8:00. So if the system time zone is
		set to Asia/Singapore, this option will not work correctly.

		Option 2) direct calculation
		sec + min*60 + hour*3600 + yday*86400 + (year-1970)*31536000
		+ (year-1969)/4*86400 - (year-1901)/100*86400
		+ (year-1601)/400*86400
		In the above, yday is days since Jan 1 of the year, and all
		calculations are integer based (math.floor)
		Ref: http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
		This option does not depend on system timezone at all.
		So we implement it here.
	--]]
	return tostring(second + minute * 60 + hour * 3600
						+ daysOfYear(year, month, day) * 86400
						+ (year - 1970) * 31536000
						+ math.floor((year - 1969) / 4) * 86400
						- math.floor((year - 1901) / 100) * 86400
						+ math.floor((year - 1601) / 400) * 86400)
end

return Daemon

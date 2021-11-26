require('CWMP.Error')
require('Logger')

Logger.addSubsystem('cubic')
local subROOT = conf.topRoot .. '.X_NETCOMM.'

local getSystemMode = function ()

	local connstatus= luardb.get('wlan.0.sta.connStatus')
	if connstatus and connstatus == "Connected" then return 4, "WiFi" end
	local wwansysmod = luardb.get('wwan.0.system_network_status.system_mode')

	if wwansysmod and wwansysmod:lower() == 'umts' then return 3, wwansysmod:lower() end
	if wwansysmod and wwansysmod:lower() == 'hsdpa' then return 3, wwansysmod:lower() end
	if wwansysmod and wwansysmod:lower() == 'hsupa' then return 3,wwansysmod:lower() end
	if wwansysmod and wwansysmod:lower() == 'hspa' then return 3, wwansysmod:lower() end
	if wwansysmod and wwansysmod:lower() == 'lte' then return 3, wwansysmod:lower() end

	if wwansysmod and wwansysmod:lower() == 'edge' then return 2, wwansysmod:lower() end

	if wwansysmod and wwansysmod:lower() == 'gsm' then return 1, wwansysmod:lower() end
	if wwansysmod and wwansysmod:lower() == 'gprs' then return 1, wwansysmod:lower() end

	return 0, "Not Avaiable"
end

local ScanDirForFirmware =function (directory)
    local i, t, popen = 0, {}, io.popen
    for filename in popen('ls -a "'..directory..'"'):lines() do
	if string.match(filename,".*%.cdi%s*$") or string.match(filename,".*%.usf%s*$") then
       		i = i + 1
        	t[i] = filename
	end
    end
    return t
end

return {

	[subROOT .. 'AGStatus.AGStatus'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			local sn = luardb.get('uboot.sn')
			if not sn or sn == '' then sn='12345678' end

			local msgtime = os.date('%d/%m/%Y %H:%M:%S')
			if not msgtime or msgtime == '' then msgtime = '00/00/1970 00:00:00' end

			local system_mode, wwan_mode=getSystemMode()
			local sigQualityRssi = 0
			local sigQualityBer = 0
			if system_mode == 4 then
				sigQualityRssi = 99
				sigQualityBer = 99
			else
				local dbmSigStrength = luardb.get('wwan.0.radio.information.signal_strength')
				if not dbmSigStrength or dbmSigStrength == '' then sigQualityRssi = 99
				else
					local rssi = dbmSigStrength:match("[+-]?(%d+)dBm")
					if not rssi then sigQualityRssi = 99
					else
						rssi=tonumber(rssi)
						if rssi > 112 then
							sigQualityRssi = 0
						elseif rssi < 52 then
							sigQualityRssi = 31
						else
							sigQualityRssi = (113 - rssi)/2
						end
					end
				end
				local rdbBer = luardb.get('wwan.0.radio.information.signal_strength.bars')
				if not rdbBer or rdbBer == '' then sigQualityBer = 99
				else
					sigQualityBer = tonumber(rdbBer)
				end
			end
			wwan_mode=string.upper(wwan_mode)
			-- Concatonate
			local AGStatusMsg=" SN:"..sn.."; System Mode:" .. system_mode .."-".. wwan_mode .. "; Signal Quality RSSI:" .. sigQualityRssi .. "; Signal Quality BER:" .. sigQualityBer .. "; AG Status update time:".. msgtime
			return 0, AGStatusMsg

		end,
		set = function(node, name, value)
			return 0
		end
	},

	[subROOT .. 'UpgradeManagement.FirmwareUploaded'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = ''
			local fwFileNames = ScanDirForFirmware("/opt/cdcs/upload")
			local result=table.concat(fwFileNames, ", ")
			return 0, result

		end,
		set = function(node, name, value)
			return 0
		end
	},

	[subROOT .. 'UpgradeManagement.FirmwareInstall'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''

		end,
		set = function(node, name, value)
			-- check firmware name
			local fwFileNameList = ScanDirForFirmware("/opt/cdcs/upload")
			local fwName=value
			if not table.contains(fwFileNameList, fwName) then return CWMP.Error.InvalidParameterValue end
			local cmd= "install_file " .. "/opt/cdcs/upload/"..fwName
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InternalError, "Firmware Upgrade Failed" end
			return 0
		end
	},

	[subROOT .. 'UpgradeManagement.FirmwareDelete'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			-- check firmware name
			local fwFileNameList = ScanDirForFirmware("/opt/cdcs/upload")
			local fwName=value
			if not table.contains(fwFileNameList, fwName) then return CWMP.Error.InvalidParameterValue end
			local cmd= "rm -f " .. "/opt/cdcs/upload/"..fwName
			local result = os.execute(cmd)
			if not result or result ~= 0 then return CWMP.Error.InternalError, "Firmware Delete Failed" end
			return 0
		end
	},

}

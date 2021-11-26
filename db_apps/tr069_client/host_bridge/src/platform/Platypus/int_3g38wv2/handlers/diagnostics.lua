
function UDPEchoConfig_Enable ()
	luardb.set('UDPEchoConfig.Enable', "1")
end

function UDPEchoConfig_Disable ()
	luardb.set('UDPEchoConfig.Enable', "0")
end

return {
----- [start] object IPPingDiagnostics --------------------------------------------------
-- 	['**.IPPingDiagnostics.'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			return 0
-- 		end,
-- 		set = function(node, name, value)
-- 			return 0
-- 		end
-- 	},
----- [ end ] object IPPingDiagnostics --------------------------------------------------

----- [start] object DownloadDiagnostics ------------------------------------------------
	['**.DownloadDiagnostics.Interface'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.DLinterface')

			if retVal == nil then return "" end

			retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			luanvramDB.set('tr069.tr143.DLinterface', value)
			return 0
		end
	},
	['**.DownloadDiagnostics.DownloadURL'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.DLURL')

			if retVal == nil then return "" end

			retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			luanvramDB.set('tr069.tr143.DLURL', value)
			return 0
		end
	},
	['**.DownloadDiagnostics.DSCP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.DLDSCP')

			if retVal == nil then return '0' end

			retVal = tonumber(retVal)

			if retVal == nil or retVal < 0 or retVal > 63 then return '0' end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			value = tonumber(value)

			if value == nil or value < 0 or value > 63 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr143.DLDSCP', value)
			return 0
		end
	},
	['**.DownloadDiagnostics.EthernetPriority'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.DLEtherPri')

			if retVal == nil then return '0' end

			retVal = tonumber(retVal)

			if retVal == nil or retVal < 0 or retVal > 7 then return '0' end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			value = tonumber(value)

			if value == nil or value < 0 or value > 7 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr143.DLEtherPri', value)
			return 0
		end
	},
----- [ end ] object DownloadDiagnostics ------------------------------------------------

----- [start] object UploadDiagnostics --------------------------------------------------
	['**.UploadDiagnostics.Interface'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.ULinterface')

			if retVal == nil then return "" end

			retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			luanvramDB.set('tr069.tr143.ULinterface', value)
			return 0
		end
	},
	['**.UploadDiagnostics.UploadURL'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.ULURL')

			if retVal == nil then return "" end

			retVal = string.gsub(retVal:gsub("^%s+", ""), "%s+$", "")

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return 0 end

			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			luanvramDB.set('tr069.tr143.ULURL', value)
			return 0
		end
	},
	['**.UploadDiagnostics.DSCP'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.ULDSCP')

			if retVal == nil then return '0' end

			retVal = tonumber(retVal)

			if retVal == nil or retVal < 0 or retVal > 63 then return '0' end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			value = tonumber(value)

			if value == nil or value < 0 or value > 63 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr143.ULDSCP', value)
			return 0
		end
	},
	['**.UploadDiagnostics.EthernetPriority'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.ULEtherPri')

			if retVal == nil then return '0' end

			retVal = tonumber(retVal)

			if retVal == nil or retVal < 0 or retVal > 7 then return '0' end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			value = tonumber(value)

			if value == nil or value < 0 or value > 7 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr143.ULEtherPri', value)
			return 0
		end
	},
	['**.UploadDiagnostics.TestFileLength'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luanvramDB.get('tr069.tr143.ULFileLen')
			if retVal == nil then return '1024' end

			retVal = tonumber(retVal)

			if retVal == nil or retVal < 0 then return '1024' end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end

			value = tonumber(value)

			if value == nil or value < 0 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('tr069.tr143.ULFileLen', value)
			return 0
		end
	},
----- [ end ] object UploadDiagnostics --------------------------------------------------

----- [start] object UDPEchoConfig ------------------------------------------------------
	['**.UDPEchoConfig.Enable'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.Enable')

			if retVal == nil then return '0' end

			if retVal == "1" or retVal == "0" then return retVal end

			return '0'
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if value == "0" or value == "1" then
				if value == "1" then
					dimclient.callbacks.register('postSession', UDPEchoConfig_Enable)
				else
					dimclient.callbacks.register('postSession', UDPEchoConfig_Disable)
				end
			else
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},
	['**.UDPEchoConfig.Interface'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.Interface')

			if retVal == nil then return "" end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			luanvramDB.set('UDPEchoConfig.Interface', value)
			luardb.set('UDPEchoConfig.Interface', value)
			return 0
		end
	},
	['**.UDPEchoConfig.SourceIPAddress'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.SourceIPAddress')

			if retVal == nil then return "" end

			if not isValidIP4(retVal) then return "" end

			return retVal
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if not isValidIP4(value) then return cwmpError.InvalidParameterValue end

			luanvramDB.set('UDPEchoConfig.SourceIPAddress', value)
			luardb.set('UDPEchoConfig.SourceIPAddress', value)
			return 0
		end
	},
	['**.UDPEchoConfig.UDPPort'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.UDPPort')

			if retVal == nil then return '8088' end
			
			local numVal = tonumber(retVal)

			if numVal == nil then return '8088' end

			if numVal < 0 or numVal > 65535 then return '8088' end

			return tostring(numVal)
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			local numVal = tonumber(value)

			if numVal == nil then return cwmpError.InvalidParameterValue end
			if numVal < 0 or numVal > 65535 then return cwmpError.InvalidParameterValue end

			luanvramDB.set('UDPEchoConfig.UDPPort', numVal)
			luardb.set('UDPEchoConfig.UDPPort', numVal)
			return 0
		end
	},
	['**.UDPEchoConfig.EchoPlusEnabled'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.EchoPlusEnabled')

			if retVal == nil then return '0' end

			if retVal == "1" or retVal == "0" then return retVal end

			return '0'
		end,
		set = function(node, name, value)
			if value == nil then return cwmpError.InvalidParameterValue end
			value = string.gsub(value:gsub("^%s+", ""), "%s+$", "")

			if value == "0" or value == "1" then
				luanvramDB.set('UDPEchoConfig.EchoPlusEnabled', value)
				luardb.set('UDPEchoConfig.EchoPlusEnabled', value)
			else
				return cwmpError.InvalidParameterValue
			end

			return 0
		end
	},

	['**.UDPEchoConfig.PacketsReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.PacketsReceived')

			if retVal == nil then return '0' end

			local numVal = tonumber(retVal)

			if numVal == nil then return '0' end

			return tostring(numVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.UDPEchoConfig.PacketsResponded'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.PacketsResponded')

			if retVal == nil then return '0' end

			local numVal = tonumber(retVal)

			if numVal == nil then return '0' end

			return tostring(numVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.UDPEchoConfig.BytesReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.BytesReceived')

			if retVal == nil then return '0' end

			local numVal = tonumber(retVal)

			if numVal == nil then return '0' end

			return tostring(numVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.UDPEchoConfig.BytesResponded'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.BytesResponded')

			if retVal == nil then return '0' end

			local numVal = tonumber(retVal)

			if numVal == nil then return '0' end

			return tostring(numVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.UDPEchoConfig.TimeFirstPacketReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.TimeFirstPacketReceived')

			if retVal == nil then return "" end

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
	['**.UDPEchoConfig.TimeLastPacketReceived'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = luardb.get('UDPEchoConfig.TimeLastPacketReceived')

			if retVal == nil then return "" end

			return retVal
		end,
		set = function(node, name, value)
			return 0
		end
	},
----- [ end ] object UDPEchoConfig ------------------------------------------------------
}

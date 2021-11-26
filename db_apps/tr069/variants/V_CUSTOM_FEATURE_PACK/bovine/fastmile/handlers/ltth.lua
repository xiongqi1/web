--[[
This script handles Nokia LTTH objects/parameters under Device.X_NOKIA_COM_LTTH_ODU.

  antenna.
  cell.
  config.
  device.
  measurements.
  system.

Copyright (C) 2016 NetComm Wireless Limited.
--]]

require("CWMP.Error")
require("Logger")
local logSubsystem = 'NOKIA_LTTH'
Logger.addSubsystem(logSubsystem)
local subRoot = conf.topRoot

return {
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.antenna.Mode'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - antenna.Mode");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.antenna.hAngle'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - antenna.hAngle");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.antenna.vAngle'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - antenna.vAngle");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.16_QAM_Rate'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.16_QAM_Rate");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.64_QAM_Rate'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.64_QAM_Rate");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.CQI'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.CQI");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.DL_PDCP_TP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.DL_PDCP_TP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.DL_PRB_Num'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.DL_PRB_Num");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Id'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Id");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.MCS'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.MCS");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Pathloss'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Pathloss");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.QPSK_Rate'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.QPSK_Rate");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.RSRP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.RSRP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.RSRQ'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.RSRQ");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.SINR'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.SINR");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.SNR'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.SNR");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Transmission_mode'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Transmission_mode");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Transmission_mode1_Rate'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Transmission_mode1_Rate");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Transmission_mode2_Rate'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Transmission_mode2_Rate");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Tx_Power_PUCCH'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Tx_Power_PUCCH");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.Tx_Power_PUSCH'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.Tx_Power_PUSCH");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.UL_PDCP_TP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.UL_PDCP_TP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.cell.UL_PRB_Num'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - cell.UL_PRB_Num");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.antennaFrequency'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.antennaFrequency");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.cellId'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.cellId");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.cellPci'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.cellPci");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
		set = function(node, name, value)
			local result = os.execute("/usr/local/bin/ltthcwrite config.cellPci " .. value);
			if result ~= 0 then result = CWMP.Error.InvalidParameterValue end
			return result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.cellSettings'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.cellSettings");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
		set = function(node, name, value)
			local result = os.execute("/usr/local/bin/ltthcwrite config.cellSettings " .. value);
			if result ~= 0 then result = CWMP.Error.InvalidParameterValue end
			return result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.instructedTiltAngle'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.instructedTiltAngle");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.config.variantVersionName'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - config.variantVersionName");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.device.tz'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - device.tz");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
		set = function(node, name, value)
			local result = os.execute("/usr/local/bin/ltthcwrite device.tz " .. value);
			if result ~= 0 then result = CWMP.Error.InvalidParameterValue end
			return result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.measurements.badSignal'] = {
		set = function(node, name, value)
			local result = os.execute("/usr/local/bin/ltthcwrite measurements.badSignal " .. value);
			if result ~= 0 then result = CWMP.Error.InvalidParameterValue end
			return result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellCQI'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellCQI");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellDL_PDCP_TP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellDL_PDCP_TP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellDL_PRB_Num'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellDL_PRB_Num");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellPathloss'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellPathloss");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellRSRP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellRSRP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellRSRQ'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellRSRQ");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellSINR'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellSINR");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellSNR'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellSNR");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellTx_Power_PUCCH'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellTx_Power_PUCCH");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellTx_Power_PUSCH'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellTx_Power_PUSCH");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellUL_PDCP_TP'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellUL_PDCP_TP");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.averageCellUL_PRB_Num'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.averageCellUL_PRB_Num");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '-1' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.connectionLosses'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.connectionLosses");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.networkDownTime'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.networkDownTime");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.networkUpTime'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.networkUpTime");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			if not tonumber(result) then result = '0' end
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.startFailureRootCause'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.startFailureRootCause");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
	[subRoot .. '.X_NOKIA_COM_LTTH_ODU.system.state'] = {
		get = function(node, name)
			local handle = io.popen("/usr/local/bin/ltthsget - system.state");
			local result = handle:read("*a");
			handle:close();
			result = string.gsub(result, "\n$", "");
			return 0, result
		end,
	};
}

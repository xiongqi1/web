require("handlers.CGI_Iface")
require("handlers.hdlerUtil")
 
local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.SMS.'  --> If somebody change this variable, please verify index of pathBits[] in [subROOT .. 'Diagnostics.WhiteLists.InstanceLists.*'].

------------------local function prototype------------------
local buildCGIRespTbl
local clearCGIRespTbl
local SMS_WhiteList_watcher
local reg_cbVariable
local cb_setcbVariables
------------------------------------------------------------

-----------------------local variable-----------------------
local CGIRespTbl = nil
local cb_regTable = nil
------------------------------------------------------------

clearCGIRespTbl = function()
	CGIRespTbl = nil
end


-------------------- Struct of CGIRespTbl --------------------
-- CGIRespTbl = {
--	EnableErrorNoti="1"
--	EnableSetCmdAck="0"
--	FixedAckDestNo=""
--	FixedErrorNotiDestNo=""
--	MaxDiagSmsTxLimit="100"
--	MaxDiagSmsTxLimitPer="DAY"
--	MaxWlTxDstIdx="19"
--	SmsTxCnt="0"
--	UseFixedAckDest="0"
--	UseFixedErrorNotiDest="0"
--	UseWhiteList="1"
--	DiagUserNo[CGIRespTbl.MaxWlTxDstIdx] = {}
--	DiagPassword[CGIRespTbl.MaxWlTxDstIdx] = {}
--}
------------------------------------------------------------
buildCGIRespTbl = function()

	if not CGIRespTbl then 
		CGIRespTbl = {}
		CGIRespTbl.DiagUserNo = {}
		CGIRespTbl.DiagPassword = {}

		CGI_Iface.getCGIresponse(CGIRespTbl, '/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET')

		local maxOfwhitelists = CGIRespTbl.MaxWlTxDstIdx or 0
		maxOfwhitelists = tonumber(maxOfwhitelists) or 0

		for i=1, (maxOfwhitelists+1) do
			local keyOfDiagUserNo = 'DiagUserNo[' .. (i-1) .. ']'
			local keyOfDiagPassword = 'DiagPassword[' .. (i-1) .. ']'

			if CGIRespTbl[keyOfDiagUserNo] and CGIRespTbl[keyOfDiagUserNo] ~= '' then
				CGIRespTbl.DiagUserNo[i] = CGIRespTbl[keyOfDiagUserNo]
				CGIRespTbl.DiagPassword[i] = CGIRespTbl[keyOfDiagPassword]
			else
				break
			end
		end

		if client:isTaskQueued('cleanUp', clearCGIRespTbl) ~= true then
			client:addTask('cleanUp', clearCGIRespTbl)
		end
	end
end

SMS_WhiteList_watcher = function(task)
	local node = task.data
	local numOfWhiteList = 0

	for i, value in hdlerUtil.traverseRdbVariable{prefix='smstools.diagconf.diag_user_no', startIdx=0} do
		if value ~= '' then
			numOfWhiteList = numOfWhiteList + 1
		else
			break;
		end
	end

	if node.instance and node.instance ~= numOfWhiteList then
		buildCGIRespTbl()

		for _, instance in ipairs(node.children) do
			if instance.name ~= '0' then
				instance.parent:deleteChild(instance)
			end
		end

		local maxInstanceId = 0
		local numOfLists = #CGIRespTbl.DiagUserNo or 0

		for i=1, numOfLists do
			maxInstanceId = i
			node:createDefaultChild(i)
		end

		node.instance = maxInstanceId
	end
end

-------------------- Struct of CGIRespTbl --------------------
-- cb_regTable = {
--	EnableErrorNoti="1"
--	EnableSetCmdAck="0"
--	FixedAckDestNo=""
--	FixedErrorNotiDestNo=""
--	MaxDiagSmsTxLimit="100"
--	MaxDiagSmsTxLimitPer="DAY"
--	MaxWlTxDstIdx="19"
--	SmsTxCnt="0"
--	UseFixedAckDest="0"
--	UseFixedErrorNotiDest="0"
--	UseWhiteList="1"
--}
------------------------------------------------------------

reg_cbVariable = function (name, value)
	if not name then return end
	if not cb_regTable then cb_regTable = {} end

	cb_regTable[name] = value

	if client:isTaskQueued('postSession', cb_setcbVariables) ~= true then
		client:addTask('postSession', cb_setcbVariables, false, cb_regTable)
	end
end


--[[
	cmd_line="/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&";

	// send diag configuration via content type bacause it could be over 256 bytes which
	// is default limit of url length defined in mpr.h
	contents_body="UseWhiteList=\""+document.SMS.menuUseWhiteList.value+"\"&"+
		"EnableSetCmdAck=\""+document.SMS.menuEnableSetCmdAck.value+"\"&"+
		"UseFixedAckDest=\""+document.SMS.menuUseFixedAckDest.value+"\"&"+
		"FixedAckDestNo=\""+document.SMS.FixedAckDestNo.value+"\"&"+
		"EnableErrorNoti=\""+document.SMS.menuEnableErrorNoti.value+"\"&"+
		"UseFixedErrorNotiDest=\""+document.SMS.menuUseFixedErrorNotiDest.value+"\"&"+
		"FixedErrorNotiDestNo=\""+document.SMS.FixedErrorNotiDestNo.value+"\"&"+
		"MaxDiagSmsTxLimit=\""+document.SMS.MaxDiagSmsTxLimit.value+"\"&"+
		"MaxDiagSmsTxLimitPer=\""+document.SMS.MaxDiagSmsTxLimitPer.value+"\"&";

	for(i=0; i<=max_wl_tx_dst_idx; i++) {
		// check if password is all digits
		if (isNaN($("#DiagPassword"+i).val())) {
			validate_alert("", "sms warning30");
			return;
		}
		contents_body+="DiagUserNo"+i+"=\""+$("#DiagUserNo"+i).val()+"\"&"+
		"DiagPassword"+i+"=\""+$("#DiagPassword"+i).val()+"\"&";
	}
--]]

local cgi_argList = {
	EnableErrorNoti=true,
	EnableSetCmdAck=true,
	FixedAckDestNo=true,
	FixedErrorNotiDestNo=true,
	MaxDiagSmsTxLimit=true,
	MaxDiagSmsTxLimitPer=true,
	UseFixedAckDest=true,
	UseFixedErrorNotiDest=true,
	UseWhiteList=true,
}

cb_setcbVariables = function (task)
	local set_valueTbl = task.data
	local contents_body = ''
	local needConfSetting = false
	local cgi_setCommand = '/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET'

	for name, _ in pairs(cgi_argList) do
		if set_valueTbl[name] then 
			needConfSetting = true
			break;
		end
	end

	if needConfSetting then
		contents_body = ''

		buildCGIRespTbl()

		for name, _ in pairs(cgi_argList) do
			contents_body = contents_body .. '&' .. name .. '="' .. (set_valueTbl[name] or CGIRespTbl[name] or '') .. '"'
		end

		local loopCntOfwhiteList = CGIRespTbl.MaxWlTxDstIdx and tonumber(CGIRespTbl.MaxWlTxDstIdx) or 0

		for i=1, (loopCntOfwhiteList+1) do
			local keyOfDiagUserNo = 'DiagUserNo[' .. (i-1) .. ']'
			local keyOfDiagPassword = 'DiagPassword[' .. (i-1) .. ']'

			contents_body = contents_body .. '&' .. 'DiagUserNo' .. (i-1) .. '="' .. (CGIRespTbl[keyOfDiagUserNo] or '') .. '"&'
							.. 'DiagPassword' .. (i-1) .. '="' .. (CGIRespTbl[keyOfDiagPassword] or '') .. '"'
		end

		CGI_Iface.setValueToCGI(cgi_setCommand .. contents_body)
	end

	cb_regTable = nil
end

return {
--[[
-- bool: readwrite
	[subROOT .. 'path'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			return 0, ''
		end,
		set = function(node, name, value)
			return 0
		end
	},
--]]

-- bool: readwrite
-- Enable remote diagnostics 
-- GET/SET: rdb variable "smstools.conf.enable_remote_cmd"
-- Default: 1
	[subROOT .. 'Diagnostics.Enable_Remote_Diagnostics'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('smstools.conf.enable_remote_cmd') or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end
			luardb.set('smstools.conf.enable_remote_cmd', setVal)
			return 0
		end
	},

-- bool: readwrite
-- Authentication 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "UseWhiteList", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "UseWhiteList" ("smstools.diagconf.use_whitelist")
-- Default: 1
	[subROOT .. 'Diagnostics.Authentication'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.UseWhiteList or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('UseWhiteList', setVal)
			return 0
		end
	},

-- bool: readwrite
-- Set command acknowledgement SMS 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "EnableSetCmdAck", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "EnableSetCmdAck" ("smstools.diagconf.enable_set_cmd_ack")
-- Default: 0
	[subROOT .. 'Diagnostics.EnableSetCmdAck'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.EnableSetCmdAck or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('EnableSetCmdAck', setVal)
			return 0
		end
	},

-- bool: readwrite
-- Send acknowledgement SMS to 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "UseFixedAckDest", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "UseFixedAckDest" ("smstools.diagconf.use_fixed_ack_dest")
-- Default: 0
	[subROOT .. 'Diagnostics.UseFixedAckDest'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.UseFixedAckDest or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('UseFixedAckDest', setVal)
			return 0
		end
	},

-- string: readwrite
-- Fixed number for Command ack 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "FixedAckDestNo", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "FixedAckDestNo" ("smstools.diagconf.fixed_ack_dest_no")
-- Default: ""
	[subROOT .. 'Diagnostics.FixedAckDestNo'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.FixedAckDestNo or ''
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil
			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('FixedAckDestNo', setVal)
			return 0
		end
	},
--TODO: Set Handler
-- bool: readwrite
-- Get/set/exec command error SMS 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "EnableErrorNoti", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "EnableErrorNoti" ("smstools.diagconf.enable_error_noti")
-- Default: 1
	[subROOT .. 'Diagnostics.EnableErrorNoti'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.EnableErrorNoti or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('EnableErrorNoti', setVal)
			return 0
		end
	},

-- bool: readwrite
-- Send error SMS to 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "UseFixedErrorNotiDest", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "UseFixedErrorNotiDest" ("smstools.diagconf.use_fixed_error_noti_dest")
-- Default: 0
	[subROOT .. 'Diagnostics.UseFixedErrorNotiDest'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.UseFixedErrorNotiDest or '0'
			result = tostring(result) or '0'

			if result ~= '1' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('UseFixedErrorNotiDest', setVal)
		end
	},
--TODO: Set Handler
-- string: readwrite
-- Fixed number for error noti 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "FixedErrorNotiDestNo", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "FixedErrorNotiDestNo" ("smstools.diagconf.fixed_error_noti_dest_no")
-- Default: ""
	[subROOT .. 'Diagnostics.FixedErrorNotiDestNo'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.FixedErrorNotiDestNo or ''
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil
			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('FixedErrorNotiDestNo', setVal)
		end
	},
--TODO:
-- uint: readwrite
-- Maximum diagnostic SMS limit 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "MaxDiagSmsTxLimit", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "MaxDiagSmsTxLimit" ("smstools.diagconf.max_diag_sms_tx_limit")
-- Default: 100
-- Available Value: 1-100
	[subROOT .. 'Diagnostics.MaxDiagSmsTxLimit'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.MaxDiagSmsTxLimit or 100
			result = tonumber(result) or 100

			if result < 0 then result = 100 end

			return 0, tostring(result)
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalInteger{input=value, minimum=1, maximum=100}

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('MaxDiagSmsTxLimit', tostring(setVal))
			return 0
		end
	},
--TODO: Not Done
-- string: readwrite
-- messages per 
-- GET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_GET", "MaxDiagSmsTxLimitPer", SET: "/cgi-bin/sms.cgi?CMD=DIAG_CONF_SET&", "MaxDiagSmsTxLimitPer" ("smstools.diagconf.max_diag_sms_tx_limit_per")
-- Default: DAY
-- Available Value: [HOUR|DAY|WEEK|MONTH]
	[subROOT .. 'Diagnostics.MaxDiagSmsTxLimitPer'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = CGIRespTbl.MaxDiagSmsTxLimitPer or "DAY"

			if result ~= "HOUR" and result ~= "DAY" and result ~= "WEEK" and result ~= "MONTH" then
				result = "DAY" 
			end

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil
			if not setVal then return CWMP.Error.InvalidArguments end

			if setVal~='HOUR' and setVal~='DAY' and setVal~='WEEK' and setVal~='MONTH' then
				return CWMP.Error.InvalidArguments, 'Setting Value is not avaliable - Available Value: [HOUR|DAY|WEEK|MONTH]'
			end
			reg_cbVariable('MaxDiagSmsTxLimitPer', setVal)
			return 0
		end
	},


---------------------<START> White List-------------------------------------
-- TODO: Set Handler
-- uint: readonly
	[subROOT .. 'Diagnostics.WhiteLists.NumberOfList'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl()

			local result = #CGIRespTbl.DiagUserNo or '0'
			return 0, tostring(result)
		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO
-- object: readonly
	[subROOT .. 'Diagnostics.WhiteLists.InstanceLists'] = {
		init = function(node, name, value)
			buildCGIRespTbl()

			local maxInstanceId = 0
			local numOfLists = #CGIRespTbl.DiagUserNo or 0

			for i=1, numOfLists do
				maxInstanceId = i
				node:createDefaultChild(i)
			end

			node.instance = maxInstanceId

			if client:isTaskQueued('preSession', SMS_WhiteList_watcher) ~= true then
				client:addTask('preSession', SMS_WhiteList_watcher, true, node) -- persistent callback function
			end

			return 0
		end,
	},
-- TODO
-- object: readonly
	[subROOT .. 'Diagnostics.WhiteLists.InstanceLists.*'] = {
		init = function(node, name, value) return 0 end,
	},
-- TODO
	[subROOT .. 'Diagnostics.WhiteLists.InstanceLists.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local instanceId_idx = 8  --> should be changed, if "subROOT" variable is changed.
			local pathBits = name:explode('.')
			local instanceId = pathBits[instanceId_idx]

			if not instanceId then return 0 end

			instanceId = tonumber(instanceId)

			if not instanceId then 
				return CWMP.Error.InternalError, 'The instance does Not exist. Name: ' .. name
			end

			buildCGIRespTbl()

			if pathBits[instanceId_idx+1] == 'Destination_Num' then
				node.value = CGIRespTbl.DiagUserNo[instanceId] or ''
			elseif pathBits[instanceId_idx+1] == 'Password' then
				node.value = CGIRespTbl.DiagPassword[instanceId] or ''
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			return 0
		end
	},
---------------------< END > White List-------------------------------------
}

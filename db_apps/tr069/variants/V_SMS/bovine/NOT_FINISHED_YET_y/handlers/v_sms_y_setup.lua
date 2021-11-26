require("handlers.CGI_Iface")
require("handlers.hdlerUtil")

require("Logger")
Logger.addSubsystem('webui_Services_SMS')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.SMS.'

------------------local function prototype------------------
local buildCGIRespTbl
local clearCGIRespTbl
local reg_cbVariable
local cb_setcbVariables
------------------------------------------------------------

------------------local variable------------------
local CGIRespTbl = nil
local cb_regTable = nil
------------------------------------------------------------

clearCGIRespTbl = function()
	CGIRespTbl = nil
end

buildCGIRespTbl = function(uri)
	if not uri then return end

	if not CGIRespTbl then 
		CGIRespTbl = {}
		CGI_Iface.getCGIresponse(CGIRespTbl, uri)

		if client:isTaskQueued('cleanUp', clearCGIRespTbl) ~= true then
			client:addTask('cleanUp', clearCGIRespTbl)
		end
	end
end


--[[
  [ Javascript argument list ]
	* SMS_CONF_SET
cmd_line=	"/cgi-bin/sms.cgi?CMD=SMS_CONF_SET&"
contents_body=	"RedirMobile="+$("#RedirMobile").val()+"&"+
		"RedirEmail="+$("#SMS.RedirEmail").val()+"&"+
		"RedirTCP="+$("#RedirTCP").val()+"&"+
		"TCPport="+$("#TCPport").val()+"&"+
		"RedirUDP="+$("#RedirUDP").val()+"&"+
		"UDPport="+$("#UDPport").val()+"&"+
		"EncodingScheme="+$("#menuEncodingScheme").val()+"&"+
		"RemoteCommand="+$("#menuRemoteCommand").val()+"&"+
		"MsgsPerPage="+$("#msgsperpage").val()

	* SMS_ONOFF
cmd_line=	"/cgi-bin/sms.cgi?CMD=SMS_ONOFF&OnOff="
contents_body=	document.SMS.menuOnOff.value

	* SAVE_SMSC_ADDR
cmd_line=	"/cgi-bin/sms.cgi?CMD=SAVE_SMSC_ADDR&NEW_SMSC_ADDR="
contents_body=	encodeUrl(document.SMS.SmscAddrNo.value)

--]]

-- Argument list
local argsListOf_SMS_Conf_Set = {
	RedirMobile=true,
	RedirEmail=true,
	RedirTCP=true,
	TCPport=true,
	RedirUDP=true,
	UDPport=true,
	EncodingScheme=true,
	RemoteCommand=true,
	MsgsPerPage=true,
}

local set_cmdList = {
	SMS_Conf_Set = '/cgi-bin/sms.cgi?CMD=SMS_CONF_SET',
	SMS_OnOff = '/cgi-bin/sms.cgi?CMD=SMS_ONOFF&OnOff=',
	Save_Smsc_Addr = '/cgi-bin/sms.cgi?CMD=SAVE_SMSC_ADDR&NEW_SMSC_ADDR=',
}

cb_setcbVariables = function (task)
	local set_valueTbl = task.data
	local contents_body = ''
	local needConfSetting = false

	for name, _ in pairs(argsListOf_SMS_Conf_Set) do
		if set_valueTbl[name] then 
			needConfSetting = true
			break;
		end
	end

	if needConfSetting then
		contents_body = ''

		buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

		for name, _ in pairs(argsListOf_SMS_Conf_Set) do
			contents_body = contents_body .. '&' .. name .. '=' .. (set_valueTbl[name] or CGIRespTbl[name] or '')
		end

		CGI_Iface.setValueToCGI(set_cmdList.SMS_Conf_Set .. contents_body)
	end

	if set_valueTbl.New_Smsc_Addr then
		local curr = luardb.get('wwan.0.sms.smsc_addr') or ''
		contents_body = ''

		if curr ~= set_valueTbl.New_Smsc_Addr then
			contents_body = CGI_Iface.EncodeUrl(set_valueTbl.New_Smsc_Addr)
			CGI_Iface.setValueToCGI(set_cmdList.Save_Smsc_Addr .. contents_body)
		end
	end

	if set_valueTbl.OnOff then
		local curr = luardb.get('smstools.enable') or '0'
		contents_body = ''

		if curr ~= set_valueTbl.OnOff then
			contents_body = set_valueTbl.OnOff
			CGI_Iface.setValueToCGI(set_cmdList.SMS_OnOff .. contents_body)
		end
	end

	cb_regTable = nil
end

reg_cbVariable = function (name, value)
	if not name then return end
	if not cb_regTable then cb_regTable = {} end

	cb_regTable[name] = value

	if client:isTaskQueued('postSession', cb_setcbVariables) ~= true then
		client:addTask('postSession', cb_setcbVariables, false, cb_regTable)
	end
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

-- Use rdb handler
-- bool: readwrite
-- Interface:	GET: rdb variable - smstools.enable [1|0]
--		SET: /cgi-bin/sms.cgi?CMD=SMS_ONOFF&OnOff= [1|0]
-- Avaliable value: 1|0
-- Default value: 1
-- If changing this value, that triggers the "sms_onoff.template" which re-emulates the module.
-- So the set value procedure should be in postSession call back.
	[subROOT .. 'Setup.GeneralConfiguration.Enable_SMS'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local result = luardb.get('smstools.enable') or '0'

			if result ~= '1' and result ~= '0' then
				result = '0'
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalBoolean(value)

			if not setVal then return CWMP.Error.InvalidArguments end
			reg_cbVariable('OnOff', setVal)
			return 0
		end
	},

-- uint: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "MsgsPerPage"
-- Cannot use rdb variable directly (smstools.conf.msg_no_per_page)
-- Avaliable value: 10-50
-- Default value: 20
	[subROOT .. 'Setup.GeneralConfiguration.Messages_Per_Page'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = CGIRespTbl.MsgsPerPage

			if not result or result == '' then result = "20" end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalInteger{input=value, minimum=10, maximum=50}

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('MsgsPerPage', tostring(setVal))
			return 0
		end
	},

-- string: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "EncodingScheme"
-- Cannot use rdb variable directly (smstools.conf.coding_scheme)
-- Avaliable value: GSM7|UCS2
-- Default value: GSM7
	[subROOT .. 'Setup.GeneralConfiguration.Encoding_Scheme'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = CGIRespTbl.EncodingScheme or ''

			if result ~= 'GSM7' and result ~= 'UCS2' then
				result = 'GSM7'
			end

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil
			if not setVal then return CWMP.Error.InvalidArguments end

			if setVal ~= 'GSM7' and setVal ~= 'UCS2' then return CWMP.Error.InvalidArguments end

			reg_cbVariable('EncodingScheme', setVal)
			return 0
		end
	},

-- string: readwrite
-- Interface:	GET: rdb variable - wwan.0.sms.smsc_addr
--		SET: /cgi-bin/sms.cgi?CMD=SAVE_SMSC_ADDR&NEW_SMSC_ADDR=encodeUrl(document.SMS.SmscAddrNo.value);
-- Avaliable value: 
-- 	var ifname="<%tmpval=get_single('wwan.0.if');%>@@tmpval";
-- 	if (smsc_addr == '' && sim_status != 'SIM OK' && (ifname == 'cns' || ifname == 'atcns')) {
-- 		document.SMS.SmscAddrNo.value=_("sms warning34");  --> SIM is not ready!
-- 	}
-- Default value: ""
	[subROOT .. 'Setup.GeneralConfiguration.SMSC_Address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local smsc_addr = luardb.get('wwan.0.sms.smsc_addr') or ''
			local ifname = luardb.get('wwan.0.if') or ''
			local sms_status = luardb.get('wwan.0.sim.status.status') or ''

			local result = ''

			if smsc_addr == '' and sms_status ~= 'SIM OK' and (ifname == 'cns' or ifname == 'atcns') then
				result = 'SIM is not ready!'
			else
				result = smsc_addr
			end
			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('New_Smsc_Addr', setVal)
			return 0
		end
	},

-- TODO: WEBUI is broken
-- bool: readwrite
-- 
-- 	[subROOT .. 'Setup.ForwardingSetup.Enable_Forwarding'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			return 0, ''
-- 		end,
-- 		set = function(node, name, value)
-- 			return 0
-- 		end
-- 	},

-- string: readwrite
-- Interface: from CGI "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "RedirMobile"
-- Cannot use rdb variable directly (smstools.conf.redirect_mob)
-- Avaliable value: phone number
-- Default value: ""
	[subROOT .. 'Setup.ForwardingSetup.Redirect_to_Mobile'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = CGIRespTbl.RedirMobile or ''

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('RedirMobile', setVal)
			return 0
		end
	},

-- string: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "RedirTCP"
-- Cannot use rdb variable directly (smstools.conf.redirect_tcp)
-- Avaliable value:
-- Default value: ""
	[subROOT .. 'Setup.ForwardingSetup.TCP_Address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = CGIRespTbl.RedirTCP or ''

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('RedirTCP', setVal)
			return 0
		end
	},

-- string: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "TCPport"
-- Cannot use rdb variable directly (smstools.conf.redirect_tcp_port)
-- Avaliable value: 1-65535
-- Default value: ""
	[subROOT .. 'Setup.ForwardingSetup.TCP_Port'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = tonumber(CGIRespTbl.TCPport or '')

			if not result or result < 1 or result > 65535 then
				result = ''
			else
				result = tostring(result)
			end

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalInteger{input=value, minimum=1, maximum=65535}

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('TCPport', tostring(setVal))
			return 0
		end
	},
-- TODO :
-- string: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "RedirUDP"
-- Cannot use rdb variable directly (smstools.conf.redirect_udp)
-- Avaliable value:
-- Default value: ""
	[subROOT .. 'Setup.ForwardingSetup.UDP_Address'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = CGIRespTbl.RedirUDP or ''

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = value and value:gsub("^%s*(.*)%s*$", "%1") or nil

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('RedirUDP', setVal)
			return 0
		end
	},

-- string: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "UDPport"
-- Cannot use rdb variable directly (smstools.conf.redirect_udp_port)
-- Avaliable value: 1-65535
-- Default value: ""
	[subROOT .. 'Setup.ForwardingSetup.UDP_Port'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')

			local result = tonumber(CGIRespTbl.UDPport or '')

			if not result or result < 1 or result > 65535 then
				result = ''
			else
				result = tostring(result)
			end

			return 0, result
		end,
		set = function(node, name, value)
			local setVal = hdlerUtil.ToInternalInteger{input=value, minimum=1, maximum=65535}

			if not setVal then return CWMP.Error.InvalidArguments end

			reg_cbVariable('UDPport', setVal)
			return 0
		end
	},

-- bool: readwrite
-- Interface: CGI - "/cgi-bin/sms.cgi?CMD=[SMS_CONF_GET|SMS_CONF_SET&]", "RemoteCommand"
-- Cannot use rdb variable directly (smstools.conf.enable_remote_cmd)
-- Avaliable value: 1|0
-- Default value: 1
-- 	[subROOT .. 'Setup.ForwardingSetup.Enable_Remote_Diagnostics'] = {
-- 		init = function(node, name, value) return 0 end,
-- 		get = function(node, name)
-- 			buildCGIRespTbl('/cgi-bin/sms.cgi?CMD=SMS_CONF_GET')
-- 
-- 			local result = CGIRespTbl.RemoteCommand or '1'
-- 			result = tostring(result)
-- 			if not result or result ~= '0' then result = '1' end
-- 			return 0, result
-- 		end,
-- 		set = function(node, name, value)
-- 			local setVal = hdlerUtil.ToInternalBoolean(value)
-- 
-- 			if not setVal then return CWMP.Error.InvalidArguments end
-- 
-- 			reg_cbVariable('RemoteCommand', setVal)
-- 			return 0
-- 		end
-- 	},
}

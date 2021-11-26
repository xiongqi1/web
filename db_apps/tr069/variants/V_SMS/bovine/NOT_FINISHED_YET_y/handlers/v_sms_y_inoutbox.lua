
require("Logger")
Logger.addSubsystem('webui_Services_SMS_InOutBox')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.SMS.' --> If somebody change this variable, please verify index of pathBits[] in [subROOT .. 'Inbox.InboxLists.*.*'] and [subROOT .. 'Outbox.OutboxLists.*.*'].

local InBoxPath = '/usr/sms/inbox/'
local OutBoxPath = '/usr/sms/outbox/'

------------------local function prototype------------------
local resetInBoxTbl
local buildInBoxTbl
local getMd5sumFromDir
local InBoxWatcher
local resetOutBoxTbl
local buildOutBoxTbl
local OutBoxWatcher
------------------------------------------------------------

------------------local variable------------------
local InBoxTbl = {
		InBox_md5sum = nil,
		lists = nil,
	}

local OutBoxTbl = {
		OutBox_md5sum = nil,
		lists = nil,
	}
------------------------------------------------------------

resetInBoxTbl = function ()
	InBoxTbl = {
		InBox_md5sum = nil,
		lists = {},
}
end

buildInBoxTbl = function ()

	resetInBoxTbl ()

	CGI_Iface.getCGIresponse(InBoxTbl.lists, '/cgi-bin/sms.cgi?CMD=READ_SMSBOX&INOUT=INBOX&')
	InBoxTbl.InBox_md5sum = getMd5sumFromDir(InBoxPath)
end

getMd5sumFromDir = function (path)
	if hdlerUtil.IsDirectory(path) then
		return Daemon.readCommandOutput('find ' .. path .. ' -type f -exec md5sum {} + 2> /dev/null | sort | md5sum')
	else
		return nil
	end
end

InBoxWatcher = function ()
	local current_md5sum = getMd5sumFromDir(InBoxPath)

	if current_md5sum ~= InBoxTbl.InBox_md5sum then
		local instanceCollection = paramTree:find(subROOT .. 'Inbox.InboxLists')
		buildInBoxTbl()

		for _, lease in ipairs(instanceCollection.children) do
			if lease.name ~= '0' then
				lease.parent:deleteChild(lease)
			end
		end

		local maxInstanceId = 0
		local loopCnt = InBoxTbl.lists.RespMsgCnt and tonumber(InBoxTbl.lists.RespMsgCnt) or 0
		for i=1, loopCnt do
			maxInstanceId = i
			instanceCollection:createDefaultChild(i)
		end

		instanceCollection.instance = maxInstanceId
	end
end

resetOutBoxTbl = function ()
	OutBoxTbl = {
		OutBox_md5sum = nil,
		lists = {},
	}
end

buildOutBoxTbl = function ()

	resetOutBoxTbl ()

	CGI_Iface.getCGIresponse(OutBoxTbl.lists, '/cgi-bin/sms.cgi?CMD=READ_SMSBOX&INOUT=OUTBOX&')
	OutBoxTbl.OutBox_md5sum = getMd5sumFromDir(OutBoxPath)
end

OutBoxWatcher = function ()
	local current_md5sum = getMd5sumFromDir(OutBoxPath)

	if current_md5sum ~= OutBoxTbl.OutBox_md5sum then
		local instanceCollection = paramTree:find(subROOT .. 'Outbox.OutboxLists')
		buildOutBoxTbl()

		for _, lease in ipairs(instanceCollection.children) do
			if lease.name ~= '0' then
				lease.parent:deleteChild(lease)
			end
		end

		local maxInstanceId = 0
		local loopCnt = OutBoxTbl.lists.RespMsgCnt and tonumber(OutBoxTbl.lists.RespMsgCnt) or 0
		for i=1, loopCnt do
			maxInstanceId = i
			instanceCollection:createDefaultChild(i)
		end

		instanceCollection.instance = maxInstanceId
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
-----------------------------------------------------------------------------
---------------------<START> Inbox-------------------------------------------

-- uint: rdadonly
	[subROOT .. 'Inbox.NumberOfInBoxMsg'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = InBoxTbl.lists.RespMsgCnt or '0'
			return 0, tostring(retVal)
		end,
		set = function(node, name, value)
			return 0
		end
	},

-- Root directory of Inbox Object
-- READONLY Object
	[subROOT .. 'Inbox.InboxLists'] = {
		init = function(node, name)
			if not hdlerUtil.IsDirectory(InBoxPath) then 
				Logger.log('webui_Services_SMS_InOutBox', 'error', 'ERROR!!: SMS InBox directory doesn not exist')
				return 0
			end

			local maxInstanceId = 0

			buildInBoxTbl()
			local loopCnt = InBoxTbl.lists.RespMsgCnt and tonumber(InBoxTbl.lists.RespMsgCnt) or 0

			for i=1, loopCnt do
				maxInstanceId = i
				node:createDefaultChild(i)
			end

			node.instance = maxInstanceId

			if client:isTaskQueued('preSession', InBoxWatcher) ~= true then
				client:addTask('preSession', InBoxWatcher, true) -- persistent callback function
			end

			return 0
		end,
	},

-- Instances of Inbox Object
	[subROOT .. 'Inbox.InboxLists.*'] = {
		init = function(node, name, value) return 0 end,
-- 		delete = function(node, name)
-- 			return 0
-- 		end
	},

-- All of parameters of every Instance
	[subROOT .. 'Inbox.InboxLists.*.*'] = {
		init = function(node, name, value)
-- 			local instanceId_idx = 7  --> should be changed, if the current tree stracture is changed.
-- 			local pathBits = name:explode('.')
-- 			local instanceId = pathBits[instanceId_idx]
-- 
-- 			if not instanceId then return 0 end
-- 
-- 			instanceId = tonumber(instanceId)
-- 			if not instanceId then return 0 end
-- 
-- 			if pathBits[instanceId_idx+1] == 'InboxContent' then
-- 				node.value = InBoxTbl.FileLists[instanceId] or ''
-- 			end
			return 0
		end,
		get = function(node, name)
			local instanceId_idx = 7  --> should be changed, if the current tree stracture is changed.
			local pathBits = name:explode('.')
			local instanceId = pathBits[instanceId_idx]  --> should be changed, if the current tree stracture is changed.

			if not instanceId then return 0 end

			instanceId = tonumber(instanceId)
			if not instanceId then 
				return CWMP.Error.InternalError, 'The instance does Not exist. Name: ' .. name
			end

			local keyOfelement = ''
			local result = ''
			if pathBits[instanceId_idx+1] == 'Status' then
				keyOfelement = 'FileName[' .. instanceId-1 .. ']'
				result = InBoxTbl.lists[keyOfelement]
				if result:match('_unread') then
					result = "Unread"
				else
					result = "Read"
				end
				node.value = result
			elseif pathBits[instanceId_idx+1] == 'From' then
				keyOfelement = 'MobNum[' .. instanceId-1 .. ']'
				result = InBoxTbl.lists[keyOfelement] or ''
				node.value = result:gsub("^%s*(.*)%s*$", "%1") or ''
			elseif pathBits[instanceId_idx+1] == 'ReceiveAt' then
				keyOfelement = 'TxTime[' .. instanceId-1 .. ']'
				result = InBoxTbl.lists[keyOfelement] or ''
				node.value = result:gsub("^%s*(.*)%s*$", "%1") or ''
			elseif pathBits[instanceId_idx+1] == 'MsgBody' then
				keyOfelement = 'MsgBody[' .. instanceId-1 .. ']'
				result = InBoxTbl.lists[keyOfelement] or ''
				node.value = result:sub(6)
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			return 0
		end
	},
---------------------< END > Inbox-------------------------------------------
-----------------------------------------------------------------------------
---------------------<START> Outbox-------------------------------------------
-- TODO:
-- uint: rdadonly
	[subROOT .. 'Outbox.NumberOfOutMsg'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local retVal = OutBoxTbl.lists.RespMsgCnt or '0'
			return 0, tostring(retVal)

		end,
		set = function(node, name, value)
			return 0
		end
	},
-- TODO:
-- Root directory of Inbox Object
-- READONLY Object
	[subROOT .. 'Outbox.OutboxLists'] = {
		init = function(node, name)
			if not hdlerUtil.IsDirectory(OutBoxPath) then 
				Logger.log('webui_Services_SMS_InOutBox', 'error', 'ERROR!!: SMS OutBox directory doesn not exist')
				return 0
			end

			local maxInstanceId = 0

			buildOutBoxTbl()
			local loopCnt = OutBoxTbl.lists.RespMsgCnt and tonumber(OutBoxTbl.lists.RespMsgCnt) or 0

			for i=1, loopCnt do
				maxInstanceId = i
				node:createDefaultChild(i)
			end

			node.instance = maxInstanceId

			if client:isTaskQueued('preSession', OutBoxWatcher) ~= true then
				client:addTask('preSession', OutBoxWatcher, true) -- persistent callback function
			end

			return 0
		end,
	},

-- Instances of Inbox Object
	[subROOT .. 'Outbox.OutboxLists.*'] = {
		init = function(node, name, value) return 0 end,
-- 		delete = function(node, name)
-- 			return 0
-- 		end
	},

-- All of parameters of every Instance
	[subROOT .. 'Outbox.OutboxLists.*.*'] = {
		init = function(node, name, value)
			return 0
		end,
		get = function(node, name)
			local instanceId_idx = 7  --> should be changed, if the current tree stracture is changed.
			local pathBits = name:explode('.')
			local instanceId = pathBits[instanceId_idx]

			if not instanceId then return 0 end

			instanceId = tonumber(instanceId)
			if not instanceId then 
				return CWMP.Error.InternalError, 'The instance does Not exist. Name: ' .. name
			end

			local keyOfelement = ''
			local result = ''
			if pathBits[instanceId_idx+1] == 'To' then
				keyOfelement = 'MobNum[' .. instanceId-1 .. ']'
				result = OutBoxTbl.lists[keyOfelement] or ''
				node.value = result:gsub("^%s*(.*)%s*$", "%1") or ''
			elseif pathBits[instanceId_idx+1] == 'SentAt' then
				keyOfelement = 'TxTime[' .. instanceId-1 .. ']'
				result = OutBoxTbl.lists[keyOfelement] or ''
				node.value = result:gsub("^%s*(.*)%s*$", "%1") or ''
			elseif pathBits[instanceId_idx+1] == 'MsgBody' then
				keyOfelement = 'MsgBody[' .. instanceId-1 .. ']'
				result = OutBoxTbl.lists[keyOfelement] or ''
				node.value = result:sub(6)
			else
				error('Dunno how to handle ' .. name)
			end
			return 0, node.value
		end,
		set = function(node, name, value)
			return 0
		end
	},
---------------------< END > Outbox-------------------------------------------


}

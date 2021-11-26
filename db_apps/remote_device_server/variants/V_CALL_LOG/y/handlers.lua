-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Web request handlers for call history.

require('stringutil')

local turbo = require("turbo")
local util = require("rds.util")
local config = require("rds.config")
local basepath = config.basepath
local luardb = require('luardb')

local  callHistoryRdbName = "voice_call.call_history."
local  qmiCmdCtrlRdbName = 'wwan.0.voice.mgmt.ctrl'
local  maxCallHistoryListLen = 100

-- wait call log management command completes.
-- Or check whether there is any command in process.
--
-- waitTimeout: timeout in integer
-- return: true, if there is no command in process. false, if timed out.
-- Note: https://pdgwiki.netcommwireless.com/mediawiki/index.php/POTS_bridge_v2#RDB_voice_call_history_management
local function waitCommandComplete(waitTimeout)
    local timeout = tonumber(waitTimeout) or 5 -- default 5 seconds

    while not (timeout <= 0 or luardb.get(qmiCmdCtrlRdbName) == '') do
        os.execute('sleep 1')
        timeout = timeout - 1
    end
    return timeout > 0 and true or luardb.get(qmiCmdCtrlRdbName) == ''
end

local  CallHistoryListAllHandler = class("CallHistoryListAllHandler", turbo.web.RequestHandler)

-- map to rdc:get, return the call history list
function  CallHistoryListAllHandler:get(path)
    if not waitCommandComplete() then
        self:set_status(500)
        return
    end

    local callHistoryList = {}
    local listIndex = 1
    local latestIndex = luardb.get(callHistoryRdbName.."index") or 0
    for loopIndex = 0, maxCallHistoryListLen-1 do
        local dbIndex = math.fmod(latestIndex + loopIndex +1, maxCallHistoryListLen)
        local callInfo = luardb.get(callHistoryRdbName..tostring(dbIndex))
        if callInfo then
            -- decode the rdb string into a dictonary
            -- the call history in the rdb: voice_call.call_history.1  - 8,in,0410305940,ALLOWED,1533000360,1
            local callFields = callInfo:explode(',')
            if #callFields >= 6 then
                local uid = callFields[1]
                local direction = callFields[2]
                local number = callFields[3]
                local timeStamp = tonumber(callFields[5])
                local duration = callFields[6]
                -- timeoffset = "UTC time" - "local system time"
                local timeoffset = tonumber(callFields[7]) or 0

                -- convert the direction description to match the frond end
                if direction == 'in' then
                    direction = "Incoming"
                    -- identify the missed call and blocked call
                    if duration == "-1" then
                        direction = "Missed"
                    elseif duration == "-2" then
                        direction = "Blocked"
                    elseif duration == "-3" then
                        direction = "Declined"
                    end
                elseif direction == 'out' then
                    direction = "Outgoing"
                end

                -- convert the timestamp into local readable time
                local time = ''
                if timeStamp then
                    time = timeStamp - timeoffset
                end

                -- if the call setup failed, set the duration to '-'
                if duration == "-1" or duration == "-2"  or duration == "-3" then
                   duration = "-"
                end

                local id = tostring(dbIndex)..':'..tostring(uid)
                callHistoryList['call_log'..tostring(listIndex)] = {['id']= id, ['direction'] = direction,
                                                                    ['number'] = number, ['time'] = time,
                                                                    ['duration'] = duration}
                listIndex = listIndex + 1
            end
        end
    end
    -- send the list to the client
    self:write(callHistoryList)
    self:set_status(200)
end

-- map to rdc: delete, delet the requested records from the rdb
-- rdb set wwan.0.voice.mgmt.ctrl "DELETE_CALL_HISTORY 1:3 3:5"
function  CallHistoryListAllHandler:delete(path)
    local items = (self:get_json() or {})["items"]
    if not items then
        self:set_status(400) -- Bad Request
        return
    end

    local itemListStr = string.trim(table.concat(items:explode(','), ' '))
    if itemListStr == '' then
        self:set_status(400) -- Bad Request
        return
    end

    luardb.set(qmiCmdCtrlRdbName, 'DELETE_CALL_HISTORY ' .. itemListStr, 'p')
    if waitCommandComplete() then
        self:set_status(200)
    else
        self:set_status(500)
    end
end

local  blkListPrefix = "pbx.block_calls."
local  blkListTrigger = blkListPrefix.."trigger"
local  maxBlkListLen  = 30
local  currentIndex

local  BlockListAllHandler = class("BlockListAllHandler", turbo.web.RequestHandler)

-- get the current index for add, if needed, compact the index.
local function compactBlockListIndex()
    dbIndex = 0
    for loopIndex =0, maxBlkListLen-1 do
        local number = luardb.get(blkListPrefix..tostring(loopIndex)..".number")
        if number and #number then
            -- compact the index in the rdb if there is some blank record
            if (loopIndex > dbIndex) then
                 local description = luardb.get(blkListPrefix..tostring(loopIndex)..".description")
                 luardb.set(blkListPrefix..tostring(dbIndex)..".number", number, 'p')
                 luardb.set(blkListPrefix..tostring(dbIndex)..".description", description, 'p')

                 luardb.unset(blkListPrefix..tostring(loopIndex)..".number")
                 luardb.unset(blkListPrefix..tostring(loopIndex)..".description")
            end
            dbIndex = dbIndex + 1
        end
    end
    return dbIndex
end

currentIndex = compactBlockListIndex()

-- map to rdc:get, return the block list and compact the rdb if needed
function  BlockListAllHandler:get(path)
    -- read the whole table into a list
    blockList = {}
    for loopIndex =0, maxBlkListLen-1 do
        local number = luardb.get(blkListPrefix..tostring(loopIndex)..".number")
        if number and #number then
            local description = luardb.get(blkListPrefix..tostring(loopIndex)..".description")
            local tempRec = { ['id'] = tostring(loopIndex), ['number'] = number, ['description'] = description }
            blockList['call_block'..tostring(loopIndex+1)] = tempRec
        else
            break
        end
    end
    -- send the list to the client
    self:write(blockList)
    self:set_status(200)
end

-- map to rdc: delete, delet the requested records from the rdb
function  BlockListAllHandler:delete(path)
    jsonTable = self:get_json()
    -- get the index list to be delete from the client request
    items = jsonTable["items"]
    -- the index list is a string, so split it into individual value
    for  index in (items..','):gmatch("(.-)"..',') do
        luardb.unset(blkListPrefix..index..".number")
        luardb.unset(blkListPrefix..index..".description")
    end
    -- compact the index in the rdb if there is some blank record
    currentIndex = compactBlockListIndex()
    -- trigger POTS Bridge to reload the block list
    luardb.set(blkListTrigger, '1')
    self:set_status(200)
end

-- map to rdc:add, save the new phone number and description into rdb
function BlockListAllHandler:post(path)
    -- get the number and the description from the client request
    jsonTable = self:get_json()
    number = jsonTable["number"]
    description = jsonTable["description"]
    -- save it to the rdb
    luardb.set(blkListPrefix..tostring(currentIndex)..".number", number, 'p')
    luardb.set(blkListPrefix..tostring(currentIndex)..".description", description, 'p')
    -- update the index for next adding
    currentIndex = math.fmod(currentIndex + 1, maxBlkListLen)
    -- trigger POTS Bridge to reload the block list
    luardb.set(blkListTrigger, '1')
    self:set_status(201)
end

-- Return array of path/handler mappings
return { {basepath .. "/call/history$", CallHistoryListAllHandler},
         {basepath .. "/call/block$", BlockListAllHandler} }

--[[
This script handles system power-up count parameters under

  Device.X_<VENDOR>_PowerUpCount

Copyright (C) 2020 Casa Systems. Inc.
--]]

require("rdbobject")

--[[
require("Logger")
local logSubsystem = "PowerUp_count"
Logger.addSubsystem(logSubsystem, "debug") -- TODO: change level to 'notice' once debugged
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. "."

local rdbObjConf = {
    persist = true,
    idSelection = "smallestUnused",
}

local rdbObj = rdbobject.getClass("system.powerup_record", rdbObjConf)

-- traverse nodes with lua generic "for" loop
--
-- class: class object of rdbobject.
-- reverse: true -> traverse from old to new,
--          false -> traverse from new to old.
-- return: iterator function for generic for-loop
local function traverseNodes(class, reverse)
    local idx = 0
    local instList = class:getAll()
    return function ()
        idx = idx + 1
        if reverse then
            return instList[idx]
        else
            return instList[#instList + 1 - idx]
        end
    end
end

return {
    -- string:readonly
    -- List of system power-up count in last 24 hours.
    -- Each item on the list is a power-up count of 1 hour slot.
    -- The epoch of each slot is an o'clock sharp.
    -- For example, if CPE recieves GetParameterValues request at 2:30pm,
    -- the each time slot(total 25 slots in max) is as below,
    -- 02:30pm~02:00pm, 02:00pm~01:00pm, 01:00pm~12:00pm,
    -- 12:00pm~11:00am, 11:00am~10:00am ... , 04:00pm~03:00pm, 03:00pm~02:00pm
    [subRoot .. xVendorPrefix .. "_PowerUpCount"] = {
        get = function(node, name)
            local ret_tbl = {}
            local num_of_slots = 25
            local sec_hour = 3600 -- duration in seconds
            local curr_utc_hour = math.floor(tonumber(os.time()) / sec_hour) -- hours that have elapsed since January 1, 1970

            local time_gap = 0 -- time gap between current hour and first node.
            for node in traverseNodes(rdbObj, false) do -- traverse in order(from first to last)
                local utc_hour_first = tonumber(node.powerup_utc_hour)
                if utc_hour_first then
                    -- found valid first node.
                    time_gap = curr_utc_hour - utc_hour_first
                    break
                end
            end
            if time_gap > num_of_slots then
                time_gap = num_of_slots
            end

            for i=1, time_gap do
                table.insert(ret_tbl, 0)
            end

            for node in traverseNodes(rdbObj, false) do -- traverse in order(from first to last)
                if #ret_tbl >= num_of_slots then
                    break
                end
                table.insert(ret_tbl, tonumber(node.powerup_count) or 0)
            end
            return 0, table.concat(ret_tbl, ',')
        end,
    },
}

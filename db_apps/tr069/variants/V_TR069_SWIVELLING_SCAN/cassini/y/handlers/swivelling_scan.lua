--[[
This script handles objects/parameters under Device.X_<VENDOR>.Services.mmWaveScan

  Command
  Status
  LastError
  CurrentPosition

  TR069 parameter details:
    * Device.X_<VENDOR>.Services.mmWaveScan.Command
      Available Values: ["Start"|"Home"|"Stop"]
      - Start: Starting swivelling scan.
      - Home: Moving to the home position
      - Stop: Stop swivelling scan, if any is running.

    * Device.X_<VENDOR>.Services.mmWaveScan.Status
        - "Status" (string):
            -> "initialising": initialising swivelling scan
            -> "swivellingScan": in progress of swivelling scan - initialising
            -> "swivellingScanMoving": in progress of swivelling scan - moving to a location
            -> "swivellingScanAtPosition": in progress of swivelling scan - moving to a location
            -> "movingToBestPosition": best position has been found, moving to that position
            -> "successFinal": moved to best position
            -> "failed": swivelling scan failed
            -> "none": no scan has been taken place or the device has been forced to move to a position
                       (e.g as a result of the command moveToHomePosition)
            -> "busy": busy state (e.g in progress of stopSwivellingScan)
            -> "stopped": swivelling scan has been stopped (i.e by stopSwivellingScan)

    * Device.X_<VENDOR>.Services.mmWaveScan.LastError
        - "LastError" (string): Error message in failure cases

    * Device.X_<VENDOR>.Services.mmWaveScan.CurrentPosition
        -  "CurrentPosition" (integer): Current position in degree from the home position. (0~359. Or, -1: If could not get the position.)
                  If the stepper motor is moving the device, that shall be the previous stationary position.

Copyright (C) 2021 Casa Systems. Inc.
--]]

local JSON = require("JSON") -- JSON module

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRoot = conf.topRoot .. "." .. xVendorPrefix .. ".Services.mmWaveScan."

------------------local function prototype------------------
local invokeRdbRpc
local getJsonValueOnKey
local cvtIdxToDeg
------------------------------------------------------------


------------------local variable definition ----------------
------------------------------------------------------------


------------------local function definition ----------------

-- Invoke commands of "SwivellingScan" RDB RPC
--
-- cmd: "SwivellingScan" RDB RPC command
-- params: RDB RPC parameter in table
--
-- return: if success, returns true. Otherwise, returns false, error_string.
invokeRdbRpc = function(cmd, params)
    if type(cmd) ~= "string" or type(params) ~= "table" then
        return false, "Invalid Rdb RPC parameters"
    end

    local rval, result = luardb.invoke("SwivellingScan", cmd, 3, 255, unpack(params))
    if rval ~= 0 then
        return false, result
    end

    local jResult = JSON:decode(result)

    if not jResult then
        return false, "JSON result parsing error"
    end
    if not jResult["success"] then
        return false, jResult["error"]
    end
    return true
end

-- Parse json string and get value on key
--
-- jsonStr: json string
-- key: key to get json value
--
-- return: value on the key in string or nil
getJsonValueOnKey = function(jsonStr, key)
    if not jsonStr or not key then
        return
    end
    local status, jsonTbl = pcall(JSON.decode, JSON, jsonStr)
    if status and jsonTbl then
        return jsonTbl[key]
    end
    return
end

-- Convert swivelling scan position index to degree from home postion.
--
-- targetIdx: index of target position
-- homeIdx: index of home position
--
-- return: degree from home position to target position(0~359) or -1 in string type
cvtIdxToDeg = function(targetIdx, homeIdx)
    local retDeg = -1
    if not tonumber(targetIdx) or not tonumber(homeIdx) then
        return tostring(retDeg)
    end

    local targetDeg = tonumber(luardb.get(string.format("swivelling_scan.conf.position.%d.coordinate", targetIdx))) or -1
    local homeDeg = tonumber(luardb.get(string.format("swivelling_scan.conf.position.%d.coordinate", homeIdx))) or -1

    if targetDeg >= 0 and homeDeg >= 0 then
        retDeg = targetDeg - homeDeg
        if retDeg < 0 then
            retDeg = retDeg + 360
        end
    end
    return tostring(retDeg)
end

------------------------------------------------------------

return {
    -- string:writeonly
    -- Available Values: ["Start"|"Home"|"Stop"]
    [subRoot ..  "Command"] = {
        set = function(node, name, value)
            local cmdTbl = {
                Start = { cmd="startSwivellingScan", params={"param", '{"force":false}'} },
                Home = { cmd="moveToHomePosition", params={} },
                Stop = { cmd="stopSwivellingScan", params={} },
            }
            value = string.trim(value)

            if not cmdTbl[value] then
                return CWMP.Error.InvalidParameterValue
            end

            local status, errorStr = invokeRdbRpc(cmdTbl[value].cmd, cmdTbl[value].params)

            if not status then
                return CWMP.Error.InternalError, errorStr
            end
            return 0
        end
    };

    -- string:readonly
    -- Status of swivelling scan
    [subRoot ..  "Status"] = {
        get = function(node, name)
            return 0, getJsonValueOnKey(luardb.get("swivelling_scan.status"), "status") or ""
        end,
    };

    -- string:readonly
    -- Error message in failure cases
    [subRoot ..  "LastError"] = {
        get = function(node, name)
            return 0, getJsonValueOnKey(luardb.get("swivelling_scan.status"), "error") or ""
        end,
    };

    -- int:readonly
    -- Valid range: 0~359. Or, -1: If could not get the position.
    -- "CurrentPosition" (integer): Current position in degree from the home position.
    --        If the stepper motor is moving the device, that shall be the previous stationary position.
    [subRoot ..  "CurrentPosition"] = {
        get = function(node, name)
            return 0, cvtIdxToDeg(getJsonValueOnKey(luardb.get("swivelling_scan.status"), "currentPositionIndex"),
                                  luardb.get("swivelling_scan.conf.home_position_index"))
        end,
    };
}

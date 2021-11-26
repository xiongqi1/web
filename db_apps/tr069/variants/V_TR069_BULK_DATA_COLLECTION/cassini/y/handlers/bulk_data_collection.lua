--[[
This script handles objects/parameters under Device.BulkData
which is based on below standards.
  - TR-157_Amendment-10
  - https://cwmp-data-models.broadband-forum.org/tr-157-1-10-0.html

Copyright (C) 2021 Casa Systems. Inc.
--]]

require("Logger")

local logSubsystem = "BulkDataCollection"
Logger.addSubsystem(logSubsystem)

local mBulkData = require("BulkData.Parameter")
local mBulkDataDaemon = require("BulkData.Daemon")

local subRoot = conf.topRoot .. ".BulkData."
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

------------------local function prototype------------------
local numOfEntries

local initProfileInstValue
local initSubInstValue

local getProfileInstValue
local getSubInstValue

local setProfileInstValue
local setSubInstValue

local initObjectInstance
local createObjectInstance
local deleteObjectInstance

local resetProfileState
------------------------------------------------------------

------------------local variable definition ----------------

-- Rdb object list
-- * Device.BulkData.Profile
--   --> Rdb object name: tr069.bulkData.profile.
--
-- * Device.BulkData.Profile.#.HTTP.RequestURIParameter.
--   --> Rdb object : tr069.bulkData.profile.#.HttpUri.
--
-- * Device.BulkData.Profile.#.Parameter.
--   --> Rdb object : tr069.bulkData.profile.#.Parameter.
--
--    Profile rdb object ==> Profile object
--      |--- HttpUri rdb object ==> Sub object
--      |--- Parameter rdb object ==> Sub object

local profileRdbObjName = "tr069.bulkData.profile"
local rdbObjConfig = {persist = true, idSelection = "smallestUnused"}

local profileRdbObj = rdbobject.getClass(profileRdbObjName, rdbObjConfig)

-- Table for instance_name and rdb object instance pairs
-- instance_name:
--     ==> Profile instance: Profile#
--     ==> Sub instance: Profile#SubName# (SubName:[HttpUri|Parameter])
local gInstTbl = {}

-- string pattern to get profile index from node name
local patternProfileInst = "Device%.BulkData%.Profile%.(%d+)%.*"

-- string pattern to get profile/instance index of sub rdb object from node name
local patternSubInst = {
    HttpUri = "Device%.BulkData%.Profile%.(%d+)%.HTTP%.RequestURIParameter%.(%d+)%.*",
    Parameter = "Device%.BulkData%.Profile%.(%d+)%.Parameter%.(%d+)%.*",
}
------------------------------------------------------------


------------------local function definition ----------------
-- get Number of instance entries of given entryName in the pathName
--
-- pathName: full path name of NumberOfEntries parameter
-- entryName: entry name of target object.
--
-- return: number os instance entries in string.
numOfEntries = function(pathName, entryName)
    if not pathName or not entryName then return "0" end
    local pathNameList = string.explode(pathName, ".")
    if #pathNameList <= 1 then return "0" end
    pathNameList[#pathNameList] = entryName
    return hdlerUtil.getNumOfSubInstance(table.concat(pathNameList, "."))
end

-- initialise value of rdb object profile instance corresponded with given data model path
--
-- pathName: data model path name
-- rdbName: property name of rdb object
--
-- Note: it does not set default value, if the default value is nil or empty string.
initProfileInstValue = function(pathName, rdbName, defaultVal)
    if not defaultVal or defaultVal == "" then return end
    assert(pathName and rdbName, "Invalid arguments")

    if not getProfileInstValue(pathName, rdbName, nil) then
        setProfileInstValue(pathName, rdbName, defaultVal)
    end
end

-- initialise value of rdb object sub instance corresponded with given data model path
--
-- pathName: data model path name
-- rdbObjInstType: rdb object type to initialise. ["HttpUri"|"Parameter"]
-- rdbName: property name of rdb object
--
-- Note: it does not set default value, if the default value is nil or empty string.
initSubInstValue = function(pathName, rdbObjInstType, rdbName, defaultVal)
    if not defaultVal or defaultVal == "" then return end
    assert(patternSubInst[rdbObjInstType] and pathName and rdbName, "Invalid arguments")

    if not getSubInstValue(pathName, rdbObjInstType, rdbName, nil) then
        setSubInstValue(pathName, rdbObjInstType, rdbName, defaultVal)
    end
end

-- get value of rdb object profile instance corresponded with given data model path
--
-- pathName: data model pathname
-- rdbName: property name of rdb object
-- defaultVal: default value to set when the rdb variable does not exist
--
-- return: value or nil
getProfileInstValue = function(pathName, rdbName, defaultVal)
    assert(pathName and rdbName, "Invalid arguments")
    local profileIdx = string.match(pathName, patternProfileInst)
    return mBulkData.getProfileInstValue(profileIdx, rdbName, defaultVal)
end

-- get value of rdb object sub instance corresponded with given data model path
--
-- pathName: data model pathname
-- rdbObjInstType: rdb object type of the instance to get. ["HttpUri"|"Parameter"]
-- rdbName: property name of rdb object
-- defaultVal: default value to set when the rdb variable does not exist
--
-- return: value or nil
getSubInstValue = function(pathName, rdbObjInstType, rdbName, defaultVal)
    assert(patternSubInst[rdbObjInstType] and pathName and rdbName, "Invalid arguments")
    local profileIdx, instanceIdx = string.match(pathName, patternSubInst[rdbObjInstType])
    return mBulkData.getSubInstValue(rdbObjInstType, profileIdx, instanceIdx, rdbName, defaultVal)
end

-- set value of rdb object profile instance corresponded with given data model path
--
-- pathName: data model pathname
-- rdbName: property name of rdb object
-- value: value to set
--
-- return: true or false
setProfileInstValue = function(pathName, rdbName, value)
    assert(pathName and rdbName, "Invalid arguments")
    local profileIdx = string.match(pathName, patternProfileInst)
    return mBulkData.setProfileInstValue(profileIdx, rdbName, value)
end

-- set value of rdb object sub instance corresponded with given data model path
--
-- pathName: data model pathname
-- rdbObjInstType: rdb object type of the instance to set. ["HttpUri"|"Parameter"]
-- rdbName: property name of rdb object
--
-- return: true or false
setSubInstValue = function(pathName, rdbObjInstType, rdbName, value)
    assert(patternSubInst[rdbObjInstType] and pathName and rdbName, "Invalid arguments")
    local profileIdx, instanceIdx = string.match(pathName, patternSubInst[rdbObjInstType])
    return mBulkData.setSubInstValue(rdbObjInstType, profileIdx, instanceIdx, rdbName, value)
end

-- Initialise object instance of data model and rdb object
--
-- node: Data model node
-- rdbObj: rdb object
initObjectInstance = function(node, rdbObj)
    if not rdbObj then return 0 end
    for _, rdbObjIdx in ipairs(rdbObj:getIds()) do
        local id = tonumber(rdbObjIdx) or 0

        if id > 0 then
            node:createDefaultChild(id)
        end
    end
end

-- Add object instance to data model node and rdb object
--
-- node: Data model node to add object instance
-- rdbObj: rdb object to add object instance
--
-- return: (status, index_number)
createObjectInstance = function(node, rdbObj)
    local newInst = rdbObj:new()
    local dataModelIdx = rdbObj:getId(newInst)

    if not dataModelIdx then
        return CWMP.Error.InternalError
    end

    local instance = node:createDefaultChild(dataModelIdx)
    instance:recursiveInit()
    return 0, dataModelIdx
end


-- Delete object instance from data model and rdb object
--
-- node: data model node to delete
-- rdbObj: rdb object to delete object instance
--
-- return: (status, err_str)
deleteObjectInstance = function(node, rdbObj)
    local dataModelIdx = string.match(node:getPath(true), "%a+%.(%d+)%.$")

    if not dataModelIdx then
        return CWMP.Error.InvalidParameterName
    end
    if not rdbObj then
        return CWMP.Error.InternalError, "Rdb object is not valid"
    end

    local rdbObjInstance = rdbObj:getById(dataModelIdx)

    rdbObj:delete(rdbObjInstance) -- delete rdb object instance
    node.parent:deleteChild(node) -- delete data model instance
    return 0
end

-- Task function to reset BulkData state machine.
resetProfileState = function(task)
    local pIdx = tonumber(task.data)
    if pIdx then
        local inst = mBulkData.getProfileInstance(pIdx)
        if inst then
            mBulkDataDaemon.setState(inst, "init")
        end
    end
end
------------------------------------------------------------

return {
    -- Device.BulkData.Status
    -- RO:string
    [subRoot .. "Status"] = {
        get = function(node, name)
            if luardb.get("tr069.bulkData.config.enable") == "1" then
                return 0, "Enabled"
            end
            return 0, "Disabled"
        end,
    };

    -- Device.BulkData.ProfileNumberOfEntries
    -- RO:uint
    [subRoot .. "ProfileNumberOfEntries"] = {
        get = function(node, name)
            return 0, numOfEntries(name, "Profile")
        end,
    };

    -- Device.BulkData.Profile.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.
    [subRoot .. "Profile"] = {
        init = function(node, name)
            node:setAccess("readwrite") -- Support AddObject/DeleteObject methods.
            initObjectInstance(node, mBulkData.getProfileObj())
            return 0
        end,
        create = function(node, name)
            local maxProfile = tonumber(luardb.get("tr069.bulkData.config.maxProfiles")) or 10
            local rdbObj = mBulkData.getProfileObj()
            local instances = rdbObj:getAll()
            if #instances >= maxProfile then
                return CWMP.Error.ResourcesExceeded, string.format("MaxNumberOfProfiles is %s", maxProfile)
            end
            return createObjectInstance(node, rdbObj)
        end,
    };

    -- Device.BulkData.Profile.#.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.#.
    [subRoot .. "Profile.*"] = {
        delete = function(node, name)
            return deleteObjectInstance(node, mBulkData.getProfileObj())
        end,
    };

    -- Device.BulkData.Profile.#.Enable
    -- RW:bool
    -- rdb variable: tr069.bulkData.profile.#.Enable
    [subRoot .. "Profile.*.Enable"] = {
        init = function(node, name)
            initProfileInstValue(name, "Enable", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "Enable", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "1" and value ~= "0") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "Enable", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.Alias
    -- RW:string(64)
    -- rdb variable: tr069.bulkData.profile.#.Alias
    [subRoot .. "Profile.*.Alias"] = {
        init = function(node, name)
            initProfileInstValue(name, "Alias", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "Alias", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "Alias", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.Name
    -- RW:string(255)
    -- rdb variable: tr069.bulkData.profile.#.Name
    [subRoot .. "Profile.*.Name"] = {
        init = function(node, name)
            initProfileInstValue(name, "Name", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "Name", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 255 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "Name", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.NumberOfRetainedFailedReports
    -- RW:int[-1:]
    -- rdb variable: tr069.bulkData.profile.#.NumberOfRetainedFailedReports
    [subRoot .. "Profile.*.NumberOfRetainedFailedReports"] = {
        init = function(node, name)
            initProfileInstValue(name, "NumberOfRetainedFailedReports", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "NumberOfRetainedFailedReports", node.default) or node.default
        end,
        set = function(node, name, value)
            value = tonumber(value)
            if not value or value < -1 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "NumberOfRetainedFailedReports", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.Protocol
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.Protocol
    -- Valid input: File:HTTP
    [subRoot .. "Profile.*.Protocol"] = {
        init = function(node, name)
            initProfileInstValue(name, "Protocol", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "Protocol", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value ~= "HTTP" then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "Protocol", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.EncodingType
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.EncodingType
    -- Valid input: JSON
    [subRoot .. "Profile.*.EncodingType"] = {
        init = function(node, name)
            initProfileInstValue(name, "EncodingType", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "EncodingType", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value ~= "JSON" then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "EncodingType", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.ReportingInterval
    -- RW:uint[1:]
    -- rdb variable: tr069.bulkData.profile.#.ReportingInterval
    -- MinReportingInterval: 60
    [subRoot .. "Profile.*.ReportingInterval"] = {
        init = function(node, name)
            initProfileInstValue(name, "ReportingInterval", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "ReportingInterval", node.default) or node.default
        end,
        set = function(node, name, value)
            value = tonumber(value)
            if not value or value < 60 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "ReportingInterval", value) then
                return CWMP.Error.InternalError
            end
            local pathBits = name:explode('.')
            if client:isTaskQueued("postSession", resetProfileState, pathBits[4]) ~= true then
                client:addTask("postSession", resetProfileState, false, pathBits[4])
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.TimeReference
    -- RW:dateTime
    -- rdb variable: tr069.bulkData.profile.#.TimeReference
    [subRoot .. "Profile.*.TimeReference"] = {
        init = function(node, name)
            initProfileInstValue(name, "TimeReference", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "TimeReference", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "TimeReference", value) then
                return CWMP.Error.InternalError
            end
            local pathBits = name:explode('.')
            if client:isTaskQueued("postSession", resetProfileState, pathBits[4]) ~= true then
                client:addTask("postSession", resetProfileState, false, pathBits[4])
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.ParameterNumberOfEntries
    -- RO:uint
    [subRoot .. "Profile.*.ParameterNumberOfEntries"] = {
        get = function(node, name)
            return 0, numOfEntries(name, "Parameter")
        end,
    };

    -- Device.BulkData.Profile.#.X_<VENDOR>_Status
    -- RO:string
    -- rdb variable: tr069.bulkData.profile.#.Status
    [subRoot .. 'Profile.*.' .. xVendorPrefix .. '_Status'] = {
        init = function(node, name)
            initProfileInstValue(name, "Status", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "Status", node.default) or node.default
        end,
    };

    -- Device.BulkData.Profile.#.FileTransferURL
    -- RW:string(256)
    -- rdb variable: tr069.bulkData.profile.#.FtpURL
    [subRoot .. "Profile.*.FileTransferURL"] = {
        init = function(node, name)
            initProfileInstValue(name, "FtpURL", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "FtpURL", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 256 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "FtpURL", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.FileTransferUsername
    -- RW:string(64)
    -- rdb variable: tr069.bulkData.profile.#.FtpUsername
    [subRoot .. "Profile.*.FileTransferUsername"] = {
        init = function(node, name)
            initProfileInstValue(name, "FtpUsername", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "FtpUsername", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "FtpUsername", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.FileTransferPassword
    -- RW:string(64)
    -- rdb variable: tr069.bulkData.profile.#.FtpPassword
    [subRoot .. "Profile.*.FileTransferPassword"] = {
        init = function(node, name)
            initProfileInstValue(name, "FtpPassword", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "FtpPassword", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "FtpPassword", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.ControlFileFormat
    -- RW:string(128)
    -- rdb variable: tr069.bulkData.profile.#.FtpFileFormat
    [subRoot .. "Profile.*.ControlFileFormat"] = {
        init = function(node, name)
            initProfileInstValue(name, "ControlFileFormat", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "FtpFileFormat", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 128 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "FtpFileFormat", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.URL
    -- RW:string(1024)
    -- rdb variable: tr069.bulkData.profile.#.HttpURL
    [subRoot .. "Profile.*.HTTP.URL"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpURL", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpURL", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 1024 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpURL", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.Username
    -- RW:string(256)
    -- rdb variable: tr069.bulkData.profile.#.HttpUsername
    [subRoot .. "Profile.*.HTTP.Username"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpUsername", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpUsername", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 256 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpUsername", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.Password
    -- RW:string(256)
    -- rdb variable: tr069.bulkData.profile.#.HttpPassword
    [subRoot .. "Profile.*.HTTP.Password"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpPassword", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpPassword", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 256 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpPassword", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.Compression
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.HttpCompression
    -- Valid input: GZIP
    [subRoot .. "Profile.*.HTTP.Compression"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpCompression", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpCompression", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value ~= "GZIP" then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpCompression", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.Method
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.HttpMethod
    -- Valid input: POST:PUT
    [subRoot .. "Profile.*.HTTP.Method"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpMethod", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpMethod", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "POST" and value ~= "PUT") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpMethod", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.UseDateHeader
    -- RW:bool
    -- rdb variable: tr069.bulkData.profile.#.HttpUseDateHeader
    [subRoot .. "Profile.*.HTTP.UseDateHeader"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpUseDateHeader", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpUseDateHeader", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "1" and value ~= "0") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpUseDateHeader", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RetryEnable
    -- RW:bool
    -- rdb variable: tr069.bulkData.profile.#.HttpRetryEnable
    [subRoot .. "Profile.*.HTTP.RetryEnable"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpRetryEnable", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpRetryEnable", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "1" and value ~= "0") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpRetryEnable", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RetryMinimumWaitInterval
    -- RW:uint[1:65535]
    -- rdb variable: tr069.bulkData.profile.#.HttpRetryMinimumWaitInterval
    [subRoot .. "Profile.*.HTTP.RetryMinimumWaitInterval"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpRetryMinimumWaitInterval", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpRetryMinimumWaitInterval", node.default) or node.default
        end,
        set = function(node, name, value)
            value = tonumber(value)
            if not value or value < 1 or value > 65535 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpRetryMinimumWaitInterval", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RetryIntervalMultiplier
    -- RW:uint[1000:65535]
    -- rdb variable: tr069.bulkData.profile.#.HttpRetryIntervalMultiplier
    [subRoot .. "Profile.*.HTTP.RetryIntervalMultiplier"] = {
        init = function(node, name)
            initProfileInstValue(name, "HttpRetryIntervalMultiplier", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "HttpRetryIntervalMultiplier", node.default) or node.default
        end,
        set = function(node, name, value)
            value = tonumber(value)
            if not value or value < 1000 or value > 65535 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "HttpRetryIntervalMultiplier", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RequestURIParameterNumberOfEntries
    -- RO:uint
    [subRoot .. "Profile.*.HTTP.RequestURIParameterNumberOfEntries"] = {
        get = function(node, name)
            return 0, numOfEntries(name, "RequestURIParameter")
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RequestURIParameter.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.#.HttpUri.
    [subRoot .. "Profile.*.HTTP.RequestURIParameter"] = {
        init = function(node, name)
            node:setAccess("readwrite") -- Support AddObject/DeleteObject methods.

            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            initObjectInstance(node, mBulkData.getSubObj("HttpUri", profileIdx))
            return 0
        end,
        create = function(node, name)
            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            return createObjectInstance(node, mBulkData.getSubObj("HttpUri", profileIdx))
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RequestURIParameter.#.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.#.HttpUri.#.
    [subRoot .. "Profile.*.HTTP.RequestURIParameter.*"] = {
        delete = function(node, name)
            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            return deleteObjectInstance(node, mBulkData.getSubObj("HttpUri", profileIdx))
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RequestURIParameter.#.Name
    -- RW:string(64)
    -- rdb variable: tr069.bulkData.profile.#.HttpUri.#.Name
    [subRoot .. "Profile.*.HTTP.RequestURIParameter.*.Name"] = {
        get = function(node, name)
            return 0, getSubInstValue(name, "HttpUri", "Name", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setSubInstValue(name, "HttpUri", "Name", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.HTTP.RequestURIParameter.#.Reference
    -- RW:string(256)
    -- rdb variable: tr069.bulkData.profile.#.HttpUri.#.Reference
    [subRoot .. "Profile.*.HTTP.RequestURIParameter.*.Reference"] = {
        get = function(node, name)
            return 0, getSubInstValue(name, "HttpUri", "Reference", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 256 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setSubInstValue(name, "HttpUri", "Reference", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.Parameter.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.#.Parameter.
    [subRoot .. "Profile.*.Parameter"] = {
        init = function(node, name)
            node:setAccess("readwrite") -- Support AddObject/DeleteObject methods.

            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            initObjectInstance(node, mBulkData.getSubObj("Parameter", profileIdx))
            return 0
        end,
        create = function(node, name)
            local maxParamRef = tonumber(luardb.get("tr069.bulkData.config.maxParamRef")) or 300
            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            local rdbObj = mBulkData.getSubObj("Parameter", profileIdx)
            local instances = rdbObj:getAll()
            if #instances >= maxParamRef then
                return CWMP.Error.ResourcesExceeded, string.format("MaxNumberOfProfiles is %s", maxParamRef)
            end
            return createObjectInstance(node, rdbObj)
        end,
    };

    -- Device.BulkData.Profile.#.Parameter.#.
    -- RW:object
    -- rdb object name: tr069.bulkData.profile.#.Parameter.#.
    [subRoot .. "Profile.*.Parameter.*"] = {
        delete = function(node, name)
            local profileIdx = string.match(name, "Device%.BulkData%.Profile%.(%d+)%..*")
            return deleteObjectInstance(node, mBulkData.getSubObj("Parameter", profileIdx))
        end,
    };

    -- Device.BulkData.Profile.#.Parameter.#.Name
    -- RW:string(64)
    -- rdb variable: tr069.bulkData.profile.#.Parameter.#.Name
    [subRoot .. "Profile.*.Parameter.*.Name"] = {
        get = function(node, name)
            return 0, getSubInstValue(name, "Parameter", "Name", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setSubInstValue(name, "Parameter", "Name", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.#.Parameter.#.Reference
    -- RW:string(256)
    -- rdb variable: tr069.bulkData.profile.#.Parameter.#.Reference
    [subRoot .. "Profile.*.Parameter.*.Reference"] = {
        get = function(node, name)
            return 0, getSubInstValue(name, "Parameter", "Reference", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or value:len() > 64 then
                return CWMP.Error.InvalidParameterValue
            end
            if not setSubInstValue(name, "Parameter", "Reference", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.{i}.JSONEncoding.ReportFormat
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.JsonReportFormat
    -- Valid input: ObjectHierarchy|NameValuePair
    [subRoot .. "Profile.*.JSONEncoding.ReportFormat"] = {
        init = function(node, name)
            initProfileInstValue(name, "JsonReportFormat", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "JsonReportFormat", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "ObjectHierarchy" and value ~= "NameValuePair") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "JsonReportFormat", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };

    -- Device.BulkData.Profile.{i}.JSONEncoding.ReportTimestamp
    -- RW:string
    -- rdb variable: tr069.bulkData.profile.#.JsonReportTimestamp
    -- Valid input: Unix-Epoch|ISO-8601|None
    [subRoot .. "Profile.*.JSONEncoding.ReportTimestamp"] = {
        init = function(node, name)
            initProfileInstValue(name, "JsonReportTimestamp", node.default)
            return 0
        end,
        get = function(node, name)
            return 0, getProfileInstValue(name, "JsonReportTimestamp", node.default) or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "Unix-Epoch" and value ~= "ISO-8601" and value ~= "None") then
                return CWMP.Error.InvalidParameterValue
            end
            if not setProfileInstValue(name, "JsonReportTimestamp", value) then
                return CWMP.Error.InternalError
            end
            return 0
        end,
    };
}

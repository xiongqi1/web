----
-- * Copyright Notice:
-- * Copyright (C) 2021 Casa Systems.
-- *
-- * This file or portions thereof may not be copied or distributed in any form
-- * (including but not limited to printed or electronic forms and binary or object forms)
-- * without the expressed written consent of Casa Systems.
-- * Copyright laws and International Treaties protect the contents of this file.
-- * Unauthorized use is prohibited.
-- *
-- *
-- * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
-- * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
-- * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
-- * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- * SUCH DAMAGE.
----

require("luardb")
require("rdbobject")

------------------local function prototype------------------
local getProfileObj
local getSubObj

local getProfileInstance
local getSubInstance

local getProfileInstValue
local getSubInstValue

local setProfileInstValue
local setSubInstValue
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
local subObjTypeList = {HttpUri = true, Parameter=true}
------------------------------------------------------------

-- get Profile rdb object
getProfileObj = function ()
    return profileRdbObj
end

--     ==> Sub instance: Profile#SubName# (SubName:[HttpUri|Parameter])
-- get Sub rdb object
--
-- profileIdx: profile index of parent rdb object
-- subObjType: rdb object type of the instance to get. ["HttpUri"|"Parameter"]
--
-- return: rdb object
getSubObj = function(subObjType, profileIdx)
    assert(tonumber(profileIdx) and subObjTypeList[subObjType], "Invalid argument")
    return rdbobject.getClass(profileRdbObjName .. "." .. profileIdx .. "." .. subObjType, rdbObjConfig)
end

-- get rdbobject profile instance of given profile index
--
-- profileIdx: profile index to get or nil
--
-- return: rdbobject instance or nil
-- return: if profileIdx has valid index value, return rdbobject instance.
--         if profileIdx is nil, return table that has all of instances.
-- Note: argument validation should be taken place in the caller
getProfileInstance = function(profileIdx)
    if tonumber(profileIdx) then
        return profileRdbObj:getById(profileIdx)
    else
        return profileRdbObj:getAll()
    end
end

-- get rdbobject sub instance of given profile/instance index
--
-- rdbObjInstType: rdb object type of the instance to get. ["HttpUri"|"Parameter"]
-- profileIdx: profile index of parent rdb object
-- instanceIdx: instance index to get or nil
--
-- return: if instanceIdx has valid index value, return rdbobject instance.
--         if instanceIdx is nil, return table that has all of instances.
-- Note: argument validation should be taken place in the caller
getSubInstance = function(rdbObjInstType, profileIdx, instanceIdx)
    local rdbObj = rdbobject.getClass(profileRdbObjName .. "." .. profileIdx .. "." .. rdbObjInstType, rdbObjConfig)
    if tonumber(instanceIdx) then
        return rdbObj:getById(instanceIdx)
    else
        return rdbObj:getAll()
    end
end

-- get value of rdb object profile instance
--
-- profileIdx: profile index of rdb object
-- rdbName: property name of rdb object
-- defaultVal: default value to set when the rdb variable does not exist
--
-- return: value or nil
getProfileInstValue = function(profileIdx, rdbName, defaultVal)
    assert(tonumber(profileIdx) and rdbName, "Invalid arguments")
    local instance = getProfileInstance(profileIdx)
    if not instance then return end
    return instance[rdbName] or defaultVal
end

-- get value of rdb object sub instance
--
-- rdbObjInstType: rdb object type of the instance to get. ["HttpUri"|"Parameter"]
-- profileIdx: profile index of parent rdb object
-- instanceIdx: instance index of sub rdb object
-- rdbName: property name of rdb object
-- defaultVal: default value to set when the rdb variable does not exist
--
-- return: value or nil
getSubInstValue = function(rdbObjInstType, profileIdx, instanceIdx, rdbName, defaultVal)
    assert(subObjTypeList[rdbObjInstType] and tonumber(profileIdx)  and tonumber(instanceIdx) and rdbName, "Invalid arguments")
    local instance = getSubInstance(rdbObjInstType, profileIdx, instanceIdx)
    if not instance then return end
    return instance[rdbName] or defaultVal
end

-- set value of rdb object profile instance corresponded with given data model path
--
-- profileIdx: profile index of rdb object
-- rdbName: property name of rdb object
-- value: value to set
--
-- return: true or false
setProfileInstValue = function(profileIdx, rdbName, value)
    assert(tonumber(profileIdx) and rdbName, "Invalid arguments")
    local instance = getProfileInstance(profileIdx)
    if not instance then return false end
    instance[rdbName] = value
    return true
end

-- set value of rdb object sub instance corresponded with given data model path
--
-- rdbObjInstType: rdb object type of the instance to set. ["HttpUri"|"Parameter"]
-- profileIdx: profile index of parent rdb object
-- instanceIdx: instance index of sub rdb object
-- rdbName: property name of rdb object
-- value: value to set
--
-- return: true or false
setSubInstValue = function(rdbObjInstType, profileIdx, instanceIdx, rdbName, value)
    assert(subObjTypeList[rdbObjInstType] and tonumber(profileIdx)  and tonumber(instanceIdx) and rdbName, "Invalid arguments")
    local instance = getSubInstance(rdbObjInstType, profileIdx, instanceIdx)
    if not instance then return false end
    instance[rdbName] = value
    return true
end


local function cvtBool(value)
    if value ~= "1" and value ~= 1 then
        return "0"
    else
        return "1"
    end
end

return {
    getProfileObj = getProfileObj,
    getSubObj = getSubObj,

    getProfileInstance = getProfileInstance,
    getSubInstance = getSubInstance,

    getProfileInstValue = getProfileInstValue,
    getSubInstValue = getSubInstValue,

    setProfileInstValue = setProfileInstValue,
    setSubInstValue = setSubInstValue,
}

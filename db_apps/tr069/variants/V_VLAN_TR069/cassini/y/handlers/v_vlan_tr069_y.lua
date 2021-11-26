--[[
This script handles objects/parameters under
 - Device.Ethernet.XVENDOR(_EnableVLAN)
 - Device.Ethernet.VLANTerminationNumberOfEntries
 - Device.Ethernet.VLANTermination.{i}.
 - Device.Cellular.AccessPoint.{i}.X_<VENDOR>.MapToVLAN

  TR069 parameter details:
    * Device.Ethernet.
      - XVENDOR(_EnableVLAN): Enable/Disable VLAN service
      - VLANTerminationNumberOfEntries: Number of sub-object of Device.X_<VENDOR>.Services.LocalVLAN.

    * Device.X_<VENDOR>.Services.LocalVLAN.#.
      - Enable: Enable/Disable VLAN Rule
      - VLANID:
      - XVENDOR(_Name):
      - XVENDOR(_IPAddress):
      - XVENDOR(_SubnetMask):
      - XVENDOR(_EnableDHCP):
      - XVENDOR(_DHCPAddressRange):
      - XVENDOR(_DHCPLeaseTime):
      - XVENDOR(_AllowAdminAccess):

    * Device.Cellular.AccessPoint.{i}.X_<VENDOR>.
      - MapToVLAN: fullpath of corresponding VLAN object on TR069 data model.
                   Ex) Device.Ethernet.VLANTermination.1.

Copyright (C) 2021 Casa Systems. Inc.
--]]

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

local subRootVlan = conf.topRoot .. ".Ethernet."
local subRootWwan = conf.topRoot .. ".Cellular.AccessPoint.*." .. xVendorPrefix .. "."

local vlanTermObjPath = subRootVlan .. "VLANTermination."

-- String match pattern to get index of sub-object.
local pattVLANTerm = string.format("%s(%%d+)%%.%%S*", vlanTermObjPath)
local pattMapToVLAN = string.format("%s%%.Cellular%%.AccessPoint%%.(%%d+)%%.%s%%.MapToVLAN", conf.topRoot, xVendorPrefix)

local vlanRdbPrefix = "vlan."

local maxVlanRdbIdx = 49 -- The maximum number is from webui.
------------------local function prototype------------------
local vlanTrigger_cb
local registerVLANTrigger
local wwanTrigger_cb
local rebuildVlanTermInst
local setRdbIfNotExist
local getRdbVlanIds
------------------------------------------------------------

-----------local callback function definitions--------------
-- Validation check for VLANID paramter(Valid range: 0~4094)
local function setValidatorVLANID(value)
    local value = tonumber(value) or -1
    if value < 0 or value > 4094 then
        return false
    end
    return true
end

-- Validation check for DHCPLeaseTime parameter(Valid range: >= 120)
local function setValidatorDHCPLeaseTime(value)
    local value = tonumber(value) or -1
    if value < 120 then
        return false
    end
    return true
end

-- Validation check for DHCPAddressRange parameter(Valid format: Start_addr,End_addr)
local function setValidatorDHCPAddressRange(value)
    local addrRange = value and value:explode(",") or {}

    if #addrRange ~= 2
        or not Parameter.Validator.isValidIP4(string.trim(addrRange[1]))
        or not Parameter.Validator.isValidIP4(string.trim(addrRange[2])) then
        return false
    end
    return true
end

local function postSetVLANID(instIdx, value)
    local rdbIdx = (tonumber(instIdx) or 0) - 1
    if rdbIdx < 0 then return end
    local interface = luardb.get(string.format("%s%d.interface", vlanRdbPrefix, rdbIdx)) or "eth0"
    luardb.set(string.format("%s%d.name", vlanRdbPrefix, rdbIdx), interface .. '.' .. value)
end
------------------------------------------------------------

------------------local variable definition ----------------

-- Parameter name of VLANTermination object and corresponding data.
-- Key: Parameter name of VLANTermination object
-- Value:
--    -> rdbName: rdb variable name of vlan.#. instance
--    -> setVal_cb: validator on argument of SetParameterValues RPC.
--    -> valErrMsg: error message on validation failure of SetParameterValues RPC.
--    -> postSet_cb: callback which is execuated after setting value.
local vlanTermParmList = {
    ["Enable"] = {rdbName = "enable", setVal_cb = nil},
    ["VLANID"] = {
        rdbName = "id",
        setVal_cb = setValidatorVLANID,
        valErrMsg="Valid VLAN Id range is between 0 and 4094",
        postSet_cb = postSetVLANID,
    },
    [xVendorPrefix .. "_Name"] = {rdbName = "rule_name", setVal_cb = nil},
    [xVendorPrefix .. "_IPAddress"] = {
        rdbName = "address",
        setVal_cb = Parameter.Validator.isValidIP4,
        valErrMsg="Invalid IP Address"
    },
    [xVendorPrefix .."_SubnetMask"] = {
        rdbName = "netmask",
        setVal_cb = Parameter.Validator.isValidIP4Netmask,
        valErrMsg="Invalid Subnetmask"
    },
    [xVendorPrefix .. "_EnableDHCP"] = {rdbName = "dhcp.enable", setVal_cb = nil },
    [xVendorPrefix .. "_DHCPAddressRange"] = {
        rdbName = "dhcp.range",
        setVal_cb = setValidatorDHCPAddressRange,
        valErrMsg="Invalid Address Range"
    },
    [xVendorPrefix .. "_DHCPLeaseTime"] = {
        rdbName = "dhcp.lease",
        setVal_cb = setValidatorDHCPLeaseTime,
        valErrMsg="Valid DHCP Lease Time is greater then or equal to 120"
    },
    [xVendorPrefix .. "_AllowAdminAccess"] = {rdbName = "admin_access_allowed", setVal_cb = nil},
}

------------------------------------------------------------

------------------local function definition ----------------
-- callback function to trigger vlan template
vlanTrigger_cb = function()
    luardb.set("vlan.trigger", "1")
end

-- register vlanTrigger_cb in postSession
registerVLANTrigger = function()
    if client:isTaskQueued("postSession", vlanTrigger_cb) ~= true then
        client:addTask("postSession", vlanTrigger_cb, false)
    end
end

wwanTrigger_cb = function(task)
    local rdbIdx = tonumber(task.data) or 0
    if rdbIdx < 1 then
        return
    end
    luardb.set(string.format("link.policy.%d.trigger", rdbIdx), 1)
end

-- Callback function to rebuild VLANTermination instance
-- This is to handle VLAN configuration changes from others rather then ACS.
rebuildVlanTermInst = function(task)
    local node = task.data

    local instIdxList = {}
    -- build data model index list.
    for _, child in ipairs(node.children) do
        local pathName = child:getPath(true)
        if child.type == "object" and pathName then
            local instIdx = tonumber(string.match(pathName, pattVLANTerm))
            if instIdx then
                table.insert(instIdxList, instIdx)
            end
        end
    end

    local rdbIdxList = getRdbVlanIds(true) -- convert to instance index
    if table.concat(instIdxList, ",") == table.concat(rdbIdxList, ",") then
        -- No Vlan Rule changes.
        return
    end

    -- delete all of children data model object
    for _, child in ipairs(node.children) do
        if child.name ~= '0' then
            child.parent:deleteChild(child)
        end
    end

    -- add children
    for _, id in ipairs(rdbIdxList) do
        node:createDefaultChild(id)
    end
end

-- If rdb variable does not exist, create rdb variable
--
-- persist; if true, set rdb with persist flag.
setRdbIfNotExist = function(rdbName, value, persist)
    if luardb.get(rdbName) == nil then
        luardb.set(rdbName, value or "", (persist == true) and 'p' or nil)
    end
end

-- get list of rdb VLAN rules indexes
-- Note: the list is built via vlan.num
--
-- cvtToInstIdx: if true, convert RDB index to instance index
--             (rdb index starts from 0, instance index starts from 1)
--
-- return: array of rdb indexes
getRdbVlanIds = function(cvtToInstIdx)
    local retList = {}

     -- vlan.num => "last rdb index number" + 1
     -- Ex) If vlan list is 0,2,3, then vlan.num => 4
    local lastIdx = tonumber(luardb.get("vlan.num")) or 0
    local cvt = cvtToInstIdx and 1 or 0

    for i=0, (lastIdx - 1) do
        if luardb.get("vlan." .. i .. ".rule_name") then -- Same validation check with WEBUI.
            table.insert(retList, i + cvt)
        end
    end
    return retList
end
------------------------------------------------------------

return {
    -- RW:bool
    -- rdb variable: vlan.enable
    [subRootVlan .. xVendorPrefix .. "_EnableVLAN"] = {
        get = function(node, name)
            return 0, luardb.get("vlan.enable") or node.default
        end,
        set = function(node, name, value)
            if not value or (value ~= "1" and value ~= "0") then
                return CWMP.Error.InvalidParameterValue
            end
            luardb.set("vlan.enable", value)
            registerVLANTrigger()
            return 0
        end
    },

    -- RO:uint
    [subRootVlan .. "VLANTerminationNumberOfEntries"] = {
        get = function(node, name)
            return 0, hdlerUtil.getNumOfSubInstance(subRootVlan .. "VLANTermination.")
        end,
    };

    -- RW:object
    -- rdb variable: vlan.num
    [subRootVlan .. "VLANTermination"] = {
        init = function(node, name)
            node:setAccess("readwrite") -- Support AddObject/DeleteObject methods.

            for _, id in ipairs(getRdbVlanIds(true)) do -- true: convert rdb index to data model instance index.
                node:createDefaultChild(id)
            end

            -- register preSession callback to rebuild VLANTermination sub-objects.
            if client:isTaskQueued("preSession", rebuildVlanTermInst, node) ~= true then
                client:addTask("preSession", rebuildVlanTermInst, true, node)
            end
            return 0
        end,
        create = function(node, name)
            local newIdx = 1
            -- find smallest unused idx
            for loopIdx, instIdx in ipairs(getRdbVlanIds(true)) do -- true: convert rdb index to data model instance index.
                newIdx = loopIdx + 1
                if tonumber(loopIdx) ~= tonumber(instIdx) then
                    newIdx = newIdx - 1
                    break
                end
            end

            if newIdx > maxVlanRdbIdx + 1 then
                return CWMP.Error.ResourcesExceeded
            end
            local instance = node:createDefaultChild(newIdx)
            instance:recursiveInit()

            -- vlan.num => "last rdb index number" + 1
            -- Ex) If vlan list is 0,2,3, then vlan.num => 4
            luardb.set("vlan.num", math.max(tonumber(luardb.get("vlan.num")) or 0, newIdx))

            return 0, newIdx
        end,
    };

    -- RW:object
    -- rdb variable: vlan.#.
    [subRootVlan .. "VLANTermination.*"] = {
        delete = function(node, name)
            local instIdx = tonumber(string.match(name, pattVLANTerm)) or 0
            if instIdx < 1 then
                return 0
            end

            local rdbIdx = instIdx - 1
            for key, info in pairs(vlanTermParmList) do
                local rdbName = string.format("%s%d.%s", vlanRdbPrefix, rdbIdx, info.rdbName)
                luardb.unset(rdbName)
                if key == "VLANID" then
                    luardb.unset(string.format("%s%d.%s", vlanRdbPrefix, rdbIdx, "name"))
                    luardb.unset(string.format("%s%d.%s", vlanRdbPrefix, rdbIdx, "interface"))
                end
            end

            -- vlan.num => "last rdb index number" + 1
            -- Ex) If vlan list is 0,2,3, then vlan.num => 4
            local rdbIdxList = getRdbVlanIds(true) -- convert to instance index
            luardb.set("vlan.num", rdbIdxList[#rdbIdxList])
            registerVLANTrigger()
            return 0
        end,
    };

    [subRootVlan .. "VLANTermination.*.*"] = {
        init = function(node, name)
            local instIdx = tonumber(string.match(name, pattVLANTerm)) or 0
            local instName = string.match(name, ".+%.([^%.]+)$")
            local instRdbValidator = vlanTermParmList[instName]

            if instIdx < 1 then return 0 end
            if not instRdbValidator then
                return CWMP.Error.InvalidParameterName
            end

            local rdbName = string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), instRdbValidator.rdbName)
            setRdbIfNotExist(rdbName, node.default or "", true)

            if instName == "VLANID" then
                local rdbNameInterface = string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), "interface")
                local rdbNameName = string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), "name")
                local rdbNameId = tonumber(string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), "id"))

                setRdbIfNotExist(rdbNameInterface, "eth0", true)
                if rdbNameId then
                    setRdbIfNotExist(rdbNameName, string.format("eth0.%d", rdbNameId), true)
                else
                    setRdbIfNotExist(rdbNameName, "", true)
                end
            end

            return 0
        end,
        get = function(node, name)
            local instIdx = tonumber(string.match(name, pattVLANTerm)) or 0
            local instName = string.match(name, ".+%.([^%.]+)$")
            local instRdbValidator = vlanTermParmList[instName]

            if not instRdbValidator or instIdx < 1 then
                return CWMP.Error.InvalidParameterName
            end

            -- Starting index of rdb instance is 0. => (instIdx - 1)
            return 0, luardb.get(string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), instRdbValidator.rdbName)) or node.default
        end,
        set = function(node, name, value)
            local instIdx = tonumber(string.match(name, pattVLANTerm)) or 0
            local instName = string.match(name, ".+%.([^%.]+)$")
            local instRdbValidator = vlanTermParmList[instName]

            if not instRdbValidator or instIdx < 1 then
                return CWMP.Error.InvalidParameterName
            end

            if type(instRdbValidator.setVal_cb) == "function" and not instRdbValidator.setVal_cb(value) then
                return CWMP.Error.InvalidParameterValue, instRdbValidator.valErrMsg
            end

            local rdbName = string.format("%s%d.%s", vlanRdbPrefix, (instIdx - 1), instRdbValidator.rdbName)
            -- No changes, ignore.
            if luardb.get(rdbName) == tostring(value) then
                return 0
            end

            luardb.set(rdbName, value)
            registerVLANTrigger() -- trigger template in postSession

            if type(instRdbValidator.postSet_cb) == "function" then
                instRdbValidator.postSet_cb(instIdx, value)
            end
            return 0
        end
    };

    -- RW:string
    -- rdb variable: link.profile.#.vlan_index
    [subRootWwan .. "MapToVLAN"] = {
        get = function(node, name)
            local linkProfileIdx = tonumber(string.match(name, pattMapToVLAN)) or 0
            local linkProfileVLANIndex = tonumber(luardb.get(string.format("link.profile.%d.vlan_index", linkProfileIdx)))
            local retVal = ""

            if linkProfileVLANIndex then
                retVal = string.format("%s%d.", vlanTermObjPath, (linkProfileVLANIndex + 1)) -- VLAN index starts from 0. TR069 object index starts from 1.
            end
            return 0, retVal
        end,
        set = function(node, name, value)
            if luardb.get("vlan.enable") ~= "1" then
                return CWMP.Error.InvalidParameterValue, "VLAN service is disabled or no VLAN rule is configured"
            end

            local vlanIdx = -1
            local linkProfileIdx = tonumber(string.match(name, pattMapToVLAN)) or 0
            if linkProfileIdx < 1 then
                return CWMP.Error.InvalidParameterName
            end

            if string.trim(value) ~= "" then
                local lastIdx = tonumber(luardb.get("vlan.num")) or 0
                vlanIdx = (tonumber(string.match(value, pattVLANTerm)) or 0) - 1 -- converted to RDB VLAN index from data model instance index.
                if vlanIdx < 0 or vlanIdx >= lastIdx then
                    return CWMP.Error.InvalidParameterValue, "Invalid VLANTermination object path"
                end

                local vlanTermEnabled = luardb.get(string.format("%s%d.enable", vlanRdbPrefix, vlanIdx))
                if vlanTermEnabled ~= "1" then
                    return CWMP.Error.InvalidParameterValue, "VLAN Rule is not enabled"
                end
            end

            local rdbName = string.format("link.profile.%d.vlan_index", linkProfileIdx)
            -- No changes, ignore.
            if tonumber(luardb.get(rdbName)) == vlanIdx then
                return 0
            end
            luardb.set(rdbName, vlanIdx)

            if client:isTaskQueued("postSession", wwanTrigger_cb, linkProfileIdx) ~= true then
                client:addTask("postSession", wwanTrigger_cb, false, linkProfileIdx)
            end
            return 0
        end
    },
}


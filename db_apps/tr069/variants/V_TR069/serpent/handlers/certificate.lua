--[[
This script handles the following objects/parameters under Device.Security.

  CertificateNumberOfEntries.
  Certificate.{i}.

Copyright (C) 2018 NetComm Wireless Limited.
--]]

require("handlers.hdlerUtil")
require("Logger")
require("rdbobject")

------------------constant----------------------------
local certsDir = conf.net.ca_path or '/etc/ssl/certs'
local certScript = 'install_ca_cert.sh'

------------------local variable----------------------------
local logSubsystem = 'Certificate'
local subRoot=conf.topRoot..'.Security.'

-- we use rdbobjects to store certificates
-- tr069.certs.i.enable, file, serial, issuer, subject, not_before, not_after
local certsRdbPrefix = 'tr069.certs'
local certsClassConfig = { persist = true, idSelection = 'smallestUnused' }
local certsObjectClass = rdbobject.getClass(certsRdbPrefix, certsClassConfig)

Logger.addSubsystem(logSubsystem)

-- get the certificate object corresponding to node
local function getCertsObject(node)
    local id = tonumber(node.name)
    return certsObjectClass:getById(id)
end

-- map from rdbobject field to x509 certificate field
local certFields = {
    ['enable'] = '', -- no x509 field
    ['file'] = '', -- no x509 field
    ['serial'] = 'serial',
    ['issuer'] = 'issuer',
    ['subject'] = 'subject',
    ['not_before'] = 'notBefore',
    ['not_after'] = 'notAfter'
}

-- update a single rdbobject instance if field value changed
local function updateSingleCert(obj, newVals)
    for rdb, _ in pairs(certFields) do
        if obj[rdb] ~= newVals[rdb] then
            Logger.log(logSubsystem, 'info', 'update: ' .. rdb .. '=' .. newVals[rdb])
            obj[rdb] = newVals[rdb]
        end
    end
end

-- check if a certificate exists and is enabled
local function isCertEnabled(cert)
    return os.execute(certScript .. ' -c ' .. cert) == 0
end

-- map month in certificate (3-letter abbreviation) to number
local monthToNum = {
    ['Jan'] = 1,
    ['Feb'] = 2,
    ['Mar'] = 3,
    ['Apr'] = 4,
    ['May'] = 5,
    ['Jun'] = 6,
    ['Jul'] = 7,
    ['Aug'] = 8,
    ['Sep'] = 9,
    ['Oct'] = 10,
    ['Nov'] = 11,
    ['Dec'] = 12,
}

-- convert a certificate date/time to epoch seconds as a string
-- example certificate date/time: May 21 04:00:00 2002 GMT
local function certDateToEpoch(certDate)
    local M, D, h, m, s, Y = certDate:match('^(%w+)%s+(%d+)%s+(%d+):(%d+):(%d+)%s+(%d+)%s+GMT')
    if not M then
        return '0'
    end
    Logger.log(logSubsystem, 'debug', Y .. '/' .. M .. '/' .. D .. ' ' .. h .. ':' .. m .. ':' .. s)
    return Daemon.utcTimeToEpoch(Y, monthToNum[M], D, h, m, s)
end

-- get certificate attributes from a certificate file
local function getCertAttrs(cert)
    local newVals = {}
    local cmd = 'openssl x509 -noout -serial -issuer -subject -dates -in ' .. certsDir .. '/' .. cert .. ' 2>/dev/null'
    local output = Daemon.readCommandOutput(cmd)
    if not output or string.trim(output) == '' then
        Logger.log(logSubsystem, 'warning', 'Invalid certificate ' .. cert)
        return
    end
    --[[
Example output:
serial=023456
issuer= /C=US/O=GeoTrust Inc./CN=GeoTrust Global CA
subject= /C=US/O=GeoTrust Inc./CN=GeoTrust Global CA
notBefore=May 21 04:00:00 2002 GMT
notAfter=May 21 04:00:00 2022 GMT
    --]]
    Logger.log(logSubsystem, 'debug', 'cert attrs:\n' .. output)
    newVals.file = cert
    for line in output:gmatch('[^\r\n]+') do -- for each line
        local key, val = line:match('^%s*(%w+)%s*=%s*(.+)')
        if key then
            Logger.log(logSubsystem, 'debug', 'cert attr: ' .. key .. '=' .. val)
            for rdb, field in pairs(certFields) do
                if field == key then
                    if rdb == 'not_before' or rdb == 'not_after' then
                        val = certDateToEpoch(val)
                    end
                    newVals[rdb] = val
                end
            end
        end
    end
    newVals.enable = isCertEnabled(cert) and '1' or '0'
    return newVals
end

-- create a certificate rdbobject instance with given attribute values
local function createCertObject(vals)
    obj = certsObjectClass:new()
    for rdb, _ in pairs(certFields) do
        obj[rdb] = vals[rdb]
    end
    return obj
end

-- update the whole certificate rdbobject from file system
local function updateCertsObject()
    local cmd = 'find ' .. certsDir .. ' -type f -maxdepth 1 2>/dev/null'
    local output = Daemon.readCommandOutput(cmd)
    if not output then
        return
    end
    Logger.log(logSubsystem, 'debug', 'potential certs:\n' .. output)

    local fsObjs = {} -- record rdbobject instances that exist in file system
    for line in output:gmatch('[^\r\n]+') do
        local cert = line:match('^' .. certsDir .. '/(.+)')
        if cert then
            local newVals = getCertAttrs(cert)
            if newVals then -- skip invalid cert file
                local obj
                local objs = certsObjectClass:getByProperty('file', cert)
                if #objs > 0 then
                    Logger.log(logSubsystem, 'debug', 'update existing cert ' .. cert .. ' in rdbobject')
                    if #objs > 1 then
                        Logger.log(logSubsystem, 'warning', 'duplicate certificates in rdbobject')
                    end
                    obj = objs[1]
                    updateSingleCert(obj, newVals)
                else
                    Logger.log(logSubsystem, 'info', 'create new cert ' .. cert .. ' in rdbobject')
                    obj = createCertObject(newVals)
                end
                fsObjs[certsObjectClass:getId(obj)] = true
            end
        end
    end

    -- delete objects that do not have certificate files in file system
    for _, obj in ipairs(certsObjectClass:getAll()) do
        if not fsObjs[certsObjectClass:getId(obj)] then
            Logger.log(logSubsystem, 'info', 'delete cert ' .. obj.file .. ' from rdbobject')
            certsObjectClass:delete(obj)
        end
    end
end

-- this will be called before every session
local function certsUpdater()
    local node = paramTree:find(subRoot .. 'Certificate')
    updateCertsObject()

    Logger.log(logSubsystem, 'debug', 'remove existing children in param tree')
    for _, child in ipairs(node.children) do
        if child.name ~= '0' then
            child.parent:deleteChild(child)
        end
    end
    Logger.log(logSubsystem, 'debug', 'recreate children in param tree from rdbobject')
    local ids = certsObjectClass:getIds()
    for _, id in ipairs(ids) do
        node:createDefaultChild(id)
    end
end

------------------------------------------------------------

return {
    [subRoot .. 'CertificateNumberOfEntries'] = {
        get = function(node, name)
            local targetName = subRoot ..'Certificate'
            return 0, hdlerUtil.getNumOfSubInstance(targetName)
        end,
    },

    [subRoot .. 'Certificate'] = {
        init = function(node, name)
            Logger.log(logSubsystem, 'debug', 'Certificate init')
            node:setAccess('readwrite')
            if client:isTaskQueued('preSession', certsUpdater) ~= true then
                client:addTask('preSession', certsUpdater, true) -- run before every session
            end
            return 0
        end,

        create = function(node, name)
            return CWMP.Error.RequestDenied, 'Certificate should be added by Download RPC'
        end,
    },

    [subRoot .. 'Certificate.*'] = {
        delete = function(node, name)
            Logger.log(logSubsystem, 'debug', 'Certificate.* delete ' .. name)
            local obj = getCertsObject(node)
            local rval = os.execute(certScript .. ' -r ' .. obj.file)
            if rval ~= 0 then
                return CWMP.Error.InternalError, 'Failed to remove certificate ' .. obj.file
            end
            certsObjectClass:delete(obj)
            node.parent:deleteChild(node)
            return 0
        end,
    },

    [subRoot .. 'Certificate.*.Enable'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.enable or '0'
        end,

        set = function(node, name, value)
            local obj = getCertsObject(node.parent)
            local action = 'enable'
            local opt = ' -e '
            if value == '0' then
                action = 'disable'
                opt = ' -d '
            end
            local rval = os.execute(certScript .. opt .. obj.file)
            if rval ~= 0 then
                return CWMP.Error.InvalidParameterValue, 'Failed to ' .. action .. ' certificate ' .. obj.file
            end
            obj.enable = value
            return 0
        end,
    },

    [subRoot .. 'Certificate.*.SerialNumber'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.serial or ''
        end,
    },

    [subRoot .. 'Certificate.*.Issuer'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.issuer or ''
        end,
    },

    [subRoot .. 'Certificate.*.Subject'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.subject or ''
        end,
    },

    [subRoot .. 'Certificate.*.NotBefore'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.not_before or '0'
        end,
    },

    [subRoot .. 'Certificate.*.NotAfter'] = {
        get = function(node, name)
            local obj = getCertsObject(node.parent)
            return 0, obj.not_after or '0'
        end,
    },
}

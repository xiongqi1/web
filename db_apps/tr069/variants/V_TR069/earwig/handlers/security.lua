--[[
This script handles objects/parameters under Device.Security.

  Certificate.{i}.

Copyright (C) 2017 NetComm Wireless limited.
--]]

require("handlers.hdlerUtil")
require("Daemon")
require("Logger")

-----------------global constants---------------------------
CERT_NAME=conf.net.client_cert
SUB_ALT_DEFAULT_VALUE='N/A'
CERT_CACHE_TIMEOUT_IN_SECS=5
OPENSSL_CMD='/bin/openssl'
MONTH_MAP={Jan=1,Feb=2,Mar=3,Apr=4,May=5,Jun=6,Jul=7,
           Aug=8,Sep=9,Oct=10,Nov=11,Dec=12}

------------------global variables----------------------------
local logSubsystem = 'security'
Logger.debug = conf.log.debug
Logger.defaultLevel = conf.log.level
Logger.addSubsystem(logSubsystem)
local subRoot = conf.topRoot .. '.Security.'
local certLastUpdatedTime=nil
local certAttrs={}

-- Data Model attribute name to system attribute mapping
-- key = data model attr name
-- value = pattern to match
local dmAttrToPatternMap = { ['SerialNumber'] = 'Serial Number:%s+(%S+)',
                    ['NotBefore'] = 'Not Before: (.-)\n',
                    ['NotAfter'] = 'Not After : (.-)\n',
                    ['Issuer'] = 'Issuer: (.-)\n',
                    ['Subject'] = 'Subject: (.-)\n',
                    ['SignatureAlgorithm'] = 'Signature Algorithm: (.-)\n',
}

------------------local functions----------------------------

-- Convert time format of 'Dec  7 23:29:54 2017 GMT' into
-- epoch seconds.
-- @param time Time format as string
-- @return returns the time as epoch seconds
-- @TODO - if this is needed by other modules, move it to a
-- common place
local function convertCertTime(time)
    month,day,hour,min,sec,year =
        time:match('(%a+)%s(%d+) (%d+):(%d+):(%d+) (%d+) GMT')
    Logger.log(logSubsystem, 'debug', 'convert:output : ' .. tostring(day))
    month = MONTH_MAP[month]
    offset = os.time()-os.time(os.date("!*t"))
    time = tostring(os.time({day=day,month=month,year=year,hour=hour,min=min,sec=sec}) + offset)
    Logger.log(logSubsystem, 'debug', 'updated time in convertCertTime: '
        .. time)
    return time
end

-- @brief Conversion function for all the attributes if required
-- will be implemented here.
-- @param attrName attribute name
-- @param val value that has to be converted
-- @return returns the value converted
local function convert(attrName, val)
    if (attrName == 'NotAfter' or attrName == 'NotBefore') then
        -- Convert time format of 'Dec  7 23:29:54 2017 GMT' into
        -- epoch seconds.
        val = convertCertTime(val)
    end
    return val
end

-- @brief read and parse the certificate. Uses openssl to print
-- out contents of the certificate and parse the output.
-- Updates the global table with key as the attribute name and value the value
-- of the attribute in the parsed certificate
local function parseCertificate()
    local currTime = os.time()
    -- check if the request is received with in this time then
    -- return the cached variables
    if (certLastUpdatedTime ~= nil and
      os.difftime(currTime, certLastUpdatedTime) <= CERT_CACHE_TIMEOUT_IN_SECS) then
        Logger.log(logSubsystem, 'debug', 'Returning cached values')
        return
    end

    certAttrs={}
    -- Get the last modified date of the file in epoch seconds
    local mod_date=Daemon.readCommandOutput('date +%s -r ' .. CERT_NAME)
    certAttrs['LastModif']=mod_date
    Logger.log(logSubsystem, 'debug', 'mod_date: '.. mod_date)
    -- Subject Alternate Name is not present in these certificates
    certAttrs['SubjectAlt']=SUB_ALT_DEFAULT_VALUE
    local cmd_output=Daemon.readCommandOutput(OPENSSL_CMD .. ' x509 -in '
        .. CERT_NAME .. ' -text -noout')
    Logger.log(logSubsystem, 'debug', 'cert: ' .. CERT_NAME ..
        ' cmd_output: ' .. cmd_output)
    -- example output:
    -- Certificate:
    -- Data:
    --    Version: 1 (0x0)
    --    Serial Number: 12001765753338776301 (0xa68ed65d5534caed)
    --Signature Algorithm: sha256WithRSAEncryption
    --    Issuer: C=AU, ST=VIC, L=MEL, O=NetcommWireless, OU=NetcommWireless CA Unit/emailAddress=info@NetcommWireless.com, CN=NetcommWirelessCA
    --    Validity
    --        Not Before: Dec  7 23:29:54 2017 GMT
    --        Not After : Dec  2 23:29:54 2037 GMT
    --    Subject: C=AU, ST=VIC, L=MEL, O=NetcommWireless, OU=NetcommWireless CA Unit/emailAddress=info@NetcommWireless.com, CN=NetcommWirelessCA
    for attrName, pattern in pairs(dmAttrToPatternMap) do
        Logger.log(logSubsystem, 'debug', 'attrName: '.. attrName ..
            'pattern: ' .. pattern)
        val = cmd_output:match(pattern)
        Logger.log(logSubsystem, 'debug', 'attrName: '.. attrName ..
            'pattern: ' .. pattern .. ' val: ' .. val)
        if val then
            -- check if this has to be converted (conversion function)
            val = convert(attrName, val)
            certAttrs[attrName] = val
        end
    end

    -- update the cert attributes timestamp
    certLastUpdatedTime = os.time()
    Logger.log(logSubsystem, 'debug', 'updated cache at: ' .. certLastUpdatedTime)
end

-- @brief runs the 'Get' command for a given attribute and returns the value
-- @dmAttr Data Model Attribute name to get
-- @return returns the value of the given attribute
local function runGetCommand(dmAttr)
    Logger.log(logSubsystem, 'debug', ' attr: ' .. dmAttr)
    if CERT_NAME and CERT_NAME ~= 'None' then
	    -- Parse and update the certificate attributes if required
	    parseCertificate()
	    return certAttrs[dmAttr]
    else
	return ''
    end
end

------------------------------------------------------------

return {
    [subRoot .. 'Certificate.1.*'] = {
        get = function(node, name)
            return 0, runGetCommand(node.name)
        end,
        set = CWMP.Error.funcs.ReadOnly
    },
}

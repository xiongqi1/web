--[[
    NetComm OMA-DM Client

    bootstrap.lua
    Utilities for decoding and authenticating WAP bootstrap messages.

    Copyright Notice:
    Copyright (C) 2018 NetComm Wireless Limited.

    This file or portions thereof may not be copied or distributed in any form
    (including but not limited to printed or electronic forms and binary or object forms)
    without the expressed written consent of NetComm Wireless Limited.
    Copyright laws and International Treaties protect the contents of this file.
    Unauthorized use is prohibited.

    THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
    WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
]]

--[[
    TODO: This should be redone in C, against libwbxml directly, and
    translated straight into Add/Replace operations on the model.
    TODO: There's a huge number of potential fields and values that
    can pop up in a provisioning doc, and this code currently ignores
    most of it. This isn't necessarily a problem, but it should be
    kept in mind.
]]

bootstrap = {
    SEC_NETWPIN     = 0,
    SEC_USERPIN     = 1,
    SEC_USERNETWPIN = 2,
    SEC_USERPINMAC  = 3
}

local function findElements(root, name, attr, value)

    local index = 1
    return function()
        while root[index] do
            local elem = root[index]
            index = index + 1
            if elem.name == name and elem.attributes[attr] == value then
                return elem
            end
        end
        return nil
    end

end

local function findParameter(root, name)

    for _, elem in ipairs(root) do
        if elem.name == "parm" and elem.attributes.name == name then
            return elem.attributes.value
        end
    end
    return nil

end

function bootstrap.Decode(body)

    local doc = wbxml.Decode(body)

    if doc.langID ~= wbxml.LANG_PROV10 then
        error("doctype mismatch", 2)
    end
    if doc.charset ~= wbxml.CHARSET_UTF_8 then
        error("charset mismatch", 2)
    end

    local accts = {}
    for eDoc in findElements(doc, "wap-provisioningdoc", "version", "1.0") do
        for eApp in findElements(eDoc, "characteristic", "type", "APPLICATION") do
            local acct = {
                name     = findParameter(eApp, "NAME"),
                serverid = findParameter(eApp, "PROVIDER-ID"),
                addr     = findParameter(eApp, "ADDR"),
                auth     = {}
            }
            for eAuth in findElements(eApp, "characteristic", "type", "APPAUTH") do
                local level = findParameter(eAuth, "AAUTHLEVEL")
                if level == "APPSRV" then level = "SRVCRED" end
                if level == "CLIENT" then level = "CLCRED" end
                local type = findParameter(eAuth, "AAUTHTYPE")
                if not type or type == "DIGEST,MD5" then type = "DIGEST" end
                table.insert(acct.auth, {
                    level  = level,
                    type   = type,
                    name   = findParameter(eAuth, "AAUTHNAME"),
                    secret = findParameter(eAuth, "AAUTHSECRET"),
                    data   = findParameter(eAuth, "AAUTHDATA")
                })
            end
            table.insert(accts, acct)
        end
    end
    return accts

end


--[[
    DM Bootstrap Security References:
     - Section 5.7, OMA-TS-DM_Security-V1_2-20070209-A
     - Section 4.3, OMA-WAP-TS-ProvCont-V1_1-20090728-A
     - Section 5.2, OMA-WAP-TS-ProvBoot-V1_1-20090728-A
]]
function bootstrap.Verify(sec, body, mac)
    if sec == bootstrap.SEC_NETWPIN then
        return bootstrap.Verify_NETWPIN(body, mac)
    elseif sec == bootstrap.SEC_USERPIN then
        return bootstrap.Verify_USERPIN(body, mac)
    elseif sec == bootstrap.SEC_USERNETWPIN then
        return bootstrap.Verify_USERNETWPIN(body, mac)
    elseif sec == bootstrap.SEC_USERPINMAC then
        return bootstrap.Verify_USERPINMAC(body)
    end
    return false
end

--[[
    Translate security parameter value into something we recognise.
]]
function bootstrap.GetSecurityMode(sec)
    local valid = {
        ["0"]                 = 0,
        ["NETWPIN"]           = 0,
        ["NETWORKID"]         = 0,
        ["1"]                 = 1,
        ["USERPIN"]           = 1,
        ["2"]                 = 2,
        ["USERNETWPIN"]       = 2,
        ["USERPIN_NETWORKID"] = 2,
        ["3"]                 = 3,
        ["USERPINMAC"]        = 3
    }
    return sec and valid[sec]
end

--[[
    NETWPIN / NETWORKID
]]
function bootstrap.Verify_NETWPIN(body, mac)
    local imsi = rdb.Get("wwan.0.imsi.msin")
    if not imsi then
        logger.Error("No IMSI in RDB, cannot authenticate bootstrap message.")
        return false
    end
    return mac == bootstrap.CalculateMAC_NETWPIN(body, imsi)
end

function bootstrap.CalculateMAC_NETWPIN(body, imsi)
    return util.CalculateHMAC_SHA1(att.GenerateBootstrapKey(imsi), body)
end

--[[
    USERPIN
]]
function bootstrap.Verify_USERPIN(body, mac)
    local pin = rdb.Get("service.dm.bootstrap.userpin")
    if not pin then
        logger.Error("No USERPIN value in RDB, cannot authenticate bootstrap message.")
        return false
    end
    return mac == bootstrap.CalculateMAC_USERPIN(body, pin)
end

function bootstrap.CalculateMAC_USERPIN(body, pin)
    return util.CalculateHMAC_SHA1(pin, body)
end

--[[
    USERNETWPIN / USERPIN_NETWORKID
]]
function bootstrap.Verify_USERNETWPIN(body, mac)
    local imsi = rdb.Get("wwan.0.imsi.msin")
    if not imsi then
        logger.Error("No IMSI in RDB, cannot authenticate bootstrap message.")
        return false
    end
    local pin = rdb.Get("service.dm.bootstrap.userpin")
    if not pin then
        logger.Error("No USERPIN value in RDB, cannot authenticate bootstrap message.")
        return false
    end
    return mac == bootstrap.CalculateMAC_USERNETWPIN(body, pin, imsi)
end

function bootstrap.CalculateMAC_USERNETWPIN(body, pin, imsi)
    return util.CalculateHMAC_SHA1(pin .. ":" .. att.GenerateBootstrapKey(imsi), body)
end

--[[
    USERPINMAC
]]
function bootstrap.Verify_USERPINMAC(body)
    local pinmac = rdb.Get("service.dm.bootstrap.userpinmac")
    if not pinmac then
        logger.Error("No USERPINMAC value in RDB, cannot authenticate bootstrap message.")
        return false
    end
    return pinmac == bootstrap.CalculateMAC_USERPINMAC(body, pinmac:sub(1, #pinmac / 2))
end

function bootstrap.CalculateMAC_USERPINMAC(body, pin)
    local bytes = {util.CalculateHMAC_SHA1(pin, body):byte(1, #pin)}
    for k, v in pairs(bytes) do
        bytes[k] = (v % 10) + 48
    end
    return pin .. string.char(unpack(bytes))
end

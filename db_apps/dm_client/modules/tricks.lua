--[[
    NetComm OMA-DM Client

    tricks.lua
    Tools for development and testing.

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

if not rdb.IsFake and (rdb.Get("sw.version") or ""):match("%d$") then
    logger.Error("This looks like a release build, I don't think I should be here.")
    error("Silly rabbit", 3)
else
    local info = debug.getinfo(3, "Sl")
    logger.Warning("Loaded by '%s', line %s.", info.short_src:match("[^/]+$"), info.currentline)
end

require("package0")
require("bootstrap")
require("rebase")
require("wap")

tricks = {}

--[[
    Generate the body of a package0 message.
]]
function tricks.GeneratePackage0(serverid, sessionid, initiator, mode, password, nonce)

    local trigger  = util.EncodePackage0(serverid, sessionid, initiator, mode)
    local credsH   = util.ToBase64(util.CalculateMD5(serverid .. ":" .. password))
    local triggerH = util.ToBase64(util.CalculateMD5(trigger))

    return util.CalculateMD5(credsH .. ":" .. nonce .. ":" .. triggerH) .. trigger

end

--[[
    Generate the body of a WBXML bootstrap document.
]]
function tricks.GenerateBootstrap(name, uri, svName, svType, svKey, svData, clName, clType, clKey, clData)

    return util.FromHex("020B6A00C54601C6560187070603") .. name ..
           util.FromHex("000101C600015501873606037737") ..
           util.FromHex("000187380603") .. svName ..
           util.FromHex("000187070603") .. name ..
           util.FromHex("000187340603") .. uri ..
           util.FromHex("000187220603") .. name ..
           util.FromHex("0001C6570187300603") .. "APPSRV" ..
           util.FromHex("000187330603") .. svType ..
           util.FromHex("000187310603") .. svName ..
           util.FromHex("000187320603") .. svKey ..
           util.FromHex("0001872F0603") .. svData ..
           util.FromHex("000101C6570187300603") .. "CLIENT" ..
           util.FromHex("000187330603") .. clType ..
           util.FromHex("000187310603") .. clName ..
           util.FromHex("000187320603") .. clKey ..
           util.FromHex("0001872F0603") .. clData ..
           util.FromHex("0001010101")

end

--[[
    Manually insert a test message into the received messages queue in RDB.
]]
function tricks.DeliverMessage(msg)

    logger.Info("Delivering test message.")

    return rdb.Atomic(function()
        local id = (tonumber(rdb.Get("service.messaging.index")) or 0) + 1
        local db = rdb.Rebase("service.messaging." .. id)
        for k, v in pairs(msg) do
            db:Set(k, v)
        end
        rdb.Set("service.messaging.index", id)
    end)

end

--[[
    Manually insert a test notification into the received messages queue in RDB.
]]
function tricks.DeliverPackage0(pkg0)

    return tricks.DeliverMessage({
        wap_push_ct = wap.CT_PACKAGE0,
        wap_push_body = pkg0,
    })

end

--[[
    Manually insert a test bootstrap message into the received messages queue in RDB.
]]
function tricks.DeliverBootstrap(bs, sec, mac)

    return tricks.DeliverMessage({
        wap_push_ct = wap.CT_BOOTSTRAP,
        wap_push_ct_SEC = sec,
        wap_push_ct_MAC = mac and util.ToHex(mac),
        wap_push_body = bs
    })

end

--[[
    Generate a bootstrap MAC for the given security mode.
]]
function tricks.CalculateBootstrapMAC(bs, sec, pin)

    if sec == bootstrap.SEC_NETWPIN then
        return bootstrap.CalculateMAC_NETWPIN(bs, rdb.Get("wwan.0.imsi.msin"))
    elseif sec == bootstrap.SEC_USERPIN then
        return bootstrap.CalculateMAC_USERPIN(bs, pin)
    elseif sec == bootstrap.SEC_USERNETWPIN then
        return bootstrap.CalculateMAC_USERNETWPIN(bs, pin, rdb.Get("wwan.0.imsi.msin"))
    elseif sec == bootstrap.SEC_USERPINMAC then
        return bootstrap.CalculateMAC_USERPINMAC(bs, pin)
    end

end

--[[
    NetComm OMA-DM Client

    wap.lua
    Utilities for handling WAP messages.

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

require("accounts")
require("bootstrap")
require("package0")
require("rebase")

wap = {
    CT_PACKAGE0  = "application/vnd.syncml.notification",
    CT_BOOTSTRAP = "application/vnd.wap.connectivity-wbxml"
}

--[[
    List of fields that GetMessage should capture for each supported content-type.
]]
local msgFields = {
    [wap.CT_PACKAGE0] = {
        ct   = "wap_push_ct",
        body = "wap_push_body"
    },
    [wap.CT_BOOTSTRAP] = {
        ct = "wap_push_ct",
        sec = "wap_push_ct_SEC",
        mac = "wap_push_ct_MAC",
        body = "wap_push_body"
    }
}

--[[
    Check if the message at the specified ID has a content-type
    we recognise. If so, delete the message from RDB and return
    whatever fields are relevant to us.
]]
function wap.GetMessage(id)

    return rdb.Atomic(function()
        local db = rdb.Rebase("service.messaging." .. id)
        local contentType = db:Get("wap_push_ct")
        if contentType then
            local fields = msgFields[contentType]
            if fields then
                local msg = {}
                for field, key in pairs(fields) do
                    msg[field] = db:Get(key)
                end
                for key in db:Keys() do
                    db:Unset(key)
                end
                return msg
            end
        end
        return nil
    end)

end

--[[
    Returns an iterator that will retrieve any relevant messages
    between the current index and our last read index.
]]
function wap.GetMessages()

    local prev = tonumber(rdb.Get("service.dm.messaging.index")) or 0
    local last = tonumber(rdb.Get("service.messaging.index")) or 0

    return function()
        local msg
        while prev < last and not msg do
            prev = prev + 1
            msg = wap.GetMessage(prev)
        end
        rdb.Set("service.dm.messaging.index", prev)
        return msg
    end

end

--[[
    Process any waiting WAP messages.
]]
function wap.ProcessMessages()

    logger.Info("Checking for WAP messages.")
    for msg in wap.GetMessages() do
        if msg.ct == wap.CT_BOOTSTRAP then
            logger.Notice("Received bootstrap message.")
            wap.ProcessBootstrap(msg)
        elseif msg.ct == wap.CT_PACKAGE0 then
            logger.Notice("Received NI session notification.")
            wap.ProcessPackage0(msg)
        end
    end

end

--[[
    Attempt to authenticate a session notification, triggering
    the requested session if successful.
]]
function wap.ProcessPackage0(msg)

    logger.Info("Processing NI session notification.")
    logger.HexDebug("Message body", msg.body)

    local good, serverid, sessionid = pcall(package0.Decode, msg.body)
    if not good then
        logger.Error("Failed to decode notification: %s.", serverid)
    else
        logger.Debug("Server ID = '%s'", serverid)
        logger.Debug("Session ID = '%s'", sessionid)
        local acct = accounts.GetByServerID(serverid)
        if not acct then
            logger.Error("No account for server ID '%s' exists.", serverid)
        else
            local auth = acct:GetAuthByLevel("SRVCRED")
            if not auth then
                logger.Error("Failed to get credentials for server ID '%s'.", serverid)
            else
                local password = auth:Get("AAuthSecret")
                local nonce = util.FromBase64(auth:Get("AAuthData"))
                if not package0.Verify(msg.body, serverid, password, nonce) then
                    logger.Error("Failed to authenticate notification.")
                else
                    logger.Notice("Notification accepted, starting session with server '%s'.", serverid)
                    dm.DoSession(serverid, sessionid, nil, function(success)
                        logger.Log(success and logger.INFO or logger.ERROR,
                            "Network-initiated session %s.", success and "complete" or "failed")
                    end)
                end
            end
        end
    end

end

--[[
    Attempt to authenticate and then apply a bootstrap message,
    triggering a session to the first server account created
    if successful.
]]
function wap.ProcessBootstrap(msg)

    logger.Info("Processing bootstrap message.")
    logger.Debug("SEC = '%s'", msg.sec)
    logger.Debug("MAC = '%s'", msg.mac)
    logger.HexDebug("Message body", msg.body)
    local sec = bootstrap.GetSecurityMode(msg.sec)
    local mac = msg.mac and util.FromHex(msg.mac)
    if not bootstrap.Verify(sec, msg.body, mac) then
        logger.Error("Failed to authenticate bootstrap message.")
    else
        local good, bsAccts = pcall(bootstrap.Decode, msg.body)
        if not good then
            logger.Error("Failed to decode bootstrap message: %s.", bsAccts)
        else
            logger.TableDebug("Bootstrap contents", bsAccts, 0)
            if #bsAccts < 1 then
                logger.Error("Bootstrap message does not contain any accounts.")
            else
                local serverid
                for _, bsAcct in pairs(bsAccts) do
                    serverid = bsAcct.serverid
                    local acct, id = accounts.GetByServerID(serverid)
                    if not acct then
                        acct, id = accounts.Create(3), 3
                    end
                    logger.Notice("Bootstrapping account slot %i with server ID '%s'.", id, serverid)
                    acct:Delete()
                    acct:Set("Name",     bsAcct.name)
                    acct:Set("AppID",    "w7")
                    acct:Set("ServerID", bsAcct.serverid)
                    local addr = acct:CreateAddr()
                    addr:Set("Addr",     bsAcct.addr)
                    addr:Set("AddrType", "URI")
                    for _, bsAuth in pairs(bsAcct.auth) do
                        local auth = acct:CreateAuth()
                        auth:Set("AAuthLevel",  bsAuth.level)
                        auth:Set("AAuthType",   bsAuth.type)
                        auth:Set("AAuthName",   bsAuth.name)
                        auth:Set("AAuthSecret", bsAuth.secret)
                        auth:Set("AAuthData",   bsAuth.data)
                    end
                end
                logger.Notice("Bootstrap complete, starting session with server '%s'.", serverid)
                dm.DoSession(serverid, 1, nil, function(success)
                    logger.Log(success and logger.INFO or logger.ERROR,
                        "Bootstrap-initiated session %s.", success and "complete" or "failed")
                end)
            end
        end
    end

end

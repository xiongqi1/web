--[[
    NetComm OMA-DM Client

    ATT.lua
    Factory bootstrap accounts for AT&T.

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
require("event")

local function factoryBootstrap(acctid, serverid, uri, imei)
    logger.Notice("Bootstrapping account slot %i with server ID '%s'.", acctid, serverid)
    local acct = accounts.Create(acctid)
    acct:Delete()
    acct:Set("Name",     serverid)
    acct:Set("AppID",    "w7")
    acct:Set("ServerID", serverid)
    local addr = acct:CreateAddr()
    addr:Set("Addr",     uri)
    addr:Set("AddrType", "URI")
    local auth = acct:CreateAuth()
    auth:Set("AAuthLevel",  "CLCRED")
    auth:Set("AAuthType",   "DIGEST")
    auth:Set("AAuthName",   imei)
    auth:Set("AAuthSecret", att.GeneratePassword(imei, serverid, true))
    auth:Set("AAuthData",   "bnVsbA==")
    local auth = acct:CreateAuth()
    auth:Set("AAuthLevel",  "SRVCRED")
    auth:Set("AAuthType",   "DIGEST")
    auth:Set("AAuthName",   serverid)
    auth:Set("AAuthSecret", att.GeneratePassword(imei, serverid, false))
    auth:Set("AAuthData",   "bnVsbA==")
end

logger.Info("Waiting for IMEI to become available before checking accounts.")
event.Subscribe("GotIMEI", function(imei)
    logger.Info("Got IMEI, checking accounts.")
    if not accounts.Get(1) then
        factoryBootstrap(1, "Cingular", "https://xdm.wireless.att.com/oma", imei)
    end
    if not accounts.Get(2) then
        factoryBootstrap(2, "ATTLabA", "https://xdmua.wireless.labs.att.com/oma", imei)
    end
end)

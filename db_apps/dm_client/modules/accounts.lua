--[[
    NetComm OMA-DM Client

    accounts.lua
    RDB-backed storage for DM account information.

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

require("index")
require("struct")

local BASE = "service.dm.acc"

local Account = Class(Struct)

local fieldsAcct = {
    Name     = "name",
    AppID    = "appid",
    ServerID = "serverid"
}

local fieldsAddr = {
    Addr     = "addr",
    Port     = "port",
    AddrType = "type"
}

local fieldsAuth = {
    AAuthLevel  = "level",
    AAuthType   = "type",
    AAuthName   = "name",
    AAuthSecret = "secret",
    AAuthData   = "data"
}

local function buildKey(...) return table.concat({...}, ".") end

accounts = {}

function accounts.Get(id)

    if Index(BASE):Contains(id) then
        return Account(buildKey(BASE, id))
    end
    return nil

end

function accounts.GetByServerID(serverid)

    for id, acct in accounts.ForEach() do
        if acct:Get("ServerID") == serverid then
            return acct, id
        end
    end
    return nil

end

function accounts.Create(id)

    if not id then
        return Account(buildKey(BASE, Index(BASE):Allocate()))
    else
        Index(BASE):Add(id)
        return Account(buildKey(BASE, id))
    end

end

function accounts.Delete(id)

    local acctIndex = Index(BASE)
    if acctIndex:Contains(id) then
        Account(buildKey(BASE, id)):Delete()
        acctIndex:Delete(id)
    end

end

function accounts.DeleteAll()

    local acctIndex = Index(BASE)
    for id in acctIndex:ForEach() do
        Account(buildKey(BASE, id)):Delete()
        acctIndex:LazyDelete(id)
    end
    acctIndex:Write()

end

function accounts.ForEach()

    local getNext = Index(BASE):ForEach()
    return function()
        local id = getNext()
        if id then
            return id, Account(buildKey(BASE, id))
        end
        return nil
    end

end

function Account.__methods:__init(base)

    self.__base.__init(self, base, fieldsAcct)

    self.addrBase = buildKey(self.base, "addr")
    self.authBase = buildKey(self.base, "auth")

end

function Account.__methods:GetAddr(id)

    if Index(self.addrBase):Contains(id) then
        return Struct(buildKey(self.addrBase, id), fieldsAddr)
    end
    return nil

end

function Account.__methods:CreateAddr()

    return Struct(buildKey(self.addrBase, Index(self.addrBase):Allocate()), fieldsAddr)

end

function Account.__methods:DeleteAddr(id)

    local addrIndex = Index(self.addrBase)
    if addrIndex:Contains(id) then
        Struct(buildKey(self.addrBase, id), fieldsAddr):Delete()
        addrIndex:Delete(id)
    end

end

function Account.__methods:ForEachAddr()

    local getNext = Index(self.addrBase):ForEach()
    return function()
        local id = getNext()
        if id then
            return id, Struct(buildKey(self.addrBase, id), fieldsAddr)
        end
        return nil
    end

end

function Account.__methods:GetAuth(id)

    if Index(self.authBase):Contains(id) then
        return Struct(buildKey(self.authBase, id), fieldsAuth)
    end
    return nil

end

function Account.__methods:GetAuthByLevel(level)

    for _, auth in self:ForEachAuth() do
        if auth:Get("AAuthLevel") == level then
            return auth
        end
    end
    return nil

end

function Account.__methods:CreateAuth()

    return Struct(buildKey(self.authBase, Index(self.authBase):Allocate()), fieldsAuth)

end

function Account.__methods:DeleteAuth(id)

    local authIndex = Index(self.authBase)
    if authIndex:Contains(id) then
        Struct(buildKey(self.authBase, id), fieldsAuth):Delete()
        authIndex:Delete(id)
    end

end

function Account.__methods:ForEachAuth()

    local getNext = Index(self.authBase):ForEach()
    return function()
        local id = getNext()
        if id then
            return id, Struct(buildKey(self.authBase, id), fieldsAuth)
        end
        return nil
    end

end

function Account.__methods:Delete()

    self.__base.Delete(self)

    local addrIndex = Index(self.addrBase)
    for id in addrIndex:ForEach() do
        Struct(buildKey(self.addrBase, id), fieldsAddr):Delete()
        addrIndex:LazyDelete(id)
    end
    addrIndex:Write()

    local authIndex = Index(self.authBase)
    for id in authIndex:ForEach() do
        Struct(buildKey(self.authBase, id), fieldsAuth):Delete()
        authIndex:LazyDelete(id)
    end
    authIndex:Write()

end

--[[
    NetComm OMA-DM Client

    DmAcc.lua
    Server account management object.

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

local NAME = "DmAcc"
local URN  = "urn:oma:mo:oma-dm-dmacc:1.0"
local URI  = "./DmAcc"

local DmAcc = {}

RegisterObject(NAME, DmAcc)

function DmAcc:Initialise()   return STATUS.NoError end
function DmAcc:SessionStart() return STATUS.NoError end
function DmAcc:SessionEnd()   return STATUS.NoError end
function DmAcc:Shutdown()     return STATUS.NoError end

function DmAcc:SetACL()   return STATUS.NotAllowed end
function DmAcc:SetValue() return STATUS.NotAllowed end
function DmAcc:Rename()   return STATUS.NotAllowed end
function DmAcc:Delete()   return STATUS.NotAllowed end
function DmAcc:Execute()  return STATUS.NotAllowed end

function DmAcc:FindURN(urn)

    local uris = {}
    if urn == URN then
        for id in accounts.ForEach() do
            table.insert(uris, URI .. "/" .. id)
        end
    end
    return STATUS.NoError, uris

end

function DmAcc:GetType(path)

    return self:DiveIntoAccount(path,
         -- URI: ./DmAcc
        function ()
            return STATUS.NoError, TYPE.Interior
        end,
         -- URI: ./DmAcc/*
        function (acct)
            return STATUS.NoError, TYPE.Interior
        end,
         -- URI: ./DmAcc/*/*
        function (acct, field)
            return STATUS.NoError, TYPE.Leaf
        end,
         -- URI: ./DmAcc/*/App*
        function (acct, iterator)
            return STATUS.NoError, TYPE.Interior
        end,
         -- URI: ./DmAcc/*/App*/*
        function (acct, app)
            return STATUS.NoError, TYPE.Interior
        end,
         -- URI: ./DmAcc/*/App*/*/*
        function (acct, app, field)
            return STATUS.NoError, TYPE.Leaf
        end
    )

end

function DmAcc:GetACL(path)

    return self:DiveIntoAccount(path,
         -- URI: ./DmAcc
        function ()
            return STATUS.NoError, "Get=*"
        end,
         -- URI: ./DmAcc/*
        function (acct)
            return STATUS.NoError, "Get=" .. acct:Get("ServerID")
        end,
         -- URI: ./DmAcc/*/*
        function (acct, field)
            return STATUS.NoError, "Get=" .. acct:Get("ServerID")
        end,
         -- URI: ./DmAcc/*/App*
        function (acct, iterator)
            return STATUS.NoError, "Get=" .. acct:Get("ServerID")
        end,
         -- URI: ./DmAcc/*/App*/*
        function (acct, app)
            return STATUS.NoError, "Get=" .. acct:Get("ServerID")
        end,
         -- URI: ./DmAcc/*/App*/*/*
        function (acct, app, field)
            if field == "AAuthSecret" or field == "AAuthData" then
                return STATUS.NoError, ""
            end
            return STATUS.NoError, "Get=" .. acct:Get("ServerID")
        end
    )

end

function DmAcc:GetValue(path)

    return self:DiveIntoAccount(path,
         -- URI: ./DmAcc
        function ()
            return STATUS.NoError, "node", nil, self:ConcatIDs(accounts.ForEach())
        end,
         -- URI: ./DmAcc/*
        function (acct)
            return STATUS.NoError, "node", URN, self:ConcatIDs(acct:ForEach() .. "/AppAddr/AppAuth")
        end,
         -- URI: ./DmAcc/*/*
        function (acct, field)
            return STATUS.NoError, "chr", "text/plain", acct:Get(field)
        end,
         -- URI: ./DmAcc/*/App*
        function (acct, iterator)
            return STATUS.NoError, "node", nil, self:ConcatIDs(iterator)
        end,
         -- URI: ./DmAcc/*/App*/*
        function (acct, app)
            return STATUS.NoError, "node", URN, self:ConcatIDs(app:ForEach())
        end,
         -- URI: ./DmAcc/*/App*/*/*
        function (acct, app, field)
            if field == "AAuthData" then
                return STATUS.NoError, "bin", nil, util.FromBase64(app:Get(field))
            end
            return STATUS.NoError, "chr", "text/plain", app:Get(field)
        end
    )

end

function DmAcc:SetValue(path, format, type, value)

    return self:DiveIntoAccount(path,
         -- URI: ./DmAcc
        function () return STATUS.NotAllowed end,
         -- URI: ./DmAcc/*
        function (acct) return STATUS.NotAllowed end,
         -- URI: ./DmAcc/*/*
        function (acct, field) return STATUS.NotAllowed end,
         -- URI: ./DmAcc/*/App*
        function (acct, iterator) return STATUS.NotAllowed end,
         -- URI: ./DmAcc/*/App*/*
        function (acct, app) return STATUS.NotAllowed end,
         -- URI: ./DmAcc/*/App*/*/*
        function (acct, app, field)
            if field == "AAuthData" then
                value = util.ToBase64(value)
            end
            app:Set(field, value)
            return STATUS.NoError
        end
    )

end

function DmAcc:DiveIntoAccount(path, cbAcctList, cbAcctRoot, cbAcctField, cbAppList, cbAppRoot, cbAppField)

    if path:HasNext() then
        local acct = accounts.Get(path:Next():Segment())
        if acct then
            if path:HasNext() then
                if acct:Get(path:Next():Segment()) then
                    if not path:HasNext() then
                        return cbAcctField(acct, path:Segment())
                    end
                    return STATUS.NotFound
                end
                if path:Segment() == "AppAddr" then
                    return self:DiveIntoStruct(path, acct, "GetAddr", "ForEachAddr", cbAppList, cbAppRoot, cbAppField)
                end
                if path:Segment() == "AppAuth" then
                    return self:DiveIntoStruct(path, acct, "GetAuth", "ForEachAuth", cbAppList, cbAppRoot, cbAppField)
                end
                return STATUS.NotFound
            end
            return cbAcctRoot(acct)
        end
        return STATUS.NotFound
    end
    return cbAcctList()

end

function DmAcc:DiveIntoStruct(path, acct, fnGet, fnIter, cbAppList, cbAppRoot, cbAppField)

    if path:HasNext() then
        local addr = acct[fnGet](acct, path:Next():Segment())
        if addr then
            if path:HasNext() then
                if addr:Get(path:Next():Segment()) then
                    return cbAppField(acct, addr, path:Segment())
                end
                return STATUS.NotFound
            end
            return cbAppRoot(acct, addr)
        end
        return STATUS.NotFound
    end
    return cbAppList(acct, acct[fnIter](acct))

end

function DmAcc:ConcatIDs(iterator)

    local ids = {}
    for id in iterator do
        table.insert(ids, id)
    end
    return table.concat(ids, "/")

end

--[[
    NetComm OMA-DM Client

    node.lua
    DM node base class.

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

Node = Class()

function Node.__methods:__init(name, acl, urn)

    self.name = name
    self.path = "./" .. self.name
    self.urn  = urn
    self.acl  = acl

end

function Node.__methods:AddMethod(op, fn)

    self[op] = fn
    return self

end

function Node.__methods:SetPath(base)

    self.path = base .. "/" .. self.name

end

function Node.__methods:FindURN(urn, tbl)

    if urn == self.urn then
        table.insert(tbl, self.path)
    end
    return STATUS.NoError

end

function Node.__methods:TryOperation(op, path, ...)

    if path:HasNext() then
        return STATUS.NotFound
    end
    if not self[op] then
        return STATUS.NotAllowed
    end
    return self[op](self, path, ...)

end

function Node.__methods:GetACL(path)

    return STATUS.NoError, self.acl

end

function Node.__methods:SetACL(path, acl)

    self.acl = acl
    return STATUS.NoError

end

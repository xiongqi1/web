--[[
    NetComm OMA-DM Client

    node-interior.lua
    DM interior node.

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

require("node")

Node.Interior = Class(Node)

function Node.Interior.__methods:__init(name, acl, urn, ...)

    self.__base.__init(self, name, acl, urn)
    self.children = {}
    for _, node in pairs({...}) do
        self.children[node.name] = node
    end

end

function Node.Interior.__methods:SetPath(base)

    self.path = base .. "/" .. self.name
    for _, child in pairs(self.children) do
        child:SetPath(self.path)
    end

end

function Node.Interior.__methods:FindURN(urn, tbl)

    if urn == self.urn then
        table.insert(tbl, self.path)
    else
        for _, child in pairs(self.children) do
            local rc = child:FindURN(urn, tbl)
            if not IsSuccess(rc) then
                return rc
            end
        end
    end
    return STATUS.NoError

end

function Node.Interior.__methods:TryOperation(op, path, ...)

    if path:HasNext() then
        local child = self.children[path:Next():Segment()]
        if child then
            return child:TryOperation(op, path, ...)
        end
        return STATUS.NotFound
    end
    if not self[op] then
        return STATUS.NotAllowed
    end
    return self[op](self, path, ...)

end

function Node.Interior.__methods:Children()

    local names = {}
    for name, _ in pairs(self.children) do
        table.insert(names, name)
    end
    return names

end

function Node.Interior.__methods:ListChildren()

    return table.concat(self:Children(), "/")

end

function Node.Interior.__methods:GetType(resolve, path)

    return STATUS.NoError, TYPE.Interior

end

function Node.Interior.__methods:GetValue(path)

    return STATUS.NoError, "node", self.urn, self:ListChildren()

end

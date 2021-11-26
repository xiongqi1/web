--[[
    NetComm OMA-DM Client

    index.lua
    Helper class for managing object instances.

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

require("rdb")

Index = Class()

function Index.__methods:__init(where)

    self.where = where .. ".index"
    self:Read()

end

function Index.__methods:Read()

    local set = {}
    local str = rdb.Get(self.where)
    if str then
        for id in str:gmatch("%d+") do
            set[tonumber(id)] = true
        end
    end
    self.set = set

end

function Index.__methods:Write()

    local str = next(self.set)
    for id in next, self.set, str do
        str = str .. "," .. id
    end
    if str then
        rdb.Set(self.where, str, rdb.PERSIST)
    else
        rdb.Unset(self.where)
    end

end

function Index.__methods:Add(id)

    id = tonumber(id)
    if not self.set[id] then
        self.set[id] = true
        self:Write()
    end

end

function Index.__methods:Delete(id)

    id = tonumber(id)
    if self.set[id] then
        self.set[id] = nil
        self:Write()
    end

end

function Index.__methods:Allocate()

    local id = self:LazyAllocate()
    self:Write()
    return id

end

function Index.__methods:LazyAdd(id)

    self.set[tonumber(id)] = true

end

function Index.__methods:LazyDelete(id)

    self.set[tonumber(id)] = nil

end

function Index.__methods:LazyAllocate()

    local id = #self.set + 1
    self.set[id] = true
    return id

end

function Index.__methods:ForEach()

    local prev = nil
    return function()
        prev = next(self.set, prev)
        return prev
    end

end

function Index.__methods:Contains(id)

    return self.set[tonumber(id)]

end

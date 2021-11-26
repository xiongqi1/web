--[[
    NetComm OMA-DM Client

    object.lua
    Base class for DM management objects.

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

require("resolve")
require("node-interior")

Object = Class()

function Object.__methods:__init(...)

    self:SetRoot(Node.Interior(...))
    self.name = self.root.name
    self.path = "./" .. self.name

    RegisterObject(self.name, self)

end

function Object.__methods:SetRoot(root)

    self.root = root
    if root then
        root:SetPath(".")
    end

end

function Object.__methods:FindURN(urn)

    local tbl = {}
    local rc = self.root:FindURN(urn, tbl)
    return rc, tbl

end

function Object.__methods:Initialise()   return STATUS.NoError end
function Object.__methods:SessionStart() return STATUS.NoError end
function Object.__methods:SessionEnd()   return STATUS.NoError end
function Object.__methods:Shutdown()     return STATUS.NoError end

function Object.__methods:GetType(...)  return self.root:TryOperation("GetType", ...) end
function Object.__methods:GetACL(...)   return self.root:TryOperation("GetACL", ...) end
function Object.__methods:SetACL(...)   return self.root:TryOperation("SetACL", ...) end
function Object.__methods:GetValue(...) return self.root:TryOperation("GetValue", ...) end
function Object.__methods:SetValue(...) return self.root:TryOperation("SetValue", ...) end
function Object.__methods:Rename(...)   return self.root:TryOperation("Rename", ...) end
function Object.__methods:Delete(...)   return self.root:TryOperation("Delete", ...) end
function Object.__methods:Execute(...)  return self.root:TryOperation("Execute", ...) end

--[[
    NetComm OMA-DM Client

    node-leaf.lua
    DM leaf node base class.

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

Node.Leaf = Class(Node)

local DEFAULT_FORMAT = "chr"
local DEFAULT_TYPE = "text/plain"

function Node.Leaf.__methods:__init(name, acl, value)

    self.__base.__init(self, name, acl, nil)
    self.value = value

end

function Node.Leaf.__methods:GetType(path)

    return STATUS.NoError, TYPE.Leaf

end

function Node.Leaf.__methods:GetValue(path)

    return STATUS.NoError, DEFAULT_FORMAT, DEFAULT_TYPE, self.value

end

function Node.Leaf.__methods:SetValue(path, format, type, value)

    if format and format ~= DEFAULT_FORMAT then
        logger.Error("%s: Unexpected format '%s'.", self.path, format)
        return STATUS.CommandNotImplemented
    end

    if type and type ~= DEFAULT_TYPE then
        logger.Error("%s: Unexpected type '%s'.", self.path, format)
        return STATUS.CommandNotImplemented
    end

    self.value = value
    return STATUS.NoError

end


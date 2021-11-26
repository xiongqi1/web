--[[
    NetComm OMA-DM Client

    rebase.lua
    Helper class for RDB.

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

rdb.Rebase = Class()

function rdb.Rebase.__methods:__init(base)
    if type(base) == "string" then
        self.base = base:sub(-1) == "." and base or base .. "."
    else
        error("expected string for first argument, got " .. type(base), 2)
    end
end

function rdb.Rebase.__methods:Get(key)             return rdb.Get(self.base .. key) end
function rdb.Rebase.__methods:Set(key, val, flags) return rdb.Set(self.base .. key, val, flags) end
function rdb.Rebase.__methods:Unset(key)           return rdb.Unset(self.base .. key, val) end
function rdb.Rebase.__methods:GetFlags(key)        return rdb.GetFlags(self.base .. key) end
function rdb.Rebase.__methods:SetFlags(key, flags) return rdb.SetFlags(self.base .. key, flags) end
function rdb.Rebase.__methods:GetNumber(key, def)  return rdb.GetNumber(self.base .. key, def) end
function rdb.Rebase.__methods:Unwatch(key, tok)    return rdb.Unwatch(self.base .. key, tok) end
function rdb.Rebase.__methods:Path(key)            return self.base .. key end

function rdb.Rebase.__methods:Watch(key, fn, immediate)
    return rdb.Watch(self.base .. key, function(k, v)
        fn(k:sub(#self.base + 1), v)
    end, immediate)
end

function rdb.Rebase.__methods:WatchTrigger(key, fn, immediate)
    return rdb.WatchTrigger(self.base .. key, function(k, v)
        fn(k:sub(#self.base + 1), v)
    end, immediate)
end

function rdb.Rebase.__methods:Keys(match)
    local iter = rdb.Keys(match and (self.base .. match) or self.base)
    return function()
        local key = iter()
        return key and key:sub(#self.base + 1)
    end
end

function rdb.Rebase.__methods:Pairs(match)
    local iter = rdb.Pairs(match and (self.base .. match) or self.base)
    return function()
        local key, val = iter()
        return key and key:sub(#self.base + 1), val
    end
end

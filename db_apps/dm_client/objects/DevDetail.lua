--[[
    NetComm OMA-DM Client

    DevDetail.lua
    Device details management object.

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

require("object")
require("node-interior")
require("node-leaf")
require("node-rdb")

local URI = "DevDetail"
local URN = "urn:oma:mo:oma-dm-devdetail:1.0"

local function formatDate(when)
    if not when then
        return ""
    end
    return os.date("%Y-%m-%dT%H:%M:%S", when) .. os.date("%z", when):gsub("(%d%d)(%d%d)", "%1:%2")
end

local object = Object(URI, "Get=*", URN,
    Node.Leaf("DevTyp", nil, "WHP"),
    Node.Leaf("OEM",    nil, "NetComm"),
    Node.RDB("FwV",     nil, "sw.version"),
    Node.RDB("SwV",     nil, "sw.version"),
    Node.RDB("HwV",     nil, "system.product.hwver"),
    Node.Leaf("LrgObj", nil, "false"),
    Node.Interior("URI", nil, nil,
        Node.Leaf("MaxDepth",  nil, "0"),
        Node.Leaf("MaxTotLen", nil, "0"),
        Node.Leaf("MaxSegLen", nil, "0")
    ),
    Node.Interior("Ext", nil, nil,
        Node.Leaf("OSName",             nil, "NetComm"),
        Node.RDB("OSVersion",           nil, "sw.version"),
        Node.RDB("OrigFwV",             nil, "sw.original"),
        Node.RDB("PreFwV",              nil, "sw.previous"),
        Node.Leaf("InitActivationDate", nil):AddMethod("GetValue", function()
            local when = rdb.GetNumber("service.dm.initialdate")
            return STATUS.NoError, "chr", "text/plain", formatDate(when)
        end),
        Node.Leaf("LastUpdateTime", nil):AddMethod("GetValue", function()
            local when = rdb.GetNumber("service.dm.lastupdate")
            return STATUS.NoError, "chr", "text/plain", formatDate(when)
        end),
        Node.Interior("AttAccessParams", nil, nil,
            Node.Interior("DefaultApn", nil, nil,
                Node.Leaf("Apn", "Get=*&Replace=*"):AddMethod("GetValue", function()
                    return STATUS.NoError, "chr", "text/plain", rdb.Get("link.profile.1.apn") or ""
                end):AddMethod("SetValue", function(self, path, fmt, typ, val)
                    rdb.Set("link.profile.1.apn", val)
                    rdb.Set("link.profile.1.apn_extra", os.time(), rdb.PERSIST)
                    rdb.Set("link.profile.1.writeflag", "1")
                    return STATUS.NoError
                end),
                Node.Leaf("Extra", nil):AddMethod("GetValue", function()
                    local when = formatDate(rdb.GetNumber("link.profile.1.apn_extra"))
                    return STATUS.NoError, "chr", "text/plain", "ModifiedTimeStamp=" .. when
                end)
            )
        )
    )
)

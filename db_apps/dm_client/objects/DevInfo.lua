--[[
    NetComm OMA-DM Client

    DevInfo.lua
    Device info management object.

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

local URI = "DevInfo"
local URN = "urn:oma:mo:oma-dm-devinfo:1.0"

local object = Object(URI, "Get=*", URN,
    Node.RDB("Mod",    nil, "system.product.model"),
    Node.Leaf("Man",   nil, "NetComm"),
    Node.Leaf("DmV",   nil, "DM1.2"),
    Node.Leaf("Lang",  nil, "EN-US"),
    Node.Leaf("DevId", nil):AddMethod("GetValue", function()
        local val = rdb.Get("wwan.0.imei")
        return STATUS.NoError, "chr", "text/plain", val and ("IMEI:" .. val) or ""
    end)
)

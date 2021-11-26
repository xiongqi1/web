--[[
    NetComm OMA-DM Client

    Test.lua
    Dummy object for testing.

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
require("node")
require("node-interior")
require("node-leaf")

local URI = "Test"
local URN = "urn:test:1.0"

local function ExecutableNode(name)
    local node = Node(name, "Exec=*")
    node.Execute = function(self, path, arg, corr)
        logger.Notice("%s: Execute('%s', '%s')", path:Complete(), arg, corr)
        return STATUS.NoError
    end
    node.GetType = function(self, path)
        return STATUS.NoError, TYPE.Leaf
    end
    return node
end

local function CorrelatorNode(name)
    local node = ExecutableNode(name)
    node.Execute = function(self, path, arg, corr)
        logger.Notice("%s: Execute('%s', '%s')", path:Complete(), arg, corr)
        dm.DoSession(dm.GetServerID(), 1, {
            type       = URN,
            source     = path:Complete(),
            format     = "int",
            data       = 200,
            correlator = corr
        }, function (success)
            logger.Notice("%s: Alert session %s.", path:Complete(), success and "succeeded" or "failed")
        end)
        return STATUS.NoError
    end
    return node
end

local object = Object(URI, URN, "Get=*",
    -- This node supports no commands and should return 405 (Not Allowed) when accessed.
    Node("NotAllowed"),
    -- This node permits no access and should return 425 (Permission Denied) when accessed.
    Node("PermissionDenied", ""),
    -- This leaf node permits Get and Replace.
    Node.Leaf("ReplaceMe", "Get=*&Replace=*", "test data"),
    -- This interior node permits ACL changes to its children.
    Node.Interior("Interior", "Get=*&Replace=*", nil,
        Node.Leaf("DisownMe", "Get=*", "test data")
    ),
    -- This leaf can be executed.
    ExecutableNode("ExecuteMe"),
    -- This leaf can be executed, resulting in a generic alert.
    CorrelatorNode("CorrelateMe")
)

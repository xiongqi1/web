----
-- Copyright (C) 2012 NetComm Wireless Limited.
--
-- This file or portions thereof may not be copied or distributed in any form
-- (including but not limited to printed or electronic forms and binary or object forms)
-- without the expressed written consent of NetComm Wireless Limited.
-- Copyright laws and International Treaties protect the contents of this file.
-- Unauthorized use is prohibited.
--
-- THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
-- FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
-- NETCOMM WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
-- INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
-- BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
-- OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
-- AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
-- THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
-- SUCH DAMAGE.
----
require('CWMP')
require('CWMP.Object')
require('CWMP.Array')

CWMP.Message = {}

-- metatable for message instances
local imt = {}

function imt.__tostring(self)
	str = 'CWMP Version: ' .. self.version .. '\nCWMP Type: ' .. self.type .. '\n'
	if self.id then str = str .. 'ID: ' .. self.id .. '\n' end
	if self.holdRequests then str = str .. 'HoldRequests: ' .. tostring(self.holdRequests) .. '\n' end
	if self.supportedCWMPVersions then str = str .. 'SupportedCWMPVersions: ' .. self.supportedCWMPVersions .. '\n' end
	if self.useCWMPVersion then str = str .. 'UseCWMPVersion: ' .. self.useCWMPVersion .. '\n' end
--	str = str .. self:toXML(0)
	str = str .. tostring(self.message)
	return str
end


local function isFault(self)
	return (self.type == 'Fault')
end

-- create a new message instance
function CWMP.Message.new(type)
	assert(CWMP.typeExists(type), 'Unknown CWMP message type "' .. type .. '".')
	local msg = {
		id = nil,
		holdRequests = nil,
		version = '1.1',
		type = type,
		message = CWMP.Object.new(type, type, true),
		isFault = isFault
	}
	setmetatable(msg, imt)
	return msg
end


-- set message CWMP protocol version
function CWMP.Message.setVersion(self, version)
	assert(CWMP.versionExists(version), 'Unknown CWMP version "' .. version .. '".')
	self.version = version
end

-- set-up as reply to a requesting message instance
function CWMP.Message.makeReply(self, request)
	self.id = request.id
	self.version = request.version
end

function CWMP.Message.getShortDescription(self)
	if CWMP.types[self.type].getDescription then
		return CWMP.types[self.type].getDescription(self.message)
	end
	return self.type
end

return CWMP.Message

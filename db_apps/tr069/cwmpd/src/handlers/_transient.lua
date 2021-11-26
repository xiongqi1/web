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
require('Logger')
require('CWMP.Error')

return {
	['**'] = {
		init = function(node, name)
			if node.type == 'collection' then
				node.access = 'readwrite' -- allow instantiation of transient objects as instances
			end
			Logger.log('Parameter', 'debug', 'transient.init(' .. name .. ')')
			return 0
		end,
		get = function(node, name)
			Logger.log('Parameter', 'debug', 'transient.get(' .. name .. ') = "' ..  node.value .. '"')
			return 0, node.value
		end,
		set = function(node, name, value) 
			Logger.log('Parameter', 'info', 'transient.set(' .. name .. ', "' ..  value .. '") was "' .. node.value .. '"')
			node.value = value
			return 0
		end,
		create = function(node, name)
			if node.type == 'collection' then
				local instanceId = node:maxInstanceChildId() + 1
				Logger.log('Parameter', 'info', 'transient.create(' .. name .. ') new instance ID: ' .. instanceId)
				-- create new instance object
				local instance = node:createDefaultChild(instanceId)
				instance:recursiveInit()
				return 0, instanceId
			end
			return CWMP.Error.InvalidArguments, 'Not a multi-object collection.'
		end,
		delete = function(node, name)
			if node.type == 'object' then
				if node.access == 'readonly' then
					return CWMP.Error.InvalidArguments, 'Object not deleteable.'
				end
				Logger.log('Parameter', 'info', 'transient.delete(' .. name .. ')')
				node.parent:deleteChild(node)
				return 0
			end
			return CWMP.Error.InvalidArguments, 'Not an object instance.'
		end,
	}
}

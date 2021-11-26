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
require('luardb')
require('rdbobject')

----
-- RDB Object <--> TR-069 Collection Binding
----

-- rdbmem manages the instance parameters
-- rdbobj just maintains the collection and instance object
-- although it has some role in managing the destruction of instances (their rdbevent listeners)

return {
	['**'] = {
		init = function(node, name)
			if node.type == 'collection' then
				if node.rdbIdSelection == '' then node.rdbIdSelection = nil end
				-- rdbobject class setup
				node.classConfig = {
					persist = (node.rdbPersist == '1'),
					idSelection = node.rdbIdSelection or 'manual',
				}
				node.rdbClass = rdbobject.getClass(node.rdbKey, node.classConfig)
				
				-- instantiability
				if node.rdbIdSelection then
					node:setAccess('readwrite') -- can be instantiated
				else
					node:setAccess('readonly') -- not instantiable
				end

				-- fetch existing instances in RDB and create nodes for each
				local ids = node.rdbClass:getIds()
				for _, id in ipairs(ids) do
					Logger.log('Parameter', 'info', 'rdbobj.init(' .. name .. '): creating existing instance: ' .. id)
					local instance = node:createDefaultChild(id)
					-- new instance subtree is initialised by post-config parse tree init
				end
				
				-- external create/delete handling
				node.rdbClass:onCreate(function(class, instanceId, eventType)
					if not node:getChild(instanceId) then
						Logger.log('Parameter', 'info', 'rdbobj.onCreate(' .. instanceId .. ') new asynchronous instance')
						local treeInstance = node:createDefaultChild(instanceId)
						treeInstance:recursiveInit()
						-- ensure current values are notified if changes would be
						treeInstance:forAll(function(n)
							if n.notify > 0 and n.value then
								client:asyncParameterChange(n, n:getPath(), n.value)
							end
						end, true)
					end
				end)
				node.rdbClass:onDelete(function(class, instanceId, eventType)
					local instance = node:getChild(instanceId)
					if instance then
						Logger.log('Parameter', 'info', 'rdbobj.onDelete(' .. instanceId .. ') asynchronous instance deletion')
						-- delete parameter tree instance
						-- first we remove their change listeners
						instance:forAll(function(n)
							if n.rdbListenerId then
								rdbevent.removeListener(n.rdbListenerId)
							end
						end, true)
						-- then we can remove the sub-tree
						instance.parent:deleteChild(instance)
					end
				end)
			elseif node.type == 'object' then
				-- instance deletability
				if node.parent.deletable == '1' then
					node:setAccess('readwrite') -- can be deleted
				else
					node:setAccess('readonly') -- can not be deleted
				end
			else
				error('Handler can not init() node of type "' .. node.type .. '".')
			end

			return 0
		end,
		create = function(node, name)
			assert(node.type == 'collection', 'You may not create() a non-collection.') -- never happens - we hope!

			-- create rdbobject instance
			local instance = node.rdbClass:new() -- instance ID selection comes from class config
			local instanceId = node.rdbClass:getId(instance)
			Logger.log('Parameter', 'debug', 'rdbobj.create(' .. name .. ', "' .. instanceId .. '")')

			-- create parameter tree instance
		--	local treeInstance = node:createDefaultChild(instanceId)
		--	treeInstance:recursiveInit()
			-- onCreate callback does the actual data model insertion

			return 0, instanceId
		end,
		delete = function(node, name)
			assert(node.type == 'object', 'You may not delete() a non-object.'); -- never happens - we hope!

			Logger.log('Parameter', 'debug', 'rdbobj.delete(' .. name .. ')')

			-- delete the RDB instance
			local instance = node.parent.rdbClass:getById(node.name)
			node.parent.rdbClass:delete(instance)
			
			-- delete parameter tree instance
			-- first we remove their change listeners
			node:forAll(function(n)
				if n.rdbListenerId then
					rdbevent.removeListener(n.rdbListenerId)
				end
			end, true)
			-- then we can remove the sub-tree
			node.parent:deleteChild(node)
			
			return 0
		end
	}
}

----
-- Layer3Forwarding is the routing table
----

return {
	['**.ForwardNumberOfEntries'] = {
		get = function(node, name)
			local forwarding = findNode(paramRoot, 'InternetGatewayDevice.Layer3Forwarding.Forwarding')
			return forwarding:countInstanceChildren()
		end,
		set = cwmpError.funcs.ReadOnly
	},
	['**.Forwarding'] = {
		init = function(node, name)
			dimclient.log('info', 'init: ' .. node:getPath() .. ': ' .. name)
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('info', 'createInstance: ' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			print(node)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('info', 'deleteInstance: ' .. node:getPath() .. ': ' .. name .. ', ' .. instanceId)
			node.parent:deleteChild(node)
		end,
	},
}

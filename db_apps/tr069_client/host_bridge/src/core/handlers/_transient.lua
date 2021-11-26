return {
	['**'] = {
		init = function(node, name, value)
			dimclient.log('info', 'transient.init(' .. name .. ', "' .. value .. '")')
			return 0
		end,
		get = function(node, name)
			dimclient.log('info', 'transient.get(' .. name .. ') = "' ..  node.value .. '"')
			return node.value
		end,
		set = function(node, name, value) 
			dimclient.log('info', 'transient.set(' .. name .. ', "' ..  value .. '") was "' .. node.value .. '"')
			node.value = value
			return 0
		end,
		unset = function(node, name) 
			dimclient.log('info', 'transient.unset(' .. name .. ') defaulting to "' .. node.default .. '"')
			node.value = node.default
			return 0
		end,
		create = function(node, name, instanceId)
			dimclient.log('info', 'transient.create(' .. name .. ', ' .. instanceId .. ')')
			-- create new instance object
			local instance = node:createDefaultChild(instanceId)
			print(node)
			return 0
		end,
		delete = function(node, name)
			dimclient.log('info', 'transient.delete(' .. name .. ')')
			return 0
		end,
	}
}

return {
	['**'] = {
		init = function(node, name, value)
			dimclient.log('info', 'const.init(' .. name .. ') = "' ..  node.value .. '"')
			return 0
		end,
		get = function(node, name)
			dimclient.log('info', 'const.get(' .. name .. ') = "' ..  node.value .. '"')
			return node.value
		end,
		set = function(node, name, value) 
			dimclient.log('error', 'Assigning a constant parameter "' .. name .. '"? "' ..  node.value .. '" -> "' .. value .. '"')
			node.value = value	-- should we actually do this? ideally we should never get here
			return 0
		end,
		unset = function(node, name) 
			dimclient.log('error', 'Deleting a constant parameter "' .. name .. '"?')
			node.value = node.default	-- should we actually do this? ideally we should never get here
			return 0
		end,
		create = cwmpError.funcs.ResourceExceeded,
		delete = cwmpError.funcs.InvalidArgument
	}
}

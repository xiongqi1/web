return {
	['**'] = {
		get = function(node, name)
			dimclient.log('error', 'Fetching internal parameter "' .. name .. '"?')
			return node.value 
		end,
		set = function(node, name, value) 
			dimclient.log('error', 'Assigning internal parameter "' .. name .. '"? "' .. node.value .. '" -> "' .. value .. '"')
			node.value = value
			return 0
		end,
		unset = function(node, name) 
			dimclient.log('error', 'Deleting internal parameter "' .. name .. '"?')
			node.value = node.default
			return 0
		end,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	}
}

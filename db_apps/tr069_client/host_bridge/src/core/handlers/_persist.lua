return {
	['**'] = {
		init = function(node, name, value)
			node.value = luardb.get(node.rdbKey)
			if node.value == nil then
				dimclient.log('info', 'persist.init(' .. name .. '): defaulting rdb variable ' .. node.rdbKey .. ' = "' .. node.default .. '"')
				luardb.set(node.rdbKey, node.default)
				node.value = node.default
			else
				dimclient.log('debug', 'persist.init(' .. name .. '): rdb value "' .. node.value .. '"')
			end
			luardb.setFlags(node.rdbKey, luardb.getFlags(node.rdbKey) .. 'p') -- set persist flag
			
			-- creating a function per var is perhaps a bit expensive?
			-- one might implement this using a table look-up or walk of config to map RDB key to TR-069 parameter path
			local watcher = function(key, value)
				if value then
					dimclient.log('info', 'persist rdb watcher: change notification for ' .. key)
					if value ~= node.value then
						dimclient.log('info', 'persist watcher: ' .. key .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
						dimclient.setParameter(node:getPath(), value)
					end
				end
			end
			dimclient.log('info', 'persist.init(' .. name .. '): installed watcher for rdb variable '.. node.rdbKey)
			luardb.watch(node.rdbKey, watcher)
			return 0
		end,
		get = function(node, name)
			node.value = luardb.get(node.rdbKey)
			dimclient.log('info', 'persist.get(' .. name .. ') = luardb.get("' ..  node.rdbKey .. '") = "' .. node.value .. '"')
			return node.value
		end,
		set = function(node, name, value)
			dimclient.log('info', 'persist.set(' .. name .. ', "' .. value .. '") = luardb.set("' ..  node.rdbKey .. '", ...)')
			node.value = value
			luardb.set(node.rdbKey, node.value)
--			luardb.setFlags(node.rdbKey, luardb.getFlags(node.rdbKey) .. 'p') -- set persist flag
			return 0
		end,
		unset = function(node, name) 
			dimclient.log('info', 'persist.unset(' .. name .. ', "' .. value .. '") = luardb.unset("' ..  node.rdbKey .. '", ...)')
			node.value = node.default
			luardb.unset(node.rdbKey)
			return 0
		end,
		create = cwmpError.funcs.InvalidArgument,
		delete = cwmpError.funcs.InvalidArgument
	}
}

local function getKey(node, name)
	local pathBits = name:explode('.')
	return node.rdbKey .. '.' .. pathBits[#pathBits - 1] .. '.' .. node.rdbField
end

return {
	['**'] = {
		init = function(node, name, value)
			if node.type == 'collection' then
				-- collection init
				node:setAccess('readwrite') -- can be instantiated
				-- fetch instances in RDB and create nodes for each
				local class = rdbobject.getClass(node.rdbKey)
				local ids = class:getIds()
				for _, id in ipairs(ids) do
					dimclient.log('info', 'rdbobj.init(' .. name .. '): creating existing instance: ' .. id)
					local instance = node:createDefaultChild(id)
					instance:setAccess('readwrite') -- can be deleted
				end
				return 0
			elseif node.type == 'object' then
				-- instance init
				node:setAccess('readwrite') -- can be deleted
				return 0
			elseif node:isParameter() then
				-- parameter init
				local key = getKey(node, name)
				node.value = luardb.get(key)
				if node.value == nil then
					dimclient.log('info', 'rdbobj.init(' .. name .. '): defaulting rdb variable ' .. key .. ' = "' .. node.default .. '"')
					node.value = node.default
				else
					dimclient.log('debug', 'rdbobj.init(' .. name .. '): rdb value "' .. node.value .. '"')
				end
				luardb.set(key, node.value, 'p')

				-- creating a function per var is perhaps a bit expensive?
				-- one might implement this using a table look-up or walk of config to map RDB key to TR-069 parameter path
				local watcher = function(k, value)
					if value then
						dimclient.log('info', 'rdbobj rdb watcher: change notification for ' .. k)
						if value ~= node.value then
							dimclient.log('info', 'persist watcher: ' .. k .. ' changed: "' .. node.value .. '" -> "' .. value .. '"')
							dimclient.setParameter(node:getPath(), value)
						end
					end
				end
				dimclient.log('info', 'rdbobj.init(' .. name .. '): installed watcher for rdb variable '.. key)
				luardb.watch(key, watcher)
				return 0
			end
			error('Handler can not init() node of type "' .. node.type .. '".')
		end,
		get = function(node, name)
			if node.type == 'collection' or node.type == 'object' then
				return node.value
			elseif node:isParameter() then
				local key = getKey(node, name)
				node.value = luardb.get(key)
				dimclient.log('info', 'rdbobj.get(' .. name .. ') = luardb.get("' ..  key .. '") = "' .. node.value .. '"')
				return node.value
			else
				error('Handler can not get() node of type "' .. node.type .. '".')
			end
		end,
		set = function(node, name, value)
			if node.type == 'collection' or node.type == 'object' then
				return cwmpError.ReadOnly
			elseif node:isParameter() then
				local key = getKey(node, name)
				dimclient.log('info', 'rdbobj.set(' .. name .. ', "' .. value .. '") = luardb.set("' ..  key .. '", ...)')
				node.value = value
				luardb.set(key, node.value)
				return 0
			else
				error('Handler can not set() node of type "' .. node.type .. '".')
			end
		end,
		unset = function(node, name) 
			if node.type == 'collection' or node.type == 'object' then
				return cwmpError.ReadOnly
			elseif node:isParameter() then
				dimclient.log('info', 'rdbobj.unset(' .. name .. ', "' .. value .. '") = luardb.unset("' ..  node.rdbKey .. '", ...)')
				node.value = node.default
				luardb.unset(node.rdbKey)
				return 0
			else
				error('Handler can not unset() node of type "' .. node.type .. '".')
			end
		end,
		create = function(node, name, instanceId)
			if node.type == 'collection' then
				dimclient.log('info', 'rdbobj.create(' .. name .. ', "' .. instanceId .. '")')
				-- create the RDB instance
				local class = rdbobject.getClass(node.rdbKey)
				local instance = class:new(instanceId)
				-- create parameter tree instance
				local treeInstance = node:createDefaultChild(instanceId)
				treeInstance:recursiveInit()
				node.instance = instanceId
				return 0
			elseif node.type == 'object' or node:isParameter() then
				return cwmpError.InvalidArgument
			else
				error('Handler can not create() node of type "' .. node.type .. '".')
			end
		end,
		delete = function(node, name)
			if node.type == 'object' then
				dimclient.log('info', 'rdbobj.delete(' .. name .. ')')
				-- determine ID
				local pathBits = name:explode('.')
				local id = pathBits[#pathBits - 1]
				-- delete the RDB instance
				local class = rdbobject.getClass(node.rdbKey)
				local instance = class:getById(id)
				class:delete(instance)
				-- delete parameter tree instance
				node.parent:deleteChild(node)
				return 0
			elseif node.type == 'collection' or node:isParameter() then
				return cwmpError.InvalidArgument
			else
				error('Handler can not delete() node of type "' .. node.type .. '".')
			end
		end
	}
}

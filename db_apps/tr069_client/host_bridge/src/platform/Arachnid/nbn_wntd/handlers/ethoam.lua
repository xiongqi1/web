require('rdbrpcclient')
require('processmgr')
----
-- AVC Object Handler
----

local function getRmpRdbKey(node, name)
	local pathBits = name:explode('.')
	local fieldMapping = {
		['mpid'] = 'mpid', -- 0 -8191
		['ccmDefect']	= 'ccmdefect',
		['lastccmRDI']	= 'lastccmrdi',
		['lastccmPortState']		= 'lastccmportstate',
		['lastccmIFStatus']	= 'lastccmifstatus',
		['lastccmSenderID']	= 'lastccmsenderid',
		['lastccmmacAddr']	= 'lastccmmacaddr',
	}
	if not fieldMapping[pathBits[#pathBits]] then
		error('AVC: unknown parameter to map "' .. pathBits[#pathBits] .. '"')
	end
	return conf.wntd.dot1agPrefix.. '.rmp.' .. pathBits[#pathBits - 1] .. '.' .. fieldMapping[pathBits[#pathBits]]
end


local debug = conf.wntd.debug;
local RPC_TIMEOUT =10

debug=2;

------------------------------------------------
local function dinfo(v)
  if debug > 1 then
    dimclient.log('info', v)
  end
end

local function dinfo2(v1, v2)
  if debug > 1 then
    print(v1,v2)
  end
end



local rpc = nil; --rdbrpcclient:new(conf.wntd.dot1agPrefix);
local rdb_rmp= conf.wntd.dot1agPrefix..".rmp";

local RMP_root;
local active_ids={};

local rdbObjectClass = rdbobject.getClass(rdb_rmp)

-- add rmp
local function add_rmp( objid)
	local result = rpc:invoke3('add_rmp', {['objid']=objid }, RPC_TIMEOUT)
	dinfo2( "add_rmp(" .. objid ..") ", result);
	return result;
end


-- add rmp
local function del_rmp( objid)
	local result =  rpc:invoke3( 'del_rmp', {['objid']=objid}, RPC_TIMEOUT)
	dinfo2( "del_rmp(".. objid..") ", result);
	return result;
end
-- set rmp
local function set_rmp( objid, rmpid)
	local result = rpc:invoke3('set_rmp', {['objid']=objid, ['rmpid'] = rmpid}, RPC_TIMEOUT)
	dinfo2( "set_rmp(".. objid..", "..rmpid ..") ", result);
	return result;
end

-- get rmp
local function get_rmp(  rmpid)
	local result = rpc:invoke3( 'get_rmp', { ['rmpid'] = rmpid}, RPC_TIMEOUT)
	dinfo2( "get_rmp(".. rmpid..") ", result);
	return result;
end
-- update rmp
local function update_rmp( )
	local result = rpc:invoke3( 'update_rmp', { }, RPC_TIMEOUT)
	dinfo2( "update_rmp()", result);
	return result;
end
-- list rmp
---[[
local function list_rmp( )
	local result = rpc:invoke3('list_rmp', { }, RPC_TIMEOUT)
	dinfo2( "list_rmp() ", result);
	return result;
end
--]]


local function check_result( result)
	if result then
		val = string.match( result, "^Error");
		if not val then return true end
	end
	return false;
end


local function node_change_notify()
	-- create current instances
	local ids = rdbObjectClass:getIds()
	local changed = 0;
	dinfo( 'RMP: node_change_notify >>>')
	--dump_list();

	-- remove deleted node
	for id, val in pairs(active_ids) do

		if val and not table.contains(ids, id) then
			local obj_path= RMP_root..'.'..id.. '.';
			dinfo( 'Rmp: delObject ' .. obj_path )
			dimclient.delObject(obj_path);
			active_ids[id] =nil;
		end
	end
	--add new node
	for _, id in  pairs(ids) do
		if not active_ids[id] then
		 -- new note
			local obj_path= RMP_root..'.'
			dinfo( 'Rmp: addObject ' .. obj_path..', id= '..id)
			--new_alarm_id = id;
			local ret = dimclient.addObject(obj_path, id);

			if  ret ==0 then

				active_ids[id] = 1;
				dinfo( 'Rmp: addObject OK')
			end
		end

	end
	dinfo( 'RMP: node_change_notify <<<')
end

-- install watcher for external changes
local function rdb_changed(key, value)
	dinfo( 'RMP: _index change of ' .. key)
	dimclient.callbacks.register('preSession', node_change_notify)

end

--local program_path="/usr/bin/"

--local ethoamd_process= processmgr:new('ethoamd_069', program_path..'ethoamd', '');

return {
	----
	-- Container Collection
	----
--[[
	['**.Dot1ag.Installed'] = {
		init = function(node, name, value)
			dinfo('Dot1ag.Installed: model init')
			-- kill all exist program
			processmgr.killpid('ethoamd_069.*');
			return 0
		end,
		get = function(node, name)
			if node.type ~= 'default' then
				if ethoamd_process:running() then
					node.value = '1'
				else
					node.value = '0'
				end
			end

			return node.value;
		end,
		set = function(node, name, value)
			dinfo( 'Dot1ag.Installed: set '..name)
			if node.type ~= 'default' then
				-- start or stop program
				if value =="1" then
					ethoamd_process:start();
					os.execute('sleep 1');
					local r, rpc1 = pcall( rdbrpcclient.new , rdbrpcclient, conf.wntd.dot1agPrefix);
					if  r then rpc =rpc1; end
				else
					ethoamd_process:stop();
					rpc =nil;
				end
			end
			return 0
		end,

		unset = cwmpError.funcs.ReadOnly,
		create = cwmpError.funcs.ReadOnly,
		delete = cwmpError.funcs.InvalidArgument
	},

--]]

	['**.Dot1ag.Rmp'] = {
		init = function(node, name, value)
			dinfo('Rmp: model init')
			node:setAccess('readwrite') -- allow creation of instances
			-- create instances
			local r, rpc1 = pcall( rdbrpcclient.new , rdbrpcclient, conf.wntd.dot1agPrefix);
			if  r then rpc =rpc1; end

			if rpc then
				local list = list_rmp();
				if check_result(list) then
					for objid, rmpid in string.gmatch(list, "%((%d%d*),%s*(%d%d*)%)") do
						dinfo('RMP: creating existing instance ' .. objid)
						local instance = node:createDefaultChild(objid)
						instance:setAccess('readwrite') -- instance can be deleted
						active_ids[objid] =1;
					end
				end
			end

			RMP_root = node:getPath();
			luardb.watch(rdb_rmp.."._index", rdb_changed)

			return 0
		end,
		get = function(node, name) return '' end,
		set = cwmpError.funcs.ReadOnly,
		unset = cwmpError.funcs.ReadOnly,
		create = function(node, name, instanceId)
			dinfo( 'RMP: create instance ' .. instanceId)
			if rpc then
				-- create the RDB instance
				active_ids[instanceId] =1;
				result = add_rmp(instanceId);
				--dinfo2(result)
				-- create parameter tree instance
				if check_result(result) then
					local treeInstance = node:createDefaultChild(instanceId)
					treeInstance:recursiveInit()
					node.instance = instanceId;
					return 0
				end
			end
			return  cwmpError.InternalError
		end,
		delete = cwmpError.funcs.InvalidArgument
	},

----
	-- Instance Object
	----
	['**.Dot1ag.Rmp.*'] = {
		init = function(node, name, value)
			dinfo( 'Rmp.* init')
			node.value = node.default
			dinfo( 'Rmp.* : init')
			return 0
		end,
		get = function(node, name) return '' end,
		set = function(node, name)	return 0 end,
		unset = function(node, name)
			dinfo( 'Rmp.* unset  '..name)
			-- remove node in the cleared list
			if rpc then
				local id = string.match(name, "(%d%d*)%.");
				active_ids[id] = nil;
				local result = del_rmp(id);
			end

			-- delete parameter tree instance
			node.parent:deleteChild(node)
			return 0

		end,
		--create = cwmpError.funcs.InvalidArgument,
		create = function(node, name)
			dinfo( 'Rmp.* create '..name)
			return 0;
		end,

		delete = function(node, name)
			dinfo( 'Rmp.*: delete  '..name)
			return 0
		end,
		--alarm object cannot delete by user.
	},
	----
	-- Instance Fields
	----
	['**.Dot1ag.Rmp.*.*'] = {
		init = function(node, name, value)
			dinfo( 'Rmp.*.* init')
			-- skip default node instance
			if node.type == 'default' then
				node.value = node.default
				return 0
			end
			-- check RDB state and default if required
			local key = getRmpRdbKey(node, name)
			node.value = luardb.get(key)
			if node.value == nil then
				dinfo('RMP: defaulting ' .. key .. ' to "' .. node.default .. '".')
				node.value = node.default
			end
			return 0
		end,
		get = function(node, name)
			if node.type ~= 'default' then
				node.value = luardb.get(getRmpRdbKey(node, name))
				dinfo2('Rmp.*.* get' , node.value)
			end

			return node.value
		end,
		set = function(node, name, value)
			dinfo( 'Rmp.*.* set '..name)

			if node.type ~= 'default' then
				local id =string.match(name, "(%d%d*)%.mpid$");
				dinfo( id)
				if id then
					value = tonumber(value);
					if  rpc and value >=0 and value < 8192 then
						local result = set_rmp(id, value);
						if  check_result(result) then
							node.value = value
							return 0;
						end
					end
					return cwmpError.InvalidArgument
				end

			end
			return 0
		end,
		unset = cwmpError.funcs.ReadOnly,
	},

}

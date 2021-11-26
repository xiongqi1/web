-- add daemon module for popen utility functions
require('Daemon')

require('stringutil')
require('Logger')

Logger.addSubsystem('v_ipkg_y')

local subROOT = conf.topRoot .. '.X_NETCOMM.IPKGPackages.'

------------------local function prototype------------------
local string_lines
local get_installed_ipkg_list
local compare_installed_ipkg
local reconstruct_child_nodes
local no_operation_set_function
local get_table_length
------------------------------------------------------------


------------------local variable definition ----------------
local g_depthOfInstalledPackages = 5
local g_installed_ipkg_list = {}
------------------------------------------------------------


------------------local function definition ----------------


string_lines = function(str)
	local pos = 1

	return (function ()
		-- bypass if no str or big pos
		if not str or pos > str:len() then
			return
		end

		local sstr

		-- extract a string
		local eol = str:find('\n', pos)
		sstr = str:sub(pos,eol)

		-- remove carrige return if existing
		if eol then
			sstr = sstr:sub(1,-2)
		end


		-- remove carrige return if existing
		if eol then
			pos = eol + 1
		else
			pos = str:len() + 1
		end

		return sstr
	end)
end

-- create a ipkg table from result of ipkg-cl status
get_installed_ipkg_list = function()
	local installed_ipkg_list = {}

	local ipkg_cl_result = Daemon.readCommandOutput('ipkg-cl status')

	-- Package: ntc-140wx-dummy-fix-1
	-- Version: 1.0
	-- Status: install user installed
	-- Architecture: arm
	-- Installed-Time: 1431668653
	--
	-- Package: ntc-140wx-dummy-fix-2
	-- Version: 1.0
	-- Status: install user installed
	-- Architecture: arm
	-- Installed-Time: 1431671176
	--
	-- Successfully terminated.

	local installed_ipkg = {}

	for l in string_lines(ipkg_cl_result) do

		local n, v = l:match('(%S+): (.+)')

		if n then
			-- remove chars that are not friendly
			n = string.gsub(n,'[-]','')

			-- clear table at the begining of secion - paranoia code
			if n == "Package" then
				installed_ipkg = {}
			end

			installed_ipkg[n] = v

			-- accumulate each item to list
			if n == "InstalledTime" then
				table.insert(installed_ipkg_list,installed_ipkg)
				installed_ipkg = {}
			end
		end
	end

	return installed_ipkg_list
end

get_table_length = function(t)
	local count = 0
	
	for _ in pairs(t) do
		count = count + 1
	end

	return count
end

compare_installed_ipkg = function(source,target)
	local i
	local match_count = 0
	local k,v
	local member_match_count

	for i,o in ipairs(source) do

		-- compare all members
		member_match_count = 0
		for k,v in pairs(o) do
			if v == target[i][k] then
				member_match_count = member_match_count + 1
			end
		end

		-- increase match count if identical
		if member_match_count == get_table_length(target[i]) then
			match_count = match_count + 1
		end

	end

	return match_count == #target
end

reconstruct_child_nodes = function(node)
	-- delete all of children data model object
	for _, child in ipairs(node.children) do
		if child.name ~= '0' then
			child.parent:deleteChild(child)
		end
	end

	-- add children
	for i, v in ipairs(g_installed_ipkg_list) do
		node:createDefaultChild(i)
	end
end

poll_installed_ipkg_object = function (task)
	local new_installed_ipkg_list = get_installed_ipkg_list()
	local isSame = true
	local i

	-- bypass if list is not changed
	if compare_installed_ipkg(g_installed_ipkg_list,new_installed_ipkg_list) then
		return
	end

	-- update children
	g_installed_ipkg_list = new_installed_ipkg_list
	reconstruct_child_nodes(task.data)
end

no_operation_set_function = function()
	return 0
end

------------------------------------------------------------


return {

-- =====[START] IPKGPackages==================================

-- uint:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'NumberOfInstalledPackages'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = subROOT .. 'InstalledPackages.'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = no_operation_set_function
	},

-- object:readonly
-- Default Value:
-- Available Value:
-- Involved RDB variable:
	[subROOT .. 'InstalledPackages'] = {
		init = function(node, name, value)

			-- add children
			g_installed_ipkg_list = get_installed_ipkg_list();
			reconstruct_child_nodes(node)

			-- add persistent callback
			if client:isTaskQueued('preSession', poll_installed_ipkg_object) ~= true then
				client:addTask('preSession', poll_installed_ipkg_object, true, node)
			end

			return 0
		end,
	},

	[subROOT .. 'InstalledPackages.*'] = {
		init = function(node, name, value)
			local pathBits = name:explode('.')
			g_depthOfInstalledPackages = #pathBits
			return 0
		end,
	},

	[subROOT .. 'InstalledPackages.*.*'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local pathBits = name:explode('.')
			local dataModelIdx = pathBits[g_depthOfInstalledPackages]
			local paramName = pathBits[g_depthOfInstalledPackages+1]

			return 0, g_installed_ipkg_list[tonumber(dataModelIdx)][paramName]
		end,
		set = no_operation_set_function
	},


-- =====[END] IPKGPackages==================================
}

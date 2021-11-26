require("handlers.hdlerUtil")

-------------------local variable-----------------------------
--------------------------------------------------------------


-------------------local function prototype-------------------
--------------------------------------------------------------


------------------local function definition-------------------
--------------------------------------------------------------


return {
	[conf.topRoot..'.LANDeviceNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.LANDevice'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.WANDeviceNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.WANDevice'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.Cellular.InterfaceNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.Cellular.Interface'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.Cellular.AccessPointNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.Cellular.AccessPoint'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.Ethernet.InterfaceNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.Ethernet.Interface'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.Ethernet.LinkNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.Ethernet.Link'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},

	[conf.topRoot..'.PeriodicStatistics.SampleSetNumberOfEntries'] = {
		init = function(node, name, value) return 0 end,
		get = function(node, name)
			local targetName = conf.topRoot..'.PeriodicStatistics.SampleSet'

			return 0, hdlerUtil.getNumOfSubInstance(targetName)
		end,
		set = CWMP.Error.funcs.ReadOnly
	},
}

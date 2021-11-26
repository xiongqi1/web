require('CWMP.Error')

local subROOT = conf.topRoot .. '.X_NETCOMM_WEBUI.Services.NetworkQuality.'
------------------local function prototype------------------
local speedtest_cb
------------------------------------------------------------

speedtest_cb = function ()
	-- Do not need to do anything, if speed_test.sh is in being processed.
	-- Because speedtest template is already triggered via other service.
	if os.execute('pidof speed_test.sh 1>/dev/null 2>&1') ~= 0 then
		luardb.set('service.speedtest.result', '')
		luardb.set('service.speedtest.trigger', '1')
	end
end

return {
-- Attribute: bool:writeonly
-- Available Value: 1
-- rdb variable used: 'service.speedtest.trigger'
	[subROOT .. 'SpeedTestTrigger'] = {
		get = function(node, name)
			return 0, "1"
		end,
		set = function(node, name, value)
			value = string.trim(value)
			if value ~= "1" then return CWMP.Error.InvalidParameterValue end

			if client:isTaskQueued('postSession', speedtest_cb) ~= true then
				client:addTask('postSession', speedtest_cb, false)
			end

			return 0
		end
	},
}

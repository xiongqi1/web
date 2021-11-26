----
-- NTP Bindings
--
-- Vaugely Time:2 compilant, but largely read-only.
-- NTPServerX and Enable are mapped directly to RDB by config file persist() handlers.
-- We also implement InternetGatewayDevice.DeviceInfo.UpTime here too.
----

require('Daemon')

local function uptimePoller(task)
	local node = task.data
	local uptime = tostring(Daemon.readIntFromFile('/proc/uptime')) or '0'
	client:asyncParameterChange(node, node:getPath(), uptime)
end

return {
	['**.DeviceInfo.UpTime'] = {
		get = function(node, name)
			return 0, tostring(Daemon.readIntFromFile('/proc/uptime')) or '0'
		end,
		attrib = function(node, name)
			if node.notify > 0 and not client:isTaskQueued('preSession', uptimePoller) then
				client:addTask('preSession', uptimePoller, true, node)
			elseif node.notify < 1 and client:isTaskQueued('preSession', uptimePoller) then
				client:removeTask('preSession', uptimePoller)
			end
			return 0
		end,
	},
	['**.Time.CurrentLocalTime'] = {
		get = function(node, name) return 0, os.date('%s')	end
	},
	['**.Time.LocalTimeZone'] = {
		get = function(node, name) return 0, '+00:00' end
	},
	['**.Time.LocalTimeZoneName'] = {
		get = function(node, name) return 0, 'UTC' end
	},
}

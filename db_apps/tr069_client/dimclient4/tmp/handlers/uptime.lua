local function getValue(node, name)
	if name == 'InternetGatewayDevice.DeviceInfo.UpTime' then
		local file = io.open('/proc/uptime', 'r')
		local uptime = math.floor(file:read('*n'))
		file:close()
		return uptime
	else
		error('Can not handle getValue() for "' .. name .. '".')
	end
end

local function setValue(node, name, value)
	if name == 'InternetGatewayDevice.DeviceInfo.UpTime' then
		return 9008; -- read only parameter
	else
		error('Can not handle setValue() for "' .. name .. '".')
	end
	return 0;
end

return {
	['getValue'] = getValue,
	['setValue'] = setValue
}

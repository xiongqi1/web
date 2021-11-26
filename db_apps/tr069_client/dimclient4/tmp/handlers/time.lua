local function getValue(node, name)
	if name == 'InternetGatewayDevice.DeviceInfo.UpTime' then
		local file = io.open('/proc/uptime', 'r')
		local uptime = math.floor(file:read('*n'))
		file:close()
		return uptime
	elseif name == 'InternetGatewayDevice.Time.CurrentLocalTime' then
		return os.date('%s')
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZone' then
		return '+00:00'
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZoneName' then
		return 'UTC'
	else
		error('Can not handle getValue() for "' .. name .. '".')
	end
end

local function setValue(node, name, value)
	if name == 'InternetGatewayDevice.DeviceInfo.UpTime' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.CurrentLocalTime' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZone' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZoneName' then
		return cwmpError.ReadOnly
	else
		error('Can not handle setValue() for "' .. name .. '".')
	end
	return 0;
end

local function unsetValue(node, name)
	if name == 'InternetGatewayDevice.DeviceInfo.UpTime' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.CurrentLocalTime' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZone' then
		return cwmpError.ReadOnly
	elseif name == 'InternetGatewayDevice.Time.LocalTimeZoneName' then
		return cwmpError.ReadOnly
	else
		error('Can not handle unsetValue() for "' .. name .. '".')
	end
end

return {
	['getValue'] = getValue,
	['setValue'] = setValue,
	['unsetValue'] = unsetValue
}

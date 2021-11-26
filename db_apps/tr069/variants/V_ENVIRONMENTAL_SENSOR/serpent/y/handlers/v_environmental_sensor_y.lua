require("luardb")

-------------------local variable-----------------------------
--------------------------------------------------------------

-- Prefix of vendor extended parameter name.
local xVendorPrefix = conf.xVendorPrefix or "X_CASASYSTEMS"

-------------------local function definition-------------------
--------------------------------------------------------------
local function getSensorStatus()
    local sensorStatus = 'Error'
    local configStatus = luardb.get('sensors.environmental.0.enable')
    local readingStatus = luardb.get('sensors.environmental.0.status')
    if configStatus == '1' then
        if readingStatus == 'ok' then
            sensorStatus = 'Enabled'
        end
    else
        sensorStatus = 'Disabled'
    end
    return 0, sensorStatus
end

return {
-- string: readonly
-- Default Value: ""
-- Available Value: "Enabled/Disabled/Error"
    [conf.topRoot .. '.DeviceInfo.TemperatureStatus.TemperatureSensor.1.Status'] = {
        get = function(node, name)
            return getSensorStatus()
        end,
    },

-- string: readonly
-- Default Value: ""
-- Available Value: "Enabled/Disabled/Error"
    [conf.topRoot .. '.' .. xVendorPrefix .. '.EnvironmentalSensor.Humidity.1.Status'] = {
        get = function(node, name)
            return getSensorStatus()
        end,
    },
}

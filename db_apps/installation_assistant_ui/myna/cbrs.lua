--[[
Helper module for CBRS related functions

Copyright (C) 2019 Casa Systems

--]]

local m_luasign = require("luasign")
local m_escape = require("turbo.escape")
local m_luardb = require("luardb")

local p_ioloop
local p_syslog

local p_cpi_key_from_quick_copy
local p_pkcs12_data
local p_pkcs12_passphrase
local p_pkcs12_valid = false
local p_pkcs12_is_ec_key
local p_pem_is_ec_key

local p_registration_state_monitoring_timer

--[[
Check whether CPI key from Quick Copy is provided
@return true/false
--]]
local function is_cpi_key_from_quick_copy()
    return p_cpi_key_from_quick_copy and true or false
end

--[[
Set CPI key from Quick Copy
@param key_content Key content in PEM format
--]]
local function set_cpi_key_from_quick_copy(key_content)
    p_cpi_key_from_quick_copy = key_content
end

--[[
Check whether existing PKCS12 data is valid
--]]
local function is_pkcs12_valid()
    return p_pkcs12_valid
end

--[[
Checking whether given PEM key content is valid
@param pem_key PEM key content to check
@return 1) true (valid) or false (invalid); 2) Error string of invalid case
--]]
local function validate_pem_key(pem_key)
    local ret, error_str, openssl_error, is_ec_key = m_luasign.validate_pem_key(pem_key)
    if not ret then
        error_str = error_str.." "..openssl_error
    end
    p_pem_is_ec_key = is_ec_key
    return ret, error_str
end

--[[
Check whether CPI key is provided
@return true/false
--]]
local function cpi_key_available()
    return is_cpi_key_from_quick_copy() or is_pkcs12_valid()
end

--[[
Clear existing PKCS12
--]]
local function clear_pkcs12()
    p_pkcs12_valid = false
    p_pkcs12_data = nil
    p_pkcs12_passphrase = nil
end

--[[
Set and validate PKCS12 data
@param arg_pkcs12_data PKCS12 data
@param arg_pkcs12_passphrase Passphrase
@return 1) true (PKCS12 is valid) or false (PKCS12 is invalid); 2) Error string of invalid case
--]]
local function set_pkcs12(arg_pkcs12_data, arg_pkcs12_passphrase)
    -- new, regardless of validity, data always override existing one
    p_pkcs12_data = arg_pkcs12_data
    p_pkcs12_passphrase = arg_pkcs12_passphrase
    local ret, error_str, openssl_error, is_ec_key = m_luasign.validate_pkcs12(p_pkcs12_data, p_pkcs12_passphrase)
    if not ret then
        clear_pkcs12()
        if openssl_error then
            if string.find(openssl_error, "mac verify failure") then
                error_str = "The passcode is incorrect"
            else
                error_str = error_str.." "..openssl_error
            end
        end
    else
        p_pkcs12_valid = true
        p_pkcs12_is_ec_key = is_ec_key
    end
    return ret, error_str
end

--[[
Generate CpiSignatureData for CBRS Registration request
This function follows the procedure implemented in the script sas/sasCpiSignature.sh.
@param params parameters table
@return JSON-encoded CpiSignatureData
--]]
local function generate_cpi_signature_data(params)
    local payload = {
        fccId = m_luardb.get("sas.config.fccid") or "",
        cbsdSerialNumber = m_luardb.get("sas.config.cbsdSerialNumber") or "",
        installationParam = {
            latitude = tostring(params.latitude),
            longitude = tostring(params.longitude),
            height = tostring(params.height),
            heightType = tostring(params.height_type),
            eirpCapability = m_luardb.get("sas.antenna.eirp_cap") or "",
            antennaAzimuth = tostring(params.azimuth),
            antennaDowntilt = tostring(params.downtilt),
            antennaGain = m_luardb.get("sas.antenna.gain") or "",
            antennaBeamwidth = m_luardb.get("sas.antenna.beam_width") or "",
            antennaModel = m_luardb.get("sas.antenna.model") or "",
            indoorDeployment = m_luardb.get("sas.config.indoorDeployment") or ""
        },
        professionalInstallerData = {
            cpiId = params.cpi_id,
            cpiName = params.cpi_name,
            installCertificationTime = params.install_certification_time
        }
    }

    -- check if the signing key is of EC type
    local is_ec_key = function() return (is_pkcs12_valid() and p_pkcs12_is_ec_key) or p_pem_is_ec_key end

    local header = {typ = "JWT", alg = is_ec_key() and "ES256" or "RS256"}

    payload = m_escape.json_encode(payload)
    header = m_escape.json_encode(header)
    payload = m_escape.base64_encode(payload, nil, true)
    header = m_escape.base64_encode(header, nil, true)

    p_syslog.log("LOG_INFO", "CpiSignatureData - protectedHeader: "..header)
    p_syslog.log("LOG_INFO", "CpiSignatureData - encodedCpiSignedData: "..payload)

    local header_payload = header.."."..payload
    local sign_ret
    local sign_res_1
    local sign_res_2
    if is_pkcs12_valid() then
        sign_ret, sign_res_1, sign_res_2 = m_luasign.sign_with_key_from_pkcs12(p_pkcs12_data,
            p_pkcs12_passphrase, header_payload)
    elseif is_cpi_key_from_quick_copy() then
        sign_ret, sign_res_1, sign_res_2 = m_luasign.sign_with_pem_key(p_cpi_key_from_quick_copy,
            header_payload)
    else
        sign_ret = false
    end
    if not sign_ret then
        p_syslog.log("LOG_ERR", "CpiSignatureData - Error in signing: "..sign_res_1..(sign_res_2 and sign_res_2 or ""))
        return nil
    end

    p_syslog.log("LOG_INFO", "CpiSignatureData - digitalSignature: "..sign_res_2)
    local cpi_signature_data = {
        protectedHeader = header,
        encodedCpiSignedData = payload,
        digitalSignature = sign_res_2:gsub("=",""):gsub("+","-"):gsub("/","_") -- to BASE64URL format
    }
    return m_escape.json_encode(cpi_signature_data)
end

--[[
Timer function to monitor registration state to clear PKCS12 when Registration is successful
--]]
local function monitor_registration_state()
    local state = m_luardb.get("sas.registration.state")
    if state == "Registered" then
        if is_pkcs12_valid() then
            p_syslog.log("LOG_INFO", "CBRS Registered, clearing PKCS12")
            clear_pkcs12()
        end
        p_registration_state_monitoring_timer = nil
    elseif state == "Unregistered" then
        p_registration_state_monitoring_timer = nil
    else
        p_registration_state_monitoring_timer = p_ioloop:add_timeout(1000, monitor_registration_state)
    end
end

--[[
Generate CpiSignatureData and invoke registration
@param params Registration parameters table. If use_pkcs12 is set, key from PKCS12 must be used
@return true if success, false otherwise
--]]
local function invoke_registration(params, multi_step)
    if not multi_step and not cpi_key_available() then
        return nil
    end
    if params.use_pkcs12 and not is_pkcs12_valid() then
        return nil
    end
    if not (params.latitude and params.longitude and params.height and params.azimuth and params.downtilt
            and (multi_step or params.cpi_id) and (multi_step or params.cpi_name)
            and (multi_step or params.install_certification_time)and params.height_type) then
        return nil
    end

    local cpi_signature_data = nil
    if not multi_step then
        cpi_signature_data = generate_cpi_signature_data(params)
    end

    if cpi_signature_data or multi_step then
        m_luardb.set("sas.antenna.latitude", params.latitude)
        m_luardb.set("sas.antenna.longitude", params.longitude)
        m_luardb.set("sas.antenna.height", params.height)
        m_luardb.set("sas.antenna.height_type", params.height_type)
        m_luardb.set("sas.antenna.azimuth", params.azimuth)
        m_luardb.set("sas.antenna.downtilt", params.downtilt)
        if not multi_step then
            m_luardb.set("sas.config.cpiId", params.cpi_id)
            m_luardb.set("sas.config.cpiName", params.cpi_name)
            m_luardb.set("sas.config.installCertificationTime", params.install_certification_time);
        end

        -- clear current state and invoke registration
        m_luardb.set("sas.registration.state", "")
        m_luardb.set("sas.registration.response_code", "")
        m_luardb.set("sas.registration.request_error_code", "")
        m_luardb.set("sas.config.cpiSignatureData", cpi_signature_data or "")
        m_luardb.set("sas.registration.cmd", "register")

        if p_registration_state_monitoring_timer then
            p_ioloop:remove_timeout(p_registration_state_monitoring_timer)
        end
        p_registration_state_monitoring_timer = p_ioloop:add_timeout(1000, monitor_registration_state)
        return true
    else
        return false
    end
end

--[[
Initialise the module
User code must call this function before using other APIs of this module.
@param arg_ioloop Turbo IOLoop instance
@param arg_syslog Syslog instance
--]]
local function init(arg_ioloop, arg_syslog)
    p_ioloop = arg_ioloop
    p_syslog = arg_syslog
end

return {
    init = init,
    is_cpi_key_from_quick_copy = is_cpi_key_from_quick_copy,
    set_cpi_key_from_quick_copy = set_cpi_key_from_quick_copy,
    validate_pem_key = validate_pem_key,
    cpi_key_available = cpi_key_available,
    set_pkcs12 = set_pkcs12,
    is_pkcs12_valid = is_pkcs12_valid,
    invoke_registration = invoke_registration
}

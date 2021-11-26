--Copyright (C) 2021 Casa Systems.
--This upload handler stores device certificate and key into designated
--locations that are specified in RDB variables.

require('luardb')
local snextra=require("snextra")
local crt_tmp = "/tmp/lark_tmp.crt"
local key_tmp = "/tmp/lark_tmp.key"

local function storeCerts(from)
    local crt_file = luardb.get("service.authenticate.clientcrtfile")
    local key_file = luardb.get("service.authenticate.clientkeyfile")
    local pass = snextra.generate("da")

    local export_crt = "openssl pkcs12 -in " .. from .. " -nokeys " ..
        "-passin pass:" .. pass .. " -out " .. crt_tmp
    local export_key = "openssl pkcs12 -in " .. from .. " -nocerts " ..
        "-passin pass:" .. pass .. " -passout pass:" .. pass .. " -aes256 -out " .. key_tmp
    local write_keys = "uboot_write_keys.sh -c " .. crt_tmp .. " -k " .. key_tmp

    if 0 ~= os.execute(export_crt) or 0 ~= os.execute(export_key) then
        processed_message = "cannotExportCerts"
        return false
    end

    if 0 ~= os.execute(write_keys) then
        processed_message = "cannotWriteCerts"
        return false
    end

    if 0 ~= os.execute("cp " .. crt_tmp .. " " .. crt_file) then
        processed_message = "cannotCopyCert"
        return false
    end

    if 0 ~= os.execute("cp " .. key_tmp .. " " .. key_file) then
        processed_message = "cannotCopyKey"
        return false
    end

    return true
end

if "certificate" == uploaded_target then
    if storeCerts(uploaded_file) then
        processed_message = "deviceCertsUpdated"
    end
    os.remove(uploaded_file)
    os.remove(crt_tmp)
    os.remove(key_tmp)
end



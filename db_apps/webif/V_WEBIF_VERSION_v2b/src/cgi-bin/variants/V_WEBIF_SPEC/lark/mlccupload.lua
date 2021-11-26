--Copyright (C) 2019 NetComm Wireless Limited.
--This upload handler stores client certificate and key into designated
--locations that are specified in RDB variables.

require('luardb')

local function storeFile(from, to, crt)
    if from and to and 0 == os.execute("mv " .. from .. " " .. to) then
        processed_message = crt .. "Uploaded"
        return 0
    else
        processed_message = crt .. "CantBeUploaded"
        return 1
    end
end

local crt_file = luardb.get("service.authenticate.clientcrtfile")
local key_file = luardb.get("service.authenticate.clientkeyfile")

local save_to, crt_name

if "certificate" == uploaded_target then
    save_to = crt_file
    crt_name = "certificate"
end

if "privateKey" == uploaded_target then
    save_to = key_file
    local key_exists = uploaded_file and 0 == os.execute("test -f " .. uploaded_file)
    local encrypted = key_exists and 0 ~= os.execute("openssl rsa -noout -passin pass: -in "
        .. uploaded_file .. " &> /dev/null")

    if encrypted then
        crt_name = "encryptedPrivateKey"
    else
        crt_name = "privateKey"
    end
end

storeFile(uploaded_file, save_to, crt_name)

-- Copyright (C) 2021 Casa Systems.
-- managing certificate, key

local m_luardb = require("luardb")
local m_snextra = require("snextra")

return {
    ca_path = m_luardb.get("service.authenticate.cafile"),
    cert_file = m_luardb.get("service.authenticate.clientcrtfile"),
    key_file = m_luardb.get("service.authenticate.clientkeyfile"),
    key_passphrase = m_snextra.generate("da")
}

-- Copyright (C) 2018 NetComm Wireless limited.
--
-- ssh key handler
-- Applications call ssh command like this:
-- ssh -i /tmp/mdm_ssh_key.priv -o StrictHostKeyChecking=no username@hostname.com
-- for example: ssh -i /tmp/mdm_ssh_key.priv -o StrictHostKeyChecking=no root@169.254.251.2

local turbo = require("turbo")
local config = require("rds.config")
local basepath = config.basepath
local luardb = require('luardb')

local ipcSshKeyRdbName = "ipc.ssh.privkey"

local  SshPrivKeyHandler = class("SshPrivKeyHandler", turbo.web.RequestHandler)

-- map to rdc:add, save the private key into the file
function SshPrivKeyHandler:post(path)
    -- get the private key from the client request
    jsonTable = self:get_json()
    privKey = jsonTable["privkey"]
    local keyFileName = luardb.get(ipcSshKeyRdbName) or "/tmp/mdm_ssh_key.priv"
    -- save it to the file
    local file = io.open(keyFileName, "w")
    if file then
        file:write(privKey)
        file:close()
        -- change the file access priviledge to 0600
        os.execute("chmod 0600 "..keyFileName)
        self:set_status(201)
        return
    end
    self:set_status(500)
end

-- Return array of path/handler mappings
return { {basepath .. "/ssh/privkey$", SshPrivKeyHandler} }


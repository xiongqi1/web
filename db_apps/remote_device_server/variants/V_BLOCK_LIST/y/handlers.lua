-- Copyright (C) 2018 NetComm Wireless limited.
--
-- Web request handlers for call blocklist.

local turbo = require("turbo")
local util = require("rds.util")
local config = require("rds.config")
local basepath = config.basepath

--------- EXAMPLE block list handler ----------
local BlockListAllHandler = class("BlockListAllHandler", turbo.web.RequestHandler)
function BlockListAllHandler:post(path)
    jsonTable = self:get_json()
    print(util.toPrettyString(jsonTable))
    self:set_status(201) -- new item created
end

-- Return array of path/handler mappings
return {{basepath .. "/call/blocklist$", BlockListAllHandler}}

-- Copyright (C) 2019 NetComm Wireless limited.
--
-- link profile handler

require('luardb')
local turbo = require("turbo")

local config = require("rds.config")
local basepath = config.basepath
local logger = require("rds.logging")

--------- Link profile handler ----------
local LinkProfileHandler = class("LinkProfileHandler", turbo.web.RequestHandler)

-- path: "/link_profile/{index}/{key}"
--    * index: index number of link.profile
--    * key: suffix key of rdb variable "link.profile.{index}.*".
--           Note: valid key characters are "alphanumeric characters" and underscore(_).
function LinkProfileHandler:get(path)
    local resp = nil
    local index, key = string.match(path, basepath .. "/link_profile/(%d+)/([_%w]+)$")
    if index then
        resp = luardb.get(string.format('link.profile.%d.%s', index, key))
        if resp then
            self:write({index=index, key=key, value=resp})
        end
    end
    self:set_status(resp and 200 or 400)
end

-- path: "/link_profile/index"
--    * index: index number of link.profile
--
--    * example of json request:
--    [{"index":1,"key":"apn","value":"testAPN"},{"index":1,"key":"writeflag","value":"1"}]
--       -> Possible to set multiple rdb variables.
--       -> Order safety.
function LinkProfileHandler:put(path)
    local req = self:get_json()
    if req then
        for _, arg in ipairs(req) do
            if not arg.index or not tonumber(arg.index) or not arg.key or not arg.value then
                self:set_status(400)
                return
            end
            luardb.set(string.format('link.profile.%d.%s', arg.index, arg.key), arg.value)
        end
    end
    self:set_status(200)
end

-- Return array of path/handler mappings
return {
    {basepath .. "/link_profile/?%d*/?[_%w]*$", LinkProfileHandler},
}

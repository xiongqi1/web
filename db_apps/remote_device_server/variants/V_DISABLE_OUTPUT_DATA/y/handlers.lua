-- Copyright (C) 2018 NetComm Wireless limited.
--
-- RDC handler for disabling output chain of data path

require('luardb')
local turbo = require("turbo")

local config = require("rds.config")
local basepath = config.basepath
local logger = require("rds.logging")

local FirewallOutputDataHandler = class("FirewallOutputDataHandler", turbo.web.RequestHandler)
function FirewallOutputDataHandler:put(path)
    local chain = 'OUTPUT_FILTER_DISABLE_DATA'
    local req = self:get_json()
    local proc = io.popen('iptables -S 2>/dev/null', 'r')
    local chain_exists, rule_exists, chain_linked
    if not proc then
        self:set_status(500)
        logger.logErr('Failed to run iptables -S')
        return
    end
    --[[ example output:
-N OUTPUT_FILTER_DISABLE_DATA
-A OUTPUT -j OUTPUT_FILTER_DISABLE_DATA
-A OUTPUT_FILTER_DISABLE_DATA -o rmnet_data0 -j DROP
    --]]
    for line in proc:lines() do
        logger.logDbg(line)
        if line:match('^%-N ' .. chain) then
            chain_exists = true
            logger.logDbg(chain .. ' exists')
        elseif line:match('^%-A ' .. chain .. ' %-o rmnet_data0 %-j DROP') then
            rule_exists = true
            logger.logDbg('DROP rule exists in ' .. chain)
        elseif line:match('^%-A OUTPUT %-j ' .. chain) then
            chain_linked = true
            logger.logDbg(chain .. ' is linked into OUTPUT')
        end
    end
    proc:close()

    -- there is no harm if custom chain is not added to default chain
    if not chain_exists then
        logger.logDbg('create chain ' .. chain)
        if os.execute('iptables -N ' .. chain) ~= 0 then
            self:set_status(500)
            logger.logErr('failed to create ' .. chain)
            return
        end
    end
    if not rule_exists then
        logger.logDbg('create DROP rule in ' .. chain)
        if os.execute('iptables -A ' .. chain .. ' -o rmnet_data0 -j DROP') ~= 0 then
            self:set_status(500)
            logger.logErr('failed to populate ' .. chain)
            return
        end
    end

    if req.disable then
        if not chain_linked then
            logger.logDbg('add ' .. chain .. ' to OUTPUT')
            if os.execute('iptables -A OUTPUT -j ' .. chain) ~= 0 then
                self:set_status(500)
                logger.logErr('failed to add ' .. chain .. ' to OUTPUT')
                return
            end
        end
    else
        if chain_linked then
            logger.logDbg('remove ' .. chain .. ' from OUTPUT')
            if os.execute('iptables -D OUTPUT -j ' .. chain) ~= 0 then
                self:set_status(500)
                logger.logErr('failed to remove ' .. chain .. ' from OUTPUT')
                return
            end
        end
    end
    self:set_status(200)
end

-- Return array of path/handler mappings
return {
    {basepath .. "/firewall/output_data", FirewallOutputDataHandler},
}

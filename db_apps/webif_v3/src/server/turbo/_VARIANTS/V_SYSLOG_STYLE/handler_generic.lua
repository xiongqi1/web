--[[
    log file downloading
    Copyright (C) 2021 Casa Systems Inc.
--]]

local shellhandler = require("shell_handler")

-----------------------------------------------------------------------------------------------------------------------
-- Module configuration
-----------------------------------------------------------------------------------------------------------------------
local module = {}
function module.init(handlers)
    table.insert(handlers, {"^/(log_file)$", shellhandler, "/sbin/logcat.sh -a"})
end

return module

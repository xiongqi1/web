--[[
    Extension for application level GPS support in WMMD.

    Copyright (C) 2019 NetComm Wireless Limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local WmmdGpsDecorator = WmmdDecorator:new()
local QmiGDecorator = WmmdDecorator:new()

QmiGDecorator.start_gps = true

-- decorate QmiG
function QmiGDecorator:doDecorate()
    QmiGDecorator:__changeImplTbl({
        "start_gps"
    })
end

function WmmdGpsDecorator.doDecorate()
  WmmdGpsDecorator.__inputObj__.qmiG =
    QmiGDecorator:decorate(WmmdGpsDecorator.__inputObj__.qmiG)
end

return WmmdGpsDecorator

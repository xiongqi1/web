--[[

Decorators supporting Magpie installation assistant UI

Copyright (C) 2019 NetComm Wireless limited.

--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")

-- decorating wmmd
local Sdx20WmmdDecorator = WmmdDecorator:new()
-- decorating qmi_g
local Sdx20QmiGDecorator = WmmdDecorator:new()

-- SETUP DECORATORS

-- Decorate qmi_g
function Sdx20QmiGDecorator.doDecorate()
  local qmiG = Sdx20QmiGDecorator.__inputObj__

  qmiG.essential_services = {"wds","nas","dms"}
end

-- Decorate wmmd
function Sdx20WmmdDecorator.doDecorate()
  Sdx20QmiGDecorator:decorate(Sdx20WmmdDecorator.__inputObj__.qmiG)
end

return Sdx20WmmdDecorator

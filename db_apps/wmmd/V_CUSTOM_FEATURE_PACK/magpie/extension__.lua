--[[
  Decorator for Magpie

  Copyright (C) 2019 NetComm Wireless limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local MagpieDecorator = WmmdDecorator:new()
local QmiGDecorator = WmmdDecorator:new()

-- construct the neighbour list  according to RDB: wwan.0.cell_measurement.ncell.data
-- format of ncell.data:  earfcn,pci,bw,rsrp,rsrq|earfcn,pci,bw,rsrp,rsrq
-- here is an example: 55540,234,50,-70.5,-4.5|55690,317,50,-70.8,-4.8|55390,0,0,-69.5,-3.6
function QmiGDecorator:init()
  QmiGDecorator:__invokeChain("init")(self)
  self.optBandstr = "LTE Band 30 - 2300MHz"
  self.default_profile_number = {
    [30] = 1,  -- 'band 30, the default profile: wlldata_apn_number'
    [48] = 5,  -- 'band 48, the default profile: cbrssas_apn_number'
  }
end

function QmiGDecorator:doDecorate()
  QmiGDecorator:__saveChain("init")
  QmiGDecorator:__changeImplTbl({
    "init",
  })
end

function MagpieDecorator.doDecorate()
  MagpieDecorator.__inputObj__.qmiG = QmiGDecorator:decorate(MagpieDecorator.__inputObj__.qmiG)
end

return MagpieDecorator

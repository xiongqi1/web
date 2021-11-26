--[[
  Decorator for BellCa

  NEP-869 Always reattach after band preference change

  Copyright (C) 2021 NetComm Wireless limited.
--]]

local WmmdDecorator = require("wmmd.WmmdDecorator")
local BellCaDecorator = WmmdDecorator:new()
local QmiNasDecorator = WmmdDecorator:new()

function QmiNasDecorator:bandOps()
  local cmd = luardb.get("wwan.0.currentband.cmd.command")
  if cmd == "set" or cmd == "set_hexmask" then
    self.watcher.invoke("sys","detach")
  end

  QmiNasDecorator:__invokeChain("bandOps")(self)

  if cmd == "set" or cmd == "set_hexmask" then
    self.watcher.invoke("sys","attach")
  end
end

function QmiNasDecorator:doDecorate()
  QmiNasDecorator:__saveChain("bandOps")
  QmiNasDecorator:__changeImplTbl({
    "bandOps"
  })
end

function BellCaDecorator.doDecorate()
  BellCaDecorator.__inputObj__.qmiG.qs.qmi_nas = QmiNasDecorator:decorate(BellCaDecorator.__inputObj__.qmiG.qs.qmi_nas)
end

return BellCaDecorator

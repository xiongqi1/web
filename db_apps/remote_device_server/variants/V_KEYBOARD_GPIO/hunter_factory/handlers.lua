-- Copyright (C) 2018 NetComm Wireless limited.
--

local luardb = require('luardb')
local turbo = require("turbo")
local util = require("rds.util")

local config = require("rds.config")
local basepath = config.basepath
local logger = require("rds.logging")


local button_handler_script = "/usr/share/buttonmgr/event.sh"

--------- factory button handler  ----------
local ButtonHandler = class("ButtonHandler", turbo.web.RequestHandler)

function ButtonHandler:put()
  local params = self:get_json()

  logger.logDbg("button handler: got request")

  for k,v in pairs(params) do

    logger.logDbg(string.format("button handler: button state changed (button='%s',val='%s')",k,v))

    -- sanitization
    if k:match('[%w_]+') ~= k then
      logger.logErr(string.format("button handler: incorrect button name format (button='%s',val='%s')",k,v))
      self:set_status(400)
    elseif (v ~= "on") and (v ~= "off") then
      logger.logErr(string.format("button handler: incorrect button state format (button='%s',val='%s')",k,v))
      self:set_status(400)
    else
      os.execute(string.format("%s '%s' '%s'",button_handler_script,k,v))
      self:set_status(200)
    end
  end

end

return {
  {basepath .. "/factory_support/button_state", ButtonHandler},
}

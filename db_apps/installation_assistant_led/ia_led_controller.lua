#!/usr/bin/env lua
-----------------------------------------------------------------------------------------------------------------------
---- Implements core parts of the IA LED controller daemon.
----
---- Copyright (C) 2016 NetComm Wireless limited.
----
-------------------------------------------------------------------------------------------------------------------
require('stringutil');
local turbo = require('turbo');
local luardb = require('luardb');
local script_dir = arg[0]:match('.*/')
local ledmode = require(script_dir .. 'ia_led_mode');

local cfg = {
	POLL_TIMEOUT = 200,
}

-- Called when the period timer expires to update LEDs
local function poll()
	local op_mode = ledmode.get_op_mode()

	for led, controller in pairs(op_mode) do
		controller.update(led, controller.states)
	end
end

local instance = turbo.ioloop.instance()
local cb, rdbFD = luardb.synchronousMode(true)

-- Watches mode specific RDB variables to get notified
instance:add_handler(rdbFD, turbo.ioloop.READ, cb)
for k, v in pairs(ledmode.rdb_watch_vars) do
	luardb.watch(v, poll)
end

instance:set_interval(cfg.POLL_TIMEOUT, poll)
instance:start()


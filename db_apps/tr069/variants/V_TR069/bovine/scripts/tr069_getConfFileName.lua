#!/usr/bin/env lua

require('luardb')

local prefix='tr069-'
local suffix='VendorCfgFile'
local extentions ='.cfg.tar.gz'
local dateTime = os.date('%Y%m%d-%H%M%S')

local v_product=luardb.get('system.product.model')

if not v_product or v_product == '' then
	io.write(prefix .. suffix .. '_' .. dateTime .. extentions)
	return
end

if v_product == 'ntc_6908' then v_product='ntc_6000' end

io.write(prefix .. v_product .. '-' .. suffix .. '_' .. dateTime .. extentions)
return

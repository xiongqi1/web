#!/usr/bin/env lua

-- The format of name 
-- {prefix}-{type}-{v_product}-{serialnum}-{dateTime}.{extentions}
-- Example: tr069-VendorCfg-vdf_nwl10-CDE673-140616_102920.cfg.tar.gz

-- Usage: tr069_getUploadFileName.lua [CfgFile|VendorLog]
--		default argument is "CfgFile"

require('luardb')

local prefix='tr069'
local type='VendorCfg'  -- {VendorCfg|VendorLog}
local v_product=luardb.get('system.product.model')
local serialnum=luardb.get('systeminfo.serialnumber')
local dateTime=os.date('%y%m%d_%H%M%S')
local extentions=".cfg.tar.gz"  -- {.cfg.tar.gz|.log}


if #arg >= 1 then 
	if arg[1] == 'CfgFile' then
		type='VendorCfg'
		extentions=".cfg.tar.gz"
	elseif arg[1] == 'LogFile' then
		type='VendorLog'
		extentions=".log"
	elseif arg[1] == 'XEMScore' then
		type='XEMScoreLog'
		extentions=".tar.gz"
	elseif arg[1] == 'XEMSapp' then
		type='XEMSappLog'
		extentions=".tar.gz"
	end
end

if v_product == 'ntc_6908' then v_product='ntc_6000' end

io.write(prefix .. '-' .. type .. '-' .. v_product .. '-' .. serialnum .. '-' .. dateTime .. extentions)
return

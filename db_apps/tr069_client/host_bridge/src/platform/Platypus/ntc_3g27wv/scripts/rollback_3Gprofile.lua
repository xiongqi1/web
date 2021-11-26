#!/usr/bin/lua

require('stringutil')
require('luardb')
require('luanvram')

-- Example: "valid":"1","APNName":"isp.telus.com","param_index":"1","user":"","modified":"0","pass":"","name":"Telus ISP","readonly":"1","metric":"20","dial":"*99#","activated":"0","auth":"CHAP"
function add_table (tbl, element)
	local templistItem = {}

	local pasedItem = element:explode(',')

	for _ , itemvalue in ipairs(pasedItem) do
		local name, value = itemvalue:match("\"(.*)\":\"(.*)\"")
		templistItem[name]=value
	end

	table.insert(tbl, templistItem)
end



local pf_numOf = tonumber(luardb.get('tr069.saved.numOf3Gprofile'))
local pf_table = {}

if pf_numOf == nil then return 1 end

for i=1, pf_numOf do
	local arr_elem = luardb.get('tr069.saved.3Gprofile'..i)

	if arr_elem ~= nil and arr_elem ~= '' then
		add_table(pf_table, arr_elem)
	end
end

luanvram.close()
luanvram.init()

for i, arrItem in ipairs(pf_table) do
	local setVal = ""
	if arrItem.activated == '1' then
		setVal = '"name":"'..arrItem.name ..
			'","user":"'..arrItem.user ..
			'","pass":"'..arrItem.pass ..
			'","readonly":"'..arrItem.readonly ..
			'","dial":"'..arrItem.dial ..
			'","auth":"'..arrItem.auth ..
			'","metric":"'..arrItem.metric ..
			'","APNName":"'..arrItem.APNName ..'"'

		luanvram.bufset('wwanProfile'..(i-1), setVal)

		luanvram.bufset('wwanProfileIdx', (i-1))

		luanvram.bufset('wwan_APN', arrItem.APNName)
		luanvram.bufset('Dial', arrItem.dial)
		luanvram.bufset('wwan_auth', arrItem.auth)
		luanvram.bufset('wwan_user', arrItem.user)
		luanvram.bufset('wwan_pass', arrItem.pass)
		luanvram.bufset('wwan_metric', arrItem.metric)

	end
end

luanvram.commit()
os.execute("kill -SIGHUP `pidof wwand` 2> /dev/null")
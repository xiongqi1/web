#!/usr/bin/lua

require('luardb')


-- for WNTDv3

rdb='wwan.0.cell_measurement.0'

luardb.set('wwan.0.signal.0.rsrp',  '-84.0')
luardb.set('wwan.0.signal.rsrq',  '-4.0')
luardb.set('wwan.0.signal.rssinr',  '25.0')



pci_list={1,2,2,3,3,4,5,6,7,8,9,10,12,62,63,65}


cnt = 0

for freq = 42000, 42010 do
	for _, pci in pairs(pci_list) do
		cnt = cnt +1
		print(string.format("Test %d: earfcn=%d, pci=%d", cnt, freq, pci))
		luardb.set(rdb, string.format("E,%d,%d", freq, pci))
		if cnt > 6000 then
			break
		end
		luardb.wait(1)	
	end
end

luardb.unset('wwan.0.signal.0.rsrp')
luardb.unset('wwan.0.signal.rsrq')
luardb.unset('wwan.0.signal.rssinr')

luardb.set(rdb, string.format("e,%d,%d", 40000, 1))
luardb.wait(1)	
luardb.set('wwan.0.signal.rssinr',  '26.0')
luardb.set(rdb, string.format("e,%d,%d", 40000, 2))

luardb.set('wwan.0.signal.0.rsrp',  '-85')
luardb.set(rdb, string.format("e,%d,%d", 40000, 3))

luardb.wait(1)	

luardb.set('wwan.0.signal.rsrq',  '-5')
luardb.set(rdb, string.format("e,%d,%d", 40000, 4))

luardb.wait(1)	

#!/usr/bin/env lua


cfg_filename=arg[1]

local check_file = io.open(cfg_filename)

if not check_file then
	print("Error: Invalid cfg_filename")
	return 1
end
check_file:close()


local escapedTR069cfg = os.tmpname ()
if not escapedTR069cfg then 
	return 1 
end

local backupFile = os.tmpname ()
if not backupFile then 
	os.remove(escapedTR069cfg)
	return 1 
end

local tr069cfgFile = os.tmpname ()
if not tr069cfgFile then 
	os.remove(escapedTR069cfg)
	os.remove(backupFile)
	return 1
end

print('backupFile=[' .. backupFile .. '], tr069cfgFile=[' .. tr069cfgFile .. '], escapedTR069cfg=[' .. escapedTR069cfg)
os.execute('cat ' .. cfg_filename .. ' | grep -v tr069 > ' .. escapedTR069cfg)
local pattern="smstools.configured;"
os.execute('test `grep "' .. pattern .. 'YES" ' .. escapedTR069cfg .. '` && echo "' .. pattern .. '" >> ' .. escapedTR069cfg)
os.execute('dbcfg_export -o ' .. backupFile .. ' -p ""')
os.execute('cat ' .. backupFile .. ' | grep tr069 > ' .. tr069cfgFile)
os.execute('cat ' .. tr069cfgFile .. ' >> ' .. escapedTR069cfg)
os.execute('cp ' .. escapedTR069cfg .. ' ' .. cfg_filename)

os.remove(escapedTR069cfg)
os.remove(backupFile)
os.remove(tr069cfgFile)

return 0


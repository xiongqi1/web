dofile('config.lua')

require('Daemon')

local uptime = Daemon.readStringFromFile('/proc/uptime')
assert(type(uptime) == 'string', 'Expected string from Daemon.readStringFromFile()')
print('uptime string', uptime)

uptime = Daemon.readIntFromFile('/proc/uptime')
assert(type(uptime) == 'number', 'Expected number from Daemon.readIntFromFile()')
print('uptime int', uptime)

uptime = Daemon.readEntireFile('/proc/uptime')
assert(type(uptime) == 'string', 'Expected string from Daemon.readEntireFile()')
print('uptime entire', uptime)

local ls = Daemon.readCommandOutput('/bin/ls')
assert(type(ls) == 'string', 'Expected string from Daemon.readCommandOutput()')
print(ls)

local rnd = Daemon.getRandomString(16)
assert(type(rnd) == 'string', 'Expected string from Daemon.getRandomString()')
assert(#rnd == 16, 'Expected 16 char string from Daemon.getRandomString(16)')
print('random "' .. rnd .. '"')

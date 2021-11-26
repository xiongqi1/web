#!/usr/bin/lua

require('socket')

os.execute('iptables -F INPUT')

local sock = socket.tcp()
if sock == nil then 
	print("Socket Error")
	return 1
end

local connection = sock:connect('localhost', 80)
if connection == nil then
	print("Connection Error")
	return 1
end

sock:send('POST /goform/tr069_setLan HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n')
sock:close()
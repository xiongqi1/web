#!/usr/bin/lua
require('luasyslog')
require('stringutil')

luasyslog.open('', 'LOG_DAEMON')

--[[
at this point, table if_params has been set already
--]]


--[[
if_params=
{
	action='stop'
	protocolname="httpd",
	interfacename="httpd_1";
	debug=1

}
--]]




function dump(o)
    if type(o) == 'table' then
        local s = '{ '
        for k,v in pairs(o) do
                if type(k) ~= 'number' then k = '"'..k..'"' end
                s = s .. '['..k..'] = ' .. dump(v) .. ','
        end
        return s .. '}\n'
    else
        return tostring(o)
    end
end


-- get rx tx bytes from interface
local function get_if_rx_tx(if_name)

	local cmd ="ip -o -s   -f inet  link show "..if_name.."";
	local f = assert (io.popen (cmd));
	local s = assert(f:read("*a"));
	f:close();
	--print(s);
--3: eth3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc netem state UP qlen 1000\    link/ether 74:ea:3a:81:93:2c brd ff:ff:ff:ff:ff:ff\    RX: bytes  packets  errors  dropped overrun mcast   --\    811930     13079    0       0       0       0      \    TX: bytes  packets  errors  dropped carrier collsns \    815811     13101    0       0       0       0

	return string.match(s, ".+RX: bytes.+\\%s*(%d+).+TX: bytes.+\\%s*(%d+)");

end


-- The interface name starts with 'diag'

local function execlog(cmd)
  local cmdrd = cmd..' >/dev/null 2>&1'
  if if_params.debug>0 then
    luasyslog.log('LOG_INFO', 'about to '..cmd)
    cmdrd = cmd
  end
  if os.execute(cmdrd) ~= 0 then
    luasyslog.log('LOG_ERR', 'failed to '..cmd)
    --print('failed to '..cmd)
  end
end

local function stop_if(if_name, reset)
  execlog('qd-cos.sh stop ' ..if_name)
   if reset then
  	execlog('ifconfig '..if_params.interfacename..' 0.0.0.0')
  	execlog('ip vcconf '..if_params.interfacename..' local 0 remote 0')

  else
	execlog('ip link set '..if_name..' down')
	execlog('ip link delete '..if_name..' type vc')
  end
end


local rx, tx = get_if_rx_tx(if_params.interfacename);

if if_params.debug>0 then

	print(dump(if_params));

	print(rx, tx);
end

if if_params.interfacename  then


	if if_params.action == 'stop' then
		luasyslog.log('LOG_INFO', 'interface \"'.. if_params.interfacename.. '\" is ready to remove')

		stop_if(if_params.interfacename);
	elseif if_params.action == 'clear' then
		luasyslog.log('LOG_INFO', 'interface \"'.. if_params.interfacename.. '\" is ready to clear')
		stop_if(if_params.interfacename, true);

	end
end

return rx, tx;

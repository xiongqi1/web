#!/usr/bin/env lua

local self = {}

local fdagent

self.init = function ()
   -- get rid of previous queue setter if present
   os.execute('iptables -t nat -D POSTROUTING -p udp --sport 68 --dport 67 -j QUEUE >/dev/null')
   -- add queue setter
   os.execute('iptables -t nat -A POSTROUTING -p udp --sport 68 --dport 67 -j QUEUE')

  local cmd = '/usr/bin/nf-dhcp82'
  fdagent = io.popen(cmd, 'w')
  return fdagent, cmd
end

self.terminate = function ()
  if fdagent then
    fdagent:close()
    fdagent = nil
  end
end

self.agent = function (iface, id, agentid)
  if not fdagent then return end
  local res
  if not agentid then
    res = fdagent:write(iface..' '..id..'\n')
  else
    res = fdagent:write(iface..' '..id..' '..agentid..'\n')
  end
  if not res then
    self.terminate()
  end
  return res
end
 
return self

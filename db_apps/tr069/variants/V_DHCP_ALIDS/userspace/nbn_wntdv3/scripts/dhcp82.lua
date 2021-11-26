#!/usr/bin/env lua

local self = {}

local fdagent

self.init = function ()
   -- get rid of previous queue setter if present
   os.execute('iptables -t mangle -D POSTROUTING -p udp --sport 68 --dport 67 -j QUEUE >/dev/null')
   -- add queue setter
   os.execute('iptables -t mangle -A POSTROUTING -p udp --sport 68 --dport 67 -j QUEUE')
   -- get rid of previous queue setter if present
   os.execute('ip6tables -t mangle -D POSTROUTING -p udp --dport 547 -j QUEUE >/dev/null')
   -- add queue setter
   os.execute('ip6tables -t mangle -A POSTROUTING -p udp --dport 547 -j QUEUE')

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
  file=io.open("/opt/cdcs/upload/log.log","w")
  io.output(file)
  if not agentid then
    res = fdagent:write(iface..' '..id..'\n')
  else
    res = fdagent:write(iface..' '..id..' '..agentid..'\n')
    io.write("agent id:" .. agentid)
  end
  if not res then
    self.terminate()
  end
  io.write("\r\n")
  io.close(file)
  return res
end

return self

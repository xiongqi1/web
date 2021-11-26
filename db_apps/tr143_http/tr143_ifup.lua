#!/usr/bin/env lua

require('luardb')
require('luasyslog')
require('stringutil')
require('tableutil')

luasyslog.open('', 'LOG_DAEMON')
conf = dofile('/usr/lib/tr-069/config.lua')



--[[
if_params=
{
	--action='start',		-- or 'stop' or 'change'
	protocolname="httpd",
	interfacename="httpd_1",
	serveraddress="192.168.30.2", --"59.167.232.89",
	mplstag="4001",
	cos="0",
	costoexp="",
	costodscp="",
	interfaceaddress="192.168.30.5",
	interfacenetmask="255.255.255.0",
	smartedgeaddress="192.168.0.71",
	ifbuildmethod=0,
	tc1_cir=150000,
	tc2_pir=10000000,
	tc4_pir=1000000,
	debug=1
}
--]]
local IF_BUILD_CREATE_NEW	=0  --	"\t   	0 -- create new interface(delete all conflicted interface)\n"
local IF_BUILD_SAME_IF_NAME=1 --	"\t   	1 -- use same if_name interface\n"
local IF_BUILD_SIMILAR_IF_NAME=2 --	"\t   	2 -- use similary if_name interface\n"
local IF_BUILD_EXIST=	3 --	"\t   	3 -- use same address of interface\n"
local IF_BUILD_EXIST_DEL=	4 --	"\t   	4 -- use same address of interface\n"
local IF_BUILD_SAME_IF_NAME_KEEP=5  --	"use same name of interface and keep created one\n"



--if_params.interfacename=if_params.ifname;   -- needed for 'stop'



--[[
at this point, table if_params has been set already
--]]



local function dump(o)
    if type(o) == 'table' then
        local s = '{ '
        for k,v in pairs(o) do
                if type(k) ~= 'number' then k = '"'..k..'"' end
                s = s .. '['..k..'] = ' .. dump(v) .. ','
        end
        return s .. '} '
    else
        return tostring(o)
    end
end
-- get if_index and ip
-- return if_index, if_name
local function get_if_index_ip(ip)

	local cmd ="ip -o -f inet addr |grep "..ip;

	--print(cmd);
	local f = assert (io.popen (cmd));
	local s = assert(f:read("*a"));
	--print(s);
	f:close();

  --3: eth3    inet 192.168.0.3/24 brd 192.168.0.255 scope global eth3\       valid_lft forever preferred_lft forever
	return string.match(s, "^(%d+):%s*([%w:%._]+)")
end

-- get if_index and if_name
-- return if_index, if_name
local function get_if_index_name(if_name)

	local cmd ="ip -o -f inet link |grep "..if_name;

	--print(cmd);
	local f = assert (io.popen (cmd));
	local s = assert(f:read("*a"));
	--print(s);
	f:close();

  --19: httpd_0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1658 qdisc pfifo_fast state UNKNOWN qlen 1000\    link/ether 72:66:39:bc:9e:cd brd ff:ff:ff:ff:ff:ff
	--return string.match(s, "^(%d+):%s*([%w%:%.]+)%s+");
	return string.match(s, "^(%d+):%s*([%w:%._]+):")
end

-- get rx tx bytes from interface
local function get_if_rx_tx(if_name)

	local cmd ="ip -o -s   -f inet  link show "..if_name.."";
	local f = assert (io.popen (cmd));
	local s = assert(f:read("*a"));
	f:close();

--3: eth3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc netem state UP qlen 1000
--\    link/ether 74:ea:3a:81:93:2c brd ff:ff:ff:ff:ff:ff
--\    RX: bytes  packets  errors  dropped overrun mcast   --
--\    811930     13079    0       0       0       0      \    TX: bytes  packets  errors  dropped carrier collsns
--\    815811     13101    0       0       0       0
	return  if_name, string.match(s, ".+RX: bytes.+\\%s*(%d+).+TX: bytes.+\\%s*(%d+)");

end



--[[

   if debug output required, set debug to a level, otherwise leave
   nil

   Action: start, stop or change
     change is needed in case link.profile.2.address changes
     This will have to be watched for

   The if_params table is set on file loaded. The code returns true
   for all OK, nil for something wrong.

   For start, the code tries to pull the interface down first. It is
   not an error if it hasn't been set up.
--]]

local function musthave(names)
  local good = true
  for _,n in ipairs(names) do
    if not if_params[n] then
       luasyslog.log('LOG_ERR', 'parameter '..n..' is missing');
       --print('parameter '..n..' is missing');
       good = false
    end
  end
  return good
end

local neednames = {
	'serveraddress',
	'mplstag',
	'cos',
	'interfaceaddress',
	'interfacenetmask',
	'smartedgeaddress',
}


-- The interface name starts with 'diag'

local function execlog(cmd)
  local cmdrd = cmd..' >/dev/null  2>&1'
  if if_params.debug>0 then
    luasyslog.log('LOG_INFO', 'about to '..cmdrd)
    --print( 'about to '..cmd)
    cmdrd = cmd
  end

  if os.execute(cmdrd) ~= 0 then
    luasyslog.log('LOG_ERR', 'failed to '..cmd)
    --print('failed to '..cmd)
  end
end



local function change_if(ourLocalGreAddress)
  --local ourLocalGreAddress = luardb.get('link.profile.'..conf.wntd.wanProfile..'.address')
  local flags = 0x163
  if if_params.debug > 0 then
    flags = flags + 0x8000
  end
  --print(ourLocalGreAddress);

  local gre = 'ip vcconf '..if_params.interfacename..' remote '..if_params.smartedgeaddress..' local '..ourLocalGreAddress..' mpls-label '..if_params.mplstag..' flags '..flags..' default-tx-cos '..if_params.cos..' default-rx-cos '..if_params.cos
  -- if avc.dscp and string.len(avc.dscp) then
  --   gre = gre .. ' --dscp ' .. avc.dscp
  -- end
  execlog(gre)
end

local function stop_if(if_name)

	if if_params.debug>0 then
			print("ifname exist, stop it:".. if_name);
	end
  execlog('qd-cos.sh stop ' ..if_name)
  execlog('ip link set '..if_name..' down')
  execlog('ip link delete '..if_name..' type vc')
end

local function start_if(create_new)

  local link_profile_addr = 'link.profile.'..conf.wntd.wanProfile..'.iplocal';
  local ourLocalGreAddress = luardb.get(link_profile_addr)
  if not ourLocalGreAddress or   not string.match(ourLocalGreAddress, "%d+%.%d+%.%d+%.%d")  then
	if if_params.debug>0 then
		print("local address (".. link_profile_addr ..") is not defined, cannot build ".. if_params.interfacename);
	end
	return ;
  end
  local link_profile_interface=   luardb.get('link.profile.'..conf.wntd.wanProfile..'.interface') or ''
  if string.len(link_profile_interface) ==0 then
	return
  end

  if create_new then
	execlog('ip link add '..if_params.interfacename..' type vc')
  end
  local qdisc = if_params.interfacename..' default '..if_params.cos
  if if_params.debug>0 then
    qdisc = qdisc .. ' --debug'
  end
  --os.execute("ifconfig ");
	if if_params.tc1_cir and tonumber(if_params.tc1_cir) > 0 then
		qdisc = qdisc ..' --tc1 ' .. if_params.tc1_cir.. ' --tc1margin 100';
	end
	if if_params.tc2_pir and tonumber(if_params.tc2_pir) > 0 then
		qdisc = qdisc ..' --tc2 ' .. if_params.tc2_pir.. ' --tc2margin 100';
	end
	if if_params.tc4_pir and tonumber(if_params.tc4_pir) > 0 then
		qdisc = qdisc ..' --tc4 ' .. if_params.tc4_pir.. ' --tc4margin 100';
	end

  execlog('qd-cos.sh start '..qdisc )
  change_if(ourLocalGreAddress)
  execlog('ip link set '..if_params.interfacename..' up')
  local mtu =luardb.get('unid.max_frame_size');
  if mtu then
	--mtu = mtu-8+20-- 8(crc+valn) + 20 (ip) see mplsd
	mtu = mtu- 22
	execlog('ip link set '..if_params.interfacename..' mtu '.. mtu)
  end
  execlog('ip addr add '..if_params.interfaceaddress..' broadcast '..if_params.interfacenetmask..' dev '..if_params.interfacename)
  --execlog('ifconfig ' .. if_params.interfacename .. ' '..if_params.interfaceaddress .. ' netmask ' ..if_params.interfacenetmask)
  execlog('ip route add '..if_params.serveraddress..'/32 dev '..if_params.interfacename)


  if if_params.smartedgeaddress then
	-- if route table doesnot have wan interface, add it
	if os.execute("ip route show |grep -q "..if_params.smartedgeaddress) ~= 0 then
		execlog('ip route add '..if_params.smartedgeaddress ..' dev '..link_profile_interface)
	end
  end

end





--print("ifup script for ".. if_params["ProtocolName"]);
if if_params.debug>0 then
	print(dump(if_params));
end

local if_isTmp =1;
--if_name="eth1:99";


if  not if_params.interfaceaddress then
	return null, null, null;
end

if not musthave(neednames) then
  return nil
end

-- need stop exist interface
--1) same interface name, this is created by this process, so stop it !!!


--1) check interface with same name
local if_index_sn, if_name_sn = get_if_index_name( if_params.interfacename);

local if_index_sip, if_name_sip = get_if_index_ip( if_params.interfaceaddress);

---[[
if if_name_sip then
	if if_params.debug>0 then
		luasyslog.log('LOG_INFO', 'found exist interface with '..if_params.interfaceaddress)
	end
	-- if the interface exist but not have requested IP address, stop it!!!
	if  if_name_sn and if_name_sn ~= if_name_sip then
		stop_if(if_name_sn);

	end

 	if if_params.ifbuildmethod == IF_BUILD_EXIST or
			if_params.ifbuildmethod == IF_BUILD_EXIST_DEL then
	-- existing, should reuse it !
		if_params.interfacename = if_name_sip;
		if_isTmp =0;
	elseif if_params.ifbuildmethod == IF_BUILD_SIMILAR_IF_NAME then
		-- create by similar session ! reuse
		if string.find(if_name_sip, "http") or string.find(if_name_sip, "ping") then
			if_params.interfacename = if_name_sip;
		else
			stop_if(if_name_sip);
			start_if(true);
		end

	elseif if_params.ifbuildmethod == IF_BUILD_SAME_IF_NAME or
		if_params.ifbuildmethod ==IF_BUILD_SAME_IF_NAME_KEEP then
		-- same if_name and ip, reuse
		if if_name_sip == if_params.interfacename then
			if_isTmp =0;

			start_if();
		else
			stop_if(if_name_sip);
			start_if(true);
		end
	else
		-- create new ifname any way
			stop_if(if_name_sip);
			start_if(true);

	end
else
	if  if_name_sn then
		if if_params.ifbuildmethod == IF_BUILD_CREATE_NEW then
			stop_if(if_name_sn);
			start_if(true);
		else
			start_if();
		end

	else
		start_if(true);
	end


end --if if_name_sip then

 if if_params.debug>0 then
	--print (if_isTmp, get_if_index_name(if_params.interfacename));
	print (if_isTmp,get_if_rx_tx(if_params.interfacename));
 end

local link_profile_interface=   luardb.get('link.profile.'..conf.wntd.wanProfile..'.dev') or ''
--format: is_tmp, if_name, rx, tx
return if_isTmp, link_profile_interface, get_if_rx_tx(if_params.interfacename);
--]]


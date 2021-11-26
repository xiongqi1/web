--[[
    @file
    TR-143 utility functions

    Copyright (C) 2019 Casa Systems.
--]]

--[[
    Resolve a host/domain name to IP addresses

    @param host The host name to be resolved
    @retval {ipv4 addresses}, {ipv6 addresses}
    @retval nil on error
--]]
local function nsLookup(host)
    local ipv4 = {}
    local ipv6 = {}
    local ipaddr
    local f = io.popen('nslookup ' .. host)
    if not f then
        return nil
    end
--[[
root:~# nslookup netcommwireless.com
Server:    127.0.0.1
Address 1: 127.0.0.1 localhost

Name:      netcommwireless.com
Address 1: 2606:4700::6812:9d21 xxxxxxx.com
Address 2: 2606:4700::6812:9c21 xxxxxxx.com
Address 3: 104.18.156.33 xxxxxxx.com
Address 4: 104.18.157.33 xxxxxxx.com

On Cassini, nslookup returns
Server:		172.22.10.2
Address:	172.22.10.2:53

Non-authoritative answer:
Name:	netcommwireless.com
Address: 104.18.157.33
Name:	netcommwireless.com
Address: 104.18.156.33

Non-authoritative answer:
Name:	netcommwireless.com
Address: 2606:4700::6812:9c21
Name:	netcommwireless.com
Address: 2606:4700::6812:9d21
--]]
    -- skip the Server section
    while true do
        local line = f:read()
        if not line then
            f:close()
            return nil
        end
        if line:match('^Name:') then
            break
        end
    end
    -- return all addresses
    while true do
        local line = f:read()
        if not line then
            break
        end
        -- ipv4
        ipaddr = line:match('^Address%s*%d*:%s+(%d+%.%d+%.%d+%.%d+)')
        if ipaddr then
            ipv4[#ipv4 + 1] = ipaddr
        else
            -- ipv6: not a rigid test
            ipaddr = line:match('^Address%s*%d*:%s+([0-9a-fA-F:]+)')
            if ipaddr then
                ipv6[#ipv6 + 1] = ipaddr
            end
        end
    end
    f:close()
    return ipv4, ipv6
end

--[[
    Lookup the route to a given destination

    @param ipaddr The IP address (IPv4 or IPv6) of the destination as a string
    @return The outgoing network interface to reach the destination or nil on error
--]]
local function routeLookup(ipaddr)
    local f = io.popen('ip route get ' .. ipaddr)
    if not f then
        return nil
    end
--[[
root:~# ip route get 8.8.8.8
8.8.8.8 via 123.209.234.13 dev wwan1  src 123.209.234.14
    cache
--]]
    local p = f:read() -- the first line has everything we need
    f:close()
    if not p then
        return nil
    end
    return p:match('dev%s+(%S+)') -- match any non-space characters after dev
end

--[[
    Check a host/IP address for name resolution and route

    @param host The host or IP address to check
    @param proto The IP protocol to check "Any|IPv4|IPv6"
    @retval ipaddr, dev, addr_type: IP address that is verified successfully, the network interface for routing and addr type 4|6
    @retval nil, error string on verification failure
--]]
local function hostCheck(host, proto)
    local ipaddr = host
    local host_is_ipv4 = host:find("^%d+%.%d+%.%d+%.%d+$")
    local host_is_ipv6 = host:find("^[0-9a-fA-F:]+$") and host:find(":+.*:+")
    local addr_type = host_is_ipv4 and 4 or 6
    if (host_is_ipv4 and proto == 'IPv6') or (host_is_ipv6 and proto == 'IPv4') then
        return nil, 'Error_CannotResolveHostName'
    end
    if not (host_is_ipv4 or host_is_ipv6) then
        -- resolve name
        local ipv4, ipv6 = nsLookup(host)
        if not ipv4 then
            return nil, 'Error_CannotResolveHostName'
        end
        if (proto == 'IPv4' and #ipv4 == 0) or (proto == 'IPv6' and #ipv6 == 0)  or (#ipv4 == 0 and #ipv6 == 0) then
            return nil, 'Error_CannotResolveHostName'
        end
        if #ipv4 > 0 and (proto == 'IPv4' or proto == 'Any') then
            ipaddr = ipv4[1]
            addr_type = 4
        else
            ipaddr = ipv6[1]
            addr_type = 6
        end
    end
    -- check route
    local dev = routeLookup(ipaddr)
    if not dev then
        return nil, 'Error_NoRouteToHost'
    end
    return ipaddr, dev, addr_type
end

return {
    hostCheck = hostCheck,
}

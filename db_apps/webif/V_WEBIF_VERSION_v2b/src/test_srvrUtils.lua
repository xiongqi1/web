#!/usr/bin/env lua
require('./cgi-bin/srvrUtils')

function Test(fn, data, result)
  for _, d in ipairs(data) do
    print(d)
    if result then
      assert(fn(d), d..' should be valid')
    else
      assert(not fn(d), d..' should be invalid')
    end
  end
end

function Test2(fn, data, d2, result)
  for _, d in ipairs(data) do
    print(d)
    if result then
      assert(fn(d,d2), d..' should be valid')
    else
      assert(not fn(d,d2), d..' should be invalid')
    end
  end
end

function isValidTest()
  Test(isValid.Ipv6Address, {"999.12345.0.0001","1050:0:0:5:600:192.168.1.1:af", "1050:0:0:0:5:600:300c:326babcdef","1050:::garbage:5:1000::"}, false)
  Test(isValid.Ipv6Address, {"192.168.0.1","0.0.0.0","1.1.1.1","255.255.255.255"}, false)
  Test(isValid.Ipv6Address, {"1050:0:0:0:5:600:300c:326b","1050:0000:0000:0000:0005:0600:300c:326b","1050:0:0:0:5:600:192.168.1.1","::1"}, true)
  Test(isValid.IpAddress, {"192.168.0.1","0.0.0.0","1.1.1.1","255.255.255.255"}, true)
  Test(isValid.IpAddress, {"a.2.2.2", "1.1.1","1.1", "1","256.1.1.1"}, false)
  Test2(isValid.Enum, {"opt0","opt1","opt2","opt3"}, {"opt0","opt1","opt2","opt3"}, true)
  Test2(isValid.Enum, {"opt4","XXXX","wtf"}, {"opt0","opt1","opt2","opt3"}, false)
  Test(isValid.Hostname, {"a","abc","abc.def","abc2.def","2abc.def"}, true)
  Test(isValid.Hostname, {"","2","abc!.def"}, false)
  Test(isValid.Username, {"fred","2fred","fred2","fr_e-d"}, true)
  Test(isValid.Username, {"","fr$ed",'fre"d',"fr<ed","fr>ed","fr%ed",
                          "fr\\ed","fr^ed","fre[d","fre]d",
                          "fr`ed","fr+ed","fr,ed","fr=ed","fr'ed",
                          "fr#ed","fr&ed", "fre.d","fre:d","fre\td"}, false)
end
isValidTest()
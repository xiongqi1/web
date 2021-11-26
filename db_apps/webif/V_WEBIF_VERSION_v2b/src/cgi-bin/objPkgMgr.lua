
function getKeyValue(line)
  local words = {}
  for word in line:gmatch("%S+") do
    table.insert(words,word)
  end
  if #words > 1 then
    local key = words[1]:gsub( ':', '')
    local value = words[2]
    if #words > 2 then
      for i = 3,#words do
        value = value..' '..words[i]
      end
    end
    return key, value
  end
end

function getPkgs(authenticated) local obj = {}
  local pkg = {}
  for line in io.lines('/usr/lib/ipkg/status') do
    local key,value = getKeyValue(line)
    if key and value then
      pkg[key] = value
      if key == 'Installed-Time' then
        pkg.uninstall=true
        if pkg.Package then
          pkg.filelist = {}
          for file in io.lines('/usr/lib/ipkg/info/'..pkg.Package..'.list') do
            table.insert(pkg.filelist,file)
          end
          pkg.control = {}
          for cntl in io.lines('/usr/lib/ipkg/info/'..pkg.Package..'.control') do
            key,value = getKeyValue(cntl)
            if key then pkg.control[key] = value end
          end
        end
        table.insert(obj,pkg)
        pkg = {}
      end
    end
  end
  return obj
end

objHander = {
  authenticatedOnly=true,
  get = getPkgs,
  validate = function(obj) return true end,
  set = function(authenticated,obj)
    if obj.uninstall then
#if V_PARTITION_LAYOUT_fisher_ab
      local cmd = "flashtool --remove-ipk '"..obj.uninstall.."' >/dev/null"
#else
      local cmd = "ipkg-cl remove '"..obj.uninstall.."' >/dev/null"
#endif
      local res = os.execute(cmd)
    end
    return getPkgs(authenticated)
  end
}

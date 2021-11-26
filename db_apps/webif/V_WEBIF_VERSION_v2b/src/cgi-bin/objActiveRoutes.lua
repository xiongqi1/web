function getRoutes(authenticated) local obj = {}
  local tmpFile = '/tmp/ActiveRoutes'
  os.execute('route -n >'..tmpFile)
  for line in io.lines(tmpFile) do
     table.insert(obj,line)
  end
  os.execute('rm '..tmpFile)
  return obj
end

objHander = {
  authenticatedOnly=true,
  get = getRoutes,
  validate = function(obj) return true end,
  set = function(authenticated,obj)
    return getRoutes(authenticated)
  end
}

objHander = {
  get = function(authenticated) local obj = {}
    -- return the system language
    obj.lang = luardb.get('webinterface.language') or 'en'
    if authenticated then
        obj.private = 'my secret'
    end
    return obj
  end,
  validate = function(obj) return true end,
  set = function(authenticated,obj)
  end
}

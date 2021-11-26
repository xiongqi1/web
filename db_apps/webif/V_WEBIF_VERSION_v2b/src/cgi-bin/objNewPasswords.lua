-- Sets the web root, web user and UNIX root passwords from the landing.html page.
--
-- Copyright (C) 2018 NetComm Wireless Limited.


-- Build time selection of the "other" username.
#ifdef V_WEBIF_SPEC_vdf
    local NON_ROOT_USERNAME = "user"
#else
    local NON_ROOT_USERNAME = "admin"
#endif

#if !defined(V_PRODUCT_ntc_220)
-- Updates the root credentials by making a system call to chpasswd.
-- Note that we get no feedback from the process as to whether it has worked or not.
function set_root_password(password)
    local handle = io.popen("/usr/sbin/chpasswd --md5", "w")
    handle:write("root:" .. password)
    handle:close()
end

-- function set_root_password(password)
--     ret_val = os.execute("echo 'root:" .. password .. "' | chpasswd --md5")
--     if ret_val ~= 0 then
--         error("password set failed")
--     end
-- end

-- Read through the UNIX password file /etc/shadow to determine the md5 hash of the current root
-- password.  Return the value.
function get_encrypted_root_password()
    for line in io.lines("/etc/shadow") do
        match = string.match(line, "^root:($1$[^:]+):[0-9:]+$")
        if match then
            return match
        end
    end
    error("root user missing")
end
#endif

-- Does a replacement password meet complexity requirements?
--
--TEMP!!! This is a stand-in for a proper implementation.  We only check length. As a consequence
--we are relying entirely on the client-side checking done in JavaScript.
function password_is_weak(password)
    return string.len(password) < 8
end

objHander = {
    pageURL='/landing.html',
    -- This page would be called as part of the authentication process.
    authenticatedOnly=false,

    --
    get=function(authenticated)
        return {}
    end,

    -- The passwords should already have been validated by the Javascript code, but it doesn't
    -- hurt to check again.
    validate=function(obj)
        for _, key in ipairs({"webRootPassword", "webUserPassword", "sshRootPassword"}) do
            password = decode(obj[key])
            if password_is_weak(password) then
                return false, "weak password for " .. key
            end
        end
        return true
    end,

    -- Set all the passwords in one foul swoop.
    set=function(authenticated, obj)
        -- These are straight forward RDB variables.
        local rootName = "admin.user.root"
        local userName = "admin.user." .. NON_ROOT_USERNAME
#if defined(V_PRODUCT_ntc_220)
        luardb.set(rootName .. ".new", decode(obj.webRootPassword))
        luardb.set(userName .. ".new", decode(obj.webUserPassword))
#else
        luardb.set(rootName, decode(obj.webRootPassword))
        luardb.set(userName, decode(obj.webUserPassword))
#endif

        -- The UNIX root passwords takes a little more doing.
#if defined(V_PRODUCT_ntc_220)
        luardb.set("telnet.passwd.new", decode(obj["sshRootPassword"]))
#else
        set_root_password(decode(obj["sshRootPassword"]))
        local encrypted = get_encrypted_root_password()
        luardb.set("telnet.passwd.encrypted", encrypted)
#endif
        -- We are now no longer running in factory mode.  By clearing this flag the user will no
        -- longer be directed to the landing page.
        luardb.set("admin.factory_default_passwords_in_use", 0)

        -- The phone module would have been turned off for security, let's turn it back on giving us
        -- WWAN connectivity.
        os.execute("/usr/sbin/sys -m 1")
    end
}

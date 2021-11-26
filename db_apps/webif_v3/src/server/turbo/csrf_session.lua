--[[
    CSRF session module

    Session data is fully contained in a secure cookie named 'session'.
    There is no session data stored at server side.
    At present, a session object includes the following fields:
      csrf: CSRF token, which is a unique random string per session.
            This field is mandatory for all sessions even pre-login.
      user: User name if a valid user has logged in successfully.
            This field is not present if user has not logged in.

    Copyright (C) 2020 Casa Systems
--]]

local ffi = require "ffi"
local turbo = require "turbo"
require "stringutil"

ffi.cdef[[
int RAND_bytes(unsigned char *buf, int len);
]]
local lssl = ffi.load("ssl")

-- generate a string of crypto secure pseudorandom numbers
-- @param len The length of string to be generated
-- @return The random string
local function cryptoSecureRandomBytes(len)
    local buf = ffi.new("char[?]", len)
    if lssl.RAND_bytes(buf, len) == 0 then
        return
    end
    return ffi.string(buf, len)
end

-- calculate XOR of two binary strings
local function xor(a, b)
    local c = {}
    for k, v in pairs({string.byte(a, 1, #a)}) do
        c[k] = bit.bxor(v, string.byte(b, k))
    end
    return string.char(unpack(c))
end

-- url safe base64 encoding
local function b64UrlEncode(buf)
    return turbo.escape.base64_encode(buf, #buf, true)
        :gsub('/', '_'):gsub('+', '-'):gsub('=', '')
end

-- url safe base64 decoding
local function b64UrlDecode(str)
    return turbo.escape.base64_decode(
        str:gsub('-', '+'):gsub('_', '/'), #str)
end

local CSRF_SECRET_LEN = 33
local CSRF_TOKEN_LEN = math.ceil(4 * 2 * CSRF_SECRET_LEN / 3) -- length without padding

-- A CSRF token is generated from a secret of length CSRF_SECRET_LEN
-- In order to protect against BREACH attack, the token is not simply the secret
-- A random mask of the same length is applied to the secret to scramble it
-- The concatenation of the mask and the scrambled secret is the CSRF token

-- @param mask a secret
-- @param secret The secret string
-- @return The concatenation of a random mask and the scrambled secret (a CSRF token)
local function maskCipherSecret(secret)
    local mask = cryptoSecureRandomBytes(#secret)
    local masked = mask .. xor(secret, mask)
    return b64UrlEncode(masked)
end

-- Given a CSRF token, return the secret
-- @param token A CSRF token
-- @return The secret of the token
local function unmaskCipherToken(token)
    local masked = b64UrlDecode(token)
    return xor(masked:sub(-CSRF_SECRET_LEN), masked:sub(1, CSRF_SECRET_LEN))
end

-- generate a brand new CSRF token
local function generateCsrfToken()
    return maskCipherSecret(cryptoSecureRandomBytes(CSRF_SECRET_LEN))
end

-- verify two CSRF tokens by comparing their secrets
-- @return true if two tokens are euqivalent; false otherwise
local function verifyCsrfToken(token1, token2)
    if type(token1) ~= 'string' or type(token2) ~= 'string' then
        return false
    end
    if #token1 ~= CSRF_TOKEN_LEN or #token2 ~= CSRF_TOKEN_LEN then
        return false
    end
    return unmaskCipherToken(token1) == unmaskCipherToken(token2)
end

local SESSION_COOKIE_NAME = 'session'
local SESSION_EXPIRY = 1800 -- 30 minutes
local SESSION_PATH = '/'

-- get session object from cookie
-- @param request handler from turbo
-- @return The session object obtained from the cookie
-- @note this will verify timestamp and signature
-- if verification fails, an error will be raised
local function getSessionCookie(reqHandler)
    local cookie = reqHandler:get_secure_cookie(SESSION_COOKIE_NAME, nil, SESSION_EXPIRY)
    --turbo.log.debug(string.format('cookie: %s', cookie))
    local session = turbo.escape.json_decode(cookie)
    return session
end

-- set a session cookie from a session object
-- @param reqHandler request handler from turbo
-- @param session The session object to be set in the cookie
local function setSessionCookie(reqHandler, session)
    local cookie = turbo.escape.json_encode(session)
    -- we patched turbo to use Max-Age together with HTTPOnly and SameSite
    reqHandler:set_secure_cookie(SESSION_COOKIE_NAME, cookie, SESSION_PATH,
                                 -1, SESSION_EXPIRY, false, true, 'strict')
end

-- check if a session object is valid
local function isSessionValid(session)
    return type(session) == 'table' and session.csrf
end

-- prepare session object by processing the cookie
-- this should be called by most handlers as first step
local function prepareSession(reqHandler)
    local status, session = pcall(getSessionCookie, reqHandler)
    if not status or not isSessionValid(session) then
        if not status then
            turbo.log.debug(string.format('get cookie failed: %s', session))
            --turbo.log.debug(string.format('raw cookie: %s', reqHandler:get_cookie(SESSION_COOKIE_NAME)))
        end
        -- no valid session cookie, generate a blank one
        session = {}
    end
    reqHandler.session = session -- attach session to request handler
end

-- replace the CSRF token in session with a new one
local function updateCsrfToken(reqHandler)
    reqHandler.session.csrf = generateCsrfToken()
end

-- get a CSRF token for a html page while keeping the same secret from session
-- if session is invalid, a new session is generated first
-- this is only called in MustacheHandler:get() right before rendering a page
local function getPageCsrfToken(reqHandler)
    local session = reqHandler.session
    if not isSessionValid(session) then
        -- we are going to render a page shortly, it's ok to change token
        session.csrf = generateCsrfToken()
    end
    local secret = unmaskCipherToken(session.csrf)
    return maskCipherSecret(secret)
end

-- update session cookie
-- this will resign the cookie with new timestamp
-- should be called before finish()
local function updateSession(reqHandler)
    -- we should always update cookie to extend expiry
    local session = reqHandler.session
    if not isSessionValid(session) then
        session = {}
        session.csrf = generateCsrfToken()
    end
    setSessionCookie(reqHandler, session)
end

return {
    cryptoSecureRandomBytes = cryptoSecureRandomBytes,
    verifyCsrfToken = verifyCsrfToken,
    isSessionValid = isSessionValid,
    prepareSession = prepareSession,
    updateCsrfToken = updateCsrfToken,
    getPageCsrfToken = getPageCsrfToken,
    updateSession = updateSession,
}

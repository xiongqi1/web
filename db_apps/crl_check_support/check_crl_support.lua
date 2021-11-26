--[[
Copyright (C) 2019 Casa Systems Inc

Helper module to support checking CRL
Working with lua turbo

--]]

local ffi = require("ffi")
local lssl = ffi.load("ssl")
local luardb = require("luardb")

--[[
Need X509_STORE_CTX_get_ex_new_index, which is defined in include/openssl/crypto.h:
#define X509_STORE_CTX_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_X509_STORE_CTX, l, p, newf, dupf, freef)
# define CRYPTO_EX_INDEX_X509_STORE_CTX   5

--]]
ffi.cdef [[
typedef void CRYPTO_EX_DATA;
typedef void CRYPTO_EX_new (void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                           int idx, long argl, void *argp);
typedef void CRYPTO_EX_free (void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                             int idx, long argl, void *argp);
typedef int CRYPTO_EX_dup (CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
                           void *from_d, int idx, long argl, void *argp);
int CRYPTO_get_ex_new_index(int class_index, long argl, void *argp,
                            CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
                            CRYPTO_EX_free *free_func);
int CRYPTO_free_ex_index(int class_index, int idx);

typedef void X509_STORE_CTX;
X509 *X509_STORE_CTX_get_current_cert(X509_STORE_CTX *ctx);
int X509_digest(const X509 *data, const EVP_MD *type, unsigned char *md, unsigned int *len);
void X509_STORE_CTX_set_error(X509_STORE_CTX *ctx, int s);

typedef void FILE;
int i2d_X509_fp(FILE *fp, X509 *x);
]]

local application_name
local application_ca_path
local trust_root_ca = false
local crl_download_max_time_secs = 10

local tmp_dir="/tmp/"

local x509_app_data_index
local current_chain_index = 0

--[[
In include/openssl/x509_vfy.h:# define
# define         X509_V_ERR_UNABLE_TO_GET_CRL                    3
# define         X509_V_ERR_CERT_REVOKED                         23
--]]
local X509_V_ERR_UNABLE_TO_GET_CRL = 3
local X509_V_ERR_CERT_REVOKED = 23

local INTERNAL_CRL_PROCESSING_ERROR = 254

-- Encodes a string into its escaped hexadecimal representation
-- @param s String to encode
local function hex_encode(s)
  return string.gsub(s, ".", function(c) return string.format("%02x", string.byte(c)) end)
end

-- Set persist RDB if value is changed
local function set_persist_rdb_if_changed(name, value)
  if luardb.get(name) ~= value then
    luardb.set(name, value, "p")
  end
end

-- This function is passed as callback new_func to X509_STORE_CTX_get_ex_new_index.
-- new_func() is called when X509 store context is initially allocated.
-- Reset current chain index here.
-- See openssl documentation for description of parameters of new_func()
local function x509_store_new_func(parent, ptr, ad, idx, argl, argp)
  current_chain_index = 0
end

-- Initialize
-- @param app_name Application name
-- @param ca_path Path to CA directory
-- @param trust_root_ca_arg (boolean) true: no need to check CA certificate; false: need check CA certificate
local function init(app_name, ca_path, trust_root_ca_arg, crl_download_time_arg)
  application_name = app_name
  application_ca_path = ca_path
  trust_root_ca = trust_root_ca_arg or false
  crl_download_max_time_secs = tonumber(crl_download_time_arg) or 10

  -- In include/openssl/crypto.h: #define CRYPTO_EX_INDEX_X509_STORE_CTX   5
  x509_app_data_index = lssl.CRYPTO_get_ex_new_index(5, 0, nil, x509_store_new_func, nil, nil)
end

-- Callback function hooked up into TLS Handshake to verify CRL
-- Passed as parameter verify_callback to openssl API SSL_CTX_set_verify for openssl
-- Signature of verify_callback:
--     int verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);
local function verify_callback(preverify_ok, x509_ctx)
  local verify_ret = 0
  if preverify_ok == 0 then
    return verify_ret
  end

  current_chain_index = current_chain_index + 1

  -- write certificate to DER file
  local cert = lssl.X509_STORE_CTX_get_current_cert(x509_ctx)
  local out_file = io.open(tmp_dir..application_name.."_cert_"..tostring(current_chain_index)..".der", "w")
  if not out_file then
    return verify_ret
  end
  local write_file_ret = lssl.i2d_X509_fp(out_file, cert)
  out_file:close()
  if write_file_ret ~= 1 then
    return verify_ret
  end

  if current_chain_index == 1 and trust_root_ca then
    return 1
  end

  -- hash the certificate
  local digest = lssl.EVP_sha1()
  -- SHA1 digest length is 20
  local digest_buf = ffi.new("unsigned char[20]")
  local digest_length_p = ffi.new("size_t [1]", 0)
  digest_length_p[0] = 20
  local rc = lssl.X509_digest(cert, digest, digest_buf, digest_length_p);
  if rc ~= 1 then
    return verify_ret
  end
  local digest_hex = hex_encode(ffi.string(digest_buf, 20))

  -- check whether the certificate has already been checked within CRL cache time
  local verified_ts_rdb = "crl_check.app."..application_name.."."..digest_hex..".verified_ts"
  local verified_status_rdb = "crl_check.app."..application_name.."."..digest_hex..".verified_status"
  local verified_ts = tonumber(luardb.get(verified_ts_rdb)) or 0
  local verified_status = tonumber(luardb.get(verified_status_rdb))
  local crl_cache_time = tonumber(luardb.get("crl_check.settings.crl_cache_time")) or 23*60*60
  local current_ts = os.time()
  local diff_ts = math.abs(current_ts - verified_ts)
  if diff_ts > crl_cache_time then
    local ret
    local ret_f = io.popen("check_crl.sh "..application_name.." "..tostring(current_chain_index).." "..application_ca_path .. " "..crl_download_max_time_secs.." >/dev/null 2>&1; echo $?")
    if ret_f then
        ret = tonumber(ret_f:read("*l")) or INTERNAL_CRL_PROCESSING_ERROR
        ret_f:close()
    else
        ret = INTERNAL_CRL_PROCESSING_ERROR
    end

    if ret == 0 or ret == X509_V_ERR_CERT_REVOKED then
      -- verified or revoked certificate will be cached
      set_persist_rdb_if_changed(verified_ts_rdb, os.time())
    else
      -- failed to download CRL or internal error will not be cached
      set_persist_rdb_if_changed(verified_ts_rdb, 0)
    end

    set_persist_rdb_if_changed(verified_status_rdb, ret)

    if ret == 0 then
        verify_ret = 1
    else
        if ret == X509_V_ERR_CERT_REVOKED then
          lssl.X509_STORE_CTX_set_error(x509_ctx, X509_V_ERR_CERT_REVOKED)
        elseif ret == X509_V_ERR_UNABLE_TO_GET_CRL then
          lssl.X509_STORE_CTX_set_error(x509_ctx, X509_V_ERR_UNABLE_TO_GET_CRL)
        else
          -- return 0 without calling X509_STORE_CTX_set_error will prompt http client to alert server about internal error
        end
    end
  else
    -- cached
    if verified_status == 0 then
      verify_ret = 1
    elseif verified_status and verified_status ~= INTERNAL_CRL_PROCESSING_ERROR then
      lssl.X509_STORE_CTX_set_error(x509_ctx, verified_status)
    else
      -- return 0 without calling X509_STORE_CTX_set_error will prompt http client to alert server about internal error
    end
  end

  return verify_ret
end

return {
  init = init,
  verify_callback = verify_callback
}

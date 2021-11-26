/*
 * NetComm OMA-DM Client
 *
 * lua_util.c
 * Miscellaneous utility functions for use in Lua scripts.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _DEFAULT_SOURCE
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>

#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

#include "util.h"

#define VERSION         11LL
#define VERSION_SHIFT   54
#define VERSION_MASK    0xFFC0000000000000
#define MODE_SHIFT      52
#define MODE_MASK       0x0030000000000000
#define INITIATOR_SHIFT 51
#define INITIATOR_MASK  0x0008000000000000
#define SESSIONID_SHIFT 8
#define SESSIONID_MASK  0x0000000000FFFF00
#define LENGTH_MASK     0x00000000000000FF

/* util.ToHex(buf)
 *
 * Returns the hex encoding of the given buffer.
 */
static int __toHex(lua_State* L)
{
    size_t srcLen;
    const char* src = luaL_checklstring(L, 1, &srcLen);

    size_t dstLen = srcLen * 2;
    char* dst = malloc(dstLen);
    if (!dst) {
        luaL_error(L, "out of memory");
    }

    hex_encode(dst, src, srcLen);
    lua_pushlstring(L, dst, dstLen);
    free(dst);
    return 1;
}

/* util.FromHex(str)
 *
 * Decode a hex string.
 */
static int __fromHex(lua_State* L)
{
    size_t srcLen;
    const char* src = luaL_checklstring(L, 1, &srcLen);

    size_t dstLen = srcLen / 2;
    char* dst = malloc(dstLen);
    if (!dst) {
        luaL_error(L, "out of memory");
    }

    if (!hex_decode(dst, src, srcLen)) {
        free(dst);
        luaL_error(L, "invalid hex string");
    }

    lua_pushlstring(L, dst, dstLen);
    free(dst);
    return 1;
}

/* util.ToBase64(buf)
 *
 * Returns the base64 encoding of the given buffer.
 */
static int __toBase64(lua_State* L)
{
    size_t srcLen;
    const char* src = luaL_checklstring(L, 1, &srcLen);

    size_t dstLen = base64_encode_length(srcLen);
    char* dst = malloc(dstLen);
    if (!dst) {
        luaL_error(L, "out of memory");
    }

    lua_pushlstring(L, dst, base64_encode(dst, src, srcLen));
    free(dst);
    return 1;
}

/* util.FromBase64(str)
 *
 * Decode a base64 string.
 */
static int __fromBase64(lua_State* L)
{
    size_t srcLen;
    const char* src = luaL_checklstring(L, 1, &srcLen);

    size_t dstLen = base64_decode_length(srcLen);
    char* dst = malloc(dstLen);
    if (!dst) {
        luaL_error(L, "out of memory");
    }

    int finalLen = base64_decode(dst, src, srcLen);
    if (finalLen < 0) {
        free(dst);
        luaL_error(L, "invalid base64 string");
    }

    lua_pushlstring(L, dst, finalLen);
    free(dst);
    return 1;
}

/* util.CalculateMD5(buf)
 *
 * Returns the MD5 hash of the given buffer.
 */
static int __calculateMD5(lua_State* L)
{
    size_t srcLen;
    const char* src = luaL_checklstring(L, 1, &srcLen);

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(src, srcLen, digest);

    lua_pushlstring(L, digest, MD5_DIGEST_LENGTH);
    return 1;
}

/* util.CalculateHMAC_SHA1(key, buf)
 *
 * Returns the SHA1 HMAC for the given buffer and key.
 */
static int __calculateHMAC_SHA1(lua_State* L)
{
    size_t keyLen;
    size_t srcLen;
    const char* key = luaL_checklstring(L, 1, &keyLen);
    const char* src = luaL_checklstring(L, 2, &srcLen);

    unsigned int mdLen = SHA_DIGEST_LENGTH;
    unsigned char md[mdLen];
    HMAC(EVP_sha1(), key, keyLen, src, srcLen, md, &mdLen);

    lua_pushlstring(L, md, mdLen);
    return 1;
}

/* util.EncodePackage0(serverid, sessionid, initiator, mode)
 *
 * Encodes and returns the trigger portion of a package0 message.
 */
static int __encodePackage0(lua_State* L)
{
    size_t length;
    const char* serverid = luaL_checklstring(L, 1, &length);
    uint64_t sessionid = luaL_checkinteger(L, 2);
    uint64_t initiator = luaL_checkinteger(L, 3);
    uint64_t mode = luaL_checkinteger(L, 4);

    luaL_argcheck(L, length < 256,                        1, "server id too long");
    luaL_argcheck(L, sessionid >= 0 && sessionid < 65536, 2, "invalid sessionid");
    luaL_argcheck(L, initiator >= 0 && initiator < 2,     3, "invalid initiator");
    luaL_argcheck(L, mode >= 0 && mode < 4,               4, "invalid mode");

    uint64_t header = ((VERSION   << VERSION_SHIFT)   & VERSION_MASK)
                    | ((mode      << MODE_SHIFT)      & MODE_MASK)
                    | ((initiator << INITIATOR_SHIFT) & INITIATOR_MASK)
                    | ((sessionid << SESSIONID_SHIFT) & SESSIONID_MASK)
                    | ((length                      ) & LENGTH_MASK);

    header = htobe64(header);

    lua_pushlstring(L, (char*)&header, sizeof(uint64_t));
    lua_pushvalue(L, 1);
    lua_concat(L, 2);

    return 1;
}

/* util.DecodePackage0(buf)
 *
 * Decodes the trigger portion of a package0 message, returning
 * the serverid, sessionid, initiator and mode fields.
 */
static int __decodePackage0(lua_State* L)
{
    size_t pLength;
    const char* pkg = luaL_checklstring(L, 1, &pLength);

    luaL_argcheck(L, pLength >= sizeof(uint64_t), 1, "string too short to contain package0");

    uint64_t header = be64toh(*(uint64_t*)pkg);

    int version   = (header & VERSION_MASK)   >> VERSION_SHIFT;
    int mode      = (header & MODE_MASK)      >> MODE_SHIFT;
    int initiator = (header & INITIATOR_MASK) >> INITIATOR_SHIFT;
    int sessionid = (header & SESSIONID_MASK) >> SESSIONID_SHIFT;
    int length    = (header & LENGTH_MASK);

    luaL_argcheck(L, version == VERSION,                   1, "version mismatch");
    luaL_argcheck(L, length > 0,                           1, "no server id");
    luaL_argcheck(L, pLength >= sizeof(uint64_t) + length, 1, "length mismatch");

    lua_pushlstring(L, pkg + sizeof(uint64_t), length);
    lua_pushinteger(L, sessionid);
    lua_pushinteger(L, initiator);
    lua_pushinteger(L, mode);

    return 4;
}

void luaopen_util(lua_State* L)
{
    static const luaL_Reg funcs[] = {
        {"ToHex",              __toHex},
        {"FromHex",            __fromHex},
        {"ToBase64",           __toBase64},
        {"FromBase64",         __fromBase64},
        {"CalculateMD5",       __calculateMD5},
        {"CalculateHMAC_SHA1", __calculateHMAC_SHA1},
        {"EncodePackage0",     __encodePackage0},
        {"DecodePackage0",     __decodePackage0},
        {NULL, NULL}
    };

    luaL_register(L, "util", funcs);
    lua_pop(L, 1);
}

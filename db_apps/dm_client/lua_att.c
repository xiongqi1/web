/*
 * NetComm OMA-DM Client
 *
 * lua_att.c
 * AT&T-specific utility functions.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/md5.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>

#include "util.h"

#define BUFSIZE_STEP1  24
#define BUFSIZE_MD5    6
#define BUFSIZE_BASE36 10
#define BUFSIZE_STEP2  (BUFSIZE_MD5 + BUFSIZE_BASE36)

/* Client dictionary, used in step 1 when generating key for CLCRED. */
static const int dictClient[] = {
    0x0e, 0x06, 0x10, 0x0c, 0x0a, 0x0e, 0x05, 0x0c, 0x12, 0x0a, 0x0b, 0x06, 0x0d, 0x0e, 0x05
};

/* Server dictionary, used in step 1 when generating key for SRVCRED. */
static const int dictServer[] = {
    0x0a, 0x06, 0x0e, 0x0e, 0x0a, 0x0b, 0x06, 0x0e, 0x0b, 0x04, 0x04, 0x07, 0x11, 0x0c, 0x0c
};

/* Step 1: Generate an initial key by taking the IMEI and fiddling
 * with it until it looks sufficiently different to appease your boss.
 */
static int attpwd_step1(char* buf, int bufLen, const char* imei, int imeiLen, const int* dict)
{
    int sn1 = 0;
    int sn2 = 0;
    for (int i = 0; i < imeiLen - 3; i++) {
        sn1 += imei[i + 3] * dict[i];
        sn2 += imei[i + 3] * imei[i + 2] * dict[i];
    }
    return snprintf(buf, bufLen, "%i-%i", sn1, sn2);
}

/* Step 2a: Generate an intermediate password by taking an MD5 hash
 * and then discarding the majority of it for some reason.
 */
static void attpwd_step2_md5(
    char* buf,
    const char* key,  int keyLen,
    const char* imei, int imeiLen,
    const char* name, int nameLen)
{
    const char* toHex = "0123456789abcdef";

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, imei, imeiLen);
    MD5_Update(&ctx, key,  keyLen);
    MD5_Update(&ctx, name, nameLen);

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    buf[0] = toHex[digest[1] & 0x0F];
    buf[1] = toHex[digest[3] >> 4];
    buf[2] = toHex[digest[4] & 0x0F];
    buf[3] = toHex[digest[6] & 0x0F];
    buf[4] = toHex[digest[12] >> 4];
    buf[5] = toHex[digest[15] & 0x0F];
}

/* Step 2b: Convert the IMEI into base-36, the only numbering system
 * clinically proven to increase entropy, and then tape the result
 * to the tattered remains of your MD5 hash.
 */
static void attpwd_step2_base36(char* buf, const char* imei)
{
    const char* toBase36 = "0123456789abcdefghijklmnopqrstuvwxyz";

    unsigned long long num = strtoull(imei, NULL, 10);
    char* cur = buf + BUFSIZE_BASE36 - 1;
    while (cur >= buf) {
        *(cur--) = toBase36[num % 36];
        num /= 36;
    }
}

/* Step 3: Shuffle the intermediate password, because why not?
 *
 * Note: The shuffle step as described in the AT&T spec is applied three
 * times; the mapping used here collapses those three steps into one.
 */
static void attpwd_step3(char* buf, const char* passwd)
{
    static const int shuffle[] = {3, 7, 11, 15, 12, 8, 4, 0, 2, 6, 10, 14, 13, 9, 5, 1};

    for (int i = 0; i < BUFSIZE_STEP2; i++) {
        buf[i] = passwd[shuffle[i]];
    }
}

/* att.GeneratePassword(imei, name, useClientDict)
 *
 * The AT&T Password Generation Algorithm (TM), in all its glory.
 */
static int __generate_password(lua_State* L)
{
    size_t imeiLen;
    size_t nameLen;
    const char* imei = luaL_checklstring(L, 1, &imeiLen);
    const char* name = luaL_checklstring(L, 2, &nameLen);
    const int*  dict = lua_toboolean(L, 3) ? dictClient : dictServer;

    char buf1[BUFSIZE_STEP1];
    char buf2[BUFSIZE_STEP2];

    int bufLen1 = attpwd_step1(buf1, sizeof(buf1), imei, imeiLen, dict);
    attpwd_step2_md5(buf2, buf1, bufLen1, imei, imeiLen, name, nameLen);
    attpwd_step2_base36(buf2 + BUFSIZE_MD5, imei);
    attpwd_step3(buf1, buf2);

    lua_pushlstring(L, buf1, BUFSIZE_STEP2);
    return 1;
}

/* att.GenerateBootstrapKey(imsi)
 *
 * Append some stuff to the IMSI to give it an even number of characters,
 * flip each pair of characters, then decode it as a hex string.
 *
 * I hope whoever designed this feels an appropriate amount of shame.
 */
static int __generate_boostrap_key(lua_State* L)
{
    size_t imsiLen;
    const char* imsi = luaL_checklstring(L, 1, &imsiLen);

    int hexLen;
    char hex[imsiLen + 3];

    if (imsiLen % 2) {
        hexLen = snprintf(hex, sizeof(hex), "9%.*s", imsiLen, imsi);
    } else {
        hexLen = snprintf(hex, sizeof(hex), "1%.*sF", imsiLen, imsi);
    }

    for (int h = 0; h < hexLen; h += 2) {
        char t = hex[h];
        hex[h] = hex[h+1];
        hex[h+1] = t;
    }

    char key[hexLen / 2];
    if (!hex_decode(key, hex, hexLen)) {
        luaL_error(L, "invalid IMSI");
    }
    lua_pushlstring(L, key, sizeof(key));

    return 1;
}

void luaopen_att(lua_State* L)
{
    const luaL_Reg funcs[] = {
        {"GeneratePassword",     __generate_password},
        {"GenerateBootstrapKey", __generate_boostrap_key},
        {NULL, NULL}
    };

    luaL_register(L, "att", funcs);
    lua_pop(L, 1);
}

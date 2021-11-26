/*
 * web client helper for ODU installer services
 *
 * Copyright (C) 2021 Casa Systems
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CASA SYSTEMS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <rdb_ops.h>
#include <libsnextra.h>

#define MODNAME "lua_odu_web_client_helper"

#define SERVER_RDB "service.authenticate.server"
#define PORT_RDB "service.authenticate.port"
#define SERVER_CA_RDB "service.authenticate.cafile"
#define CLIENT_CERT_RDB "service.authenticate.clientcrtfile"
#define CLIENT_KEY_RDB "service.authenticate.clientkeyfile"

#define BUFSZ 512
#define DEF_KEYLEN 10
#if (DEF_KEYLEN * 2 + 1 > BUFSZ)
    #error BUFSZ is too small
#endif

/*
 * Helper function to push return values to Lua virtual stack
 * First pushed argument int value used as boolean indicate success (1) or error (0).
 * Second pushed argument is string providing additional data.
 * @param L Lua state variable
 * @param bool_ret int value used as boolean indicate success or error
 * @param data additional data
 * @param data_length data length
 * @return number of pushed results
 */
static int push_return_values(lua_State *L, int bool_ret, const char *data, int data_length) {
    int ret_num = 0;
    lua_pushboolean(L, bool_ret);
    ret_num++;
    if (data) {
        lua_pushlstring(L, data, data_length);
        ret_num++;
    }
    return ret_num;
}

/*
 * Helper function to report errors to Lua
 * It pushes 0 to indicate error, as first argument, and additional error string.
 * @param L Lua state variable
 * @param error_string Error string
 * @return number of pushed results
 */
static int push_error(lua_State *L, const char *error_string) {
    return push_return_values(L, 0, error_string, strlen(error_string));
}

// for response content buffer
// to store data written by write callback function of CURLOPT_WRITEFUNCTION
struct resp_content_t {
    char *buf;
    size_t size;
};

// write callback function of CURLOPT_WRITEFUNCTION
static size_t response_content_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t real_size = size * nmemb;
    struct resp_content_t *content_mem = (struct resp_content_t *)userp;
    char *ptr = realloc(content_mem->buf, content_mem->size + real_size + 1);
    if (!ptr) {
        return 0;
    }

    content_mem->buf = ptr;
    memcpy(&(content_mem->buf[content_mem->size]), contents, real_size);
    content_mem->size += real_size;

    return real_size;
}

// try to print message into existing buffer "error" that has size "error_size"
// error_size should be > 0
#define PRINT_ERROR(format, ...) do { \
        int sp_err_ret = snprintf(error, error_size, format, ##__VA_ARGS__); \
        if (sp_err_ret < 0 || sp_err_ret >= error_size) { \
            error[0] = '\0'; \
        } \
    } while(0)

/*
 * initialise web client
 * @param curl_handle CURL handle
 * @param endpoint End-point (i.e the component of URL after https://IP:PORT/)
 * @param resp_content for response content
 * @param error error buffer
 * @param error_size error buffer length
 * @return 0 on success; error otherwise
 */
static int init_client(CURL **curl_handle, const char *endpoint,
        struct resp_content_t *resp_content,
        char *error, int error_size)
{
    char buf[BUFSZ];
    struct rdb_session *rdb_s;
    int ret;

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        PRINT_ERROR("rdb_open failed");
        return -1;
    }

#define PRINT_ERROR_CLOSE_RDB(format, ...) do { \
        PRINT_ERROR(format, ##__VA_ARGS__); \
        rdb_close(&rdb_s); \
    } while(0)

#define SET_CURLOPT_CLOSE_RDB(opt, val) do { \
        CURLcode res = curl_easy_setopt(*curl_handle, opt, val); \
        if (res != CURLE_OK) { \
            PRINT_ERROR_CLOSE_RDB("curl_easy_setopt with %s failed (%d)", #opt, res); \
            return -1; \
        } \
    } while(0)

#define SET_CURLOPT_FROM_RDB(opt, rdbk) do { \
        int ret = rdb_get_string(rdb_s, rdbk, buf, BUFSZ); \
        if (ret) { \
            PRINT_ERROR_CLOSE_RDB("rdb_get_string(%s) failed (%d)", rdbk, ret); \
            return ret; \
        } \
        SET_CURLOPT_CLOSE_RDB(opt, buf); \
    } while(0)

    curl_global_init(CURL_GLOBAL_ALL);
    *curl_handle = curl_easy_init();
    if (*curl_handle == NULL) {
        PRINT_ERROR_CLOSE_RDB("curl_easy_init failed");
        return -1;
    }

    SET_CURLOPT_CLOSE_RDB(CURLOPT_USERAGENT, "ODU installer service client");
    SET_CURLOPT_FROM_RDB(CURLOPT_CAINFO, SERVER_CA_RDB);
    SET_CURLOPT_FROM_RDB(CURLOPT_SSLCERT, CLIENT_CERT_RDB);
    SET_CURLOPT_FROM_RDB(CURLOPT_SSLKEY, CLIENT_KEY_RDB);

    ret = generate_kdf_key("da", -1, buf, DEF_KEYLEN * 2);
    if (ret) {
        PRINT_ERROR_CLOSE_RDB("generate_kdf_key failed");
        return ret;
    }
    buf[DEF_KEYLEN * 2] = '\0';
    SET_CURLOPT_CLOSE_RDB(CURLOPT_KEYPASSWD, buf);

    // do not verify host name in CN or SAN
    SET_CURLOPT_CLOSE_RDB(CURLOPT_SSL_VERIFYHOST, 0L);
    // connection should be quick
    SET_CURLOPT_CLOSE_RDB(CURLOPT_CONNECTTIMEOUT, 3L);

    {
        char server_val[BUFSZ];
        int port;
        if (!rdb_get_string(rdb_s, SERVER_RDB, server_val, BUFSZ) && !rdb_get_int(rdb_s, PORT_RDB, &port)) {
            int sp_ret = snprintf(buf, BUFSZ, "https://%s:%d/%s", server_val, port, endpoint);
            if (sp_ret < 0 || sp_ret >= BUFSZ) {
                PRINT_ERROR_CLOSE_RDB("failed to build url");
                return -1;
            }
            SET_CURLOPT_CLOSE_RDB(CURLOPT_URL, buf);
        } else {
            PRINT_ERROR_CLOSE_RDB("failed to get server IP address and/or port");
            return -1;
        }
    }
    SET_CURLOPT_CLOSE_RDB(CURLOPT_WRITEFUNCTION, response_content_cb);
    SET_CURLOPT_CLOSE_RDB(CURLOPT_WRITEDATA, (void *)resp_content);

    rdb_close(&rdb_s);

    return 0;
}

/*
 * de-initialise web client
 * @param curl_handle CURL handle
 * @param resp_content for response content
 */
static void deinit_client(CURL **curl_handle, struct resp_content_t *resp_content)
{
    if (*curl_handle) {
        curl_easy_cleanup(*curl_handle);
        *curl_handle = NULL;
    }
    free(resp_content->buf);

    curl_global_cleanup();
}

#define CALL_CURL_FUNC_RETURN_LUA(func, ...) do { \
            int c_ret = func(__VA_ARGS__); \
            if (c_ret) { \
                PRINT_ERROR("%s failed (%d)", #func, c_ret); \
                deinit_client(&curl_handle, &resp_content); \
                return push_error(L, error); \
            } \
        } while(0)
/*
 * do GET
 * Index of Lua input arguments in virtual stack
 *     1: end-point (i.e the component of URL after https://IP:PORT/)
 * Return in Lua:
 *     1: boolean (true: success; false: failed)
 *     2: response code if success; error if failed
 *     3: response body content if success
 *
 * @param L Lua state variable
 * @return number of pushed results to Lua virtual stack
 */
static int lua_do_get(lua_State *L) {
    char error[BUFSZ];
    int error_size = BUFSZ;
    struct resp_content_t resp_content;
    CURL *curl_handle;
    int ret;
    long response_code;
    size_t endpoint_length;
    const char *endpoint = luaL_checklstring (L, 1, &endpoint_length);

    if (!endpoint || endpoint_length == 0) {
        return luaL_error(L, "Invalid parameters");
    }

    resp_content.buf = NULL;
    resp_content.size = 0;

    ret = init_client(&curl_handle, endpoint, &resp_content, error, error_size);
    if (ret) {
        deinit_client(&curl_handle, &resp_content);
        return push_error(L, error);
    }

    CALL_CURL_FUNC_RETURN_LUA(curl_easy_perform, curl_handle);
    CALL_CURL_FUNC_RETURN_LUA(curl_easy_getinfo, curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    lua_pushboolean(L, 1);
    lua_pushnumber(L, (double)response_code);
    lua_pushlstring(L, resp_content.buf, resp_content.size);

    deinit_client(&curl_handle, &resp_content);

    return 3;
}

/*
 * do PUT file
 * Index of Lua input arguments in virtual stack
 *     1: end-point (i.e the component of URL after https://IP:PORT/)
 *     2: file path to be uploaded
 * Return in Lua:
 *     1: boolean (true: success; false: failed)
 *     2: response code if success; error if failed
 *     3: response body content if success
 *
 * @param L Lua state variable
 * @return number of pushed results to Lua virtual stack
 */
static int lua_do_put_file(lua_State *L) {
    char error[BUFSZ];
    int error_size = BUFSZ;
    struct resp_content_t resp_content;
    CURL *curl_handle;
    int ret;
    long response_code;
    size_t endpoint_length;
    const char *endpoint = luaL_checklstring (L, 1, &endpoint_length);
    size_t file_path_length;
    const char *file_path = luaL_checklstring (L, 2, &file_path_length);
    curl_mime *mime;
    curl_mimepart *part_name, *part_file;
    char *file_name;

    if (!endpoint || endpoint_length == 0 || !file_path || file_path_length == 0) {
        return luaL_error(L, "Invalid parameters");
    }

    resp_content.buf = NULL;
    resp_content.size = 0;

    ret = init_client(&curl_handle, endpoint, &resp_content, error, error_size);
    if (ret) {
        deinit_client(&curl_handle, &resp_content);
        return push_error(L, error);
    }

    mime = curl_mime_init(curl_handle);
    if (!mime) {
        PRINT_ERROR("curl_mime_init failed");
        deinit_client(&curl_handle, &resp_content);
        return push_error(L, error);
    }

#define PRINT_ERROR_RETURN_MIME_LUA(format, ...) do { \
        PRINT_ERROR(format, ##__VA_ARGS__); \
        curl_mime_free(mime); \
        deinit_client(&curl_handle, &resp_content); \
        return push_error(L, error); \
    } while(0)

#define CALL_CURL_FUNC_RETURN_MIME_LUA(func, ...) do { \
        int c_ret = func(__VA_ARGS__); \
        if (c_ret) { \
            PRINT_ERROR("%s failed (%d)", #func, c_ret); \
            curl_mime_free(mime); \
            deinit_client(&curl_handle, &resp_content); \
            return push_error(L, error); \
        } \
    } while(0)

    part_name = curl_mime_addpart(mime);
    part_file = curl_mime_addpart(mime);
    if (!part_name || !part_file) {
        PRINT_ERROR_RETURN_MIME_LUA("curl_mime_addpart failed");
    }

    file_name = strrchr(file_path, '/');

    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_mime_name, part_name, "name");
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_mime_data, part_name, file_name?file_name+1:file_path, CURL_ZERO_TERMINATED);
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_mime_name, part_file, "file");
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_mime_filedata, part_file, file_path);
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_easy_setopt, curl_handle, CURLOPT_MIMEPOST, mime);
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_easy_setopt, curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_easy_perform, curl_handle);
    CALL_CURL_FUNC_RETURN_MIME_LUA(curl_easy_getinfo, curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    lua_pushboolean(L, 1);
    lua_pushnumber(L, (double)response_code);
    lua_pushlstring(L, resp_content.buf, resp_content.size);

    curl_mime_free(mime);
    deinit_client(&curl_handle, &resp_content);

    return 3;
}

/**
 * Called by Lua to open the C library. Registers functions for Lua.
 *
 * @param L Lua state variable.
 */
int luaopen_lua_odu_web_client_helper(lua_State *L) {
    const struct luaL_reg lua_funcs[] = {
        { "do_get", lua_do_get },
        { "do_put_file", lua_do_put_file},
        { NULL, NULL }
    };
    luaL_register(L, MODNAME, lua_funcs);

    return 1;
}

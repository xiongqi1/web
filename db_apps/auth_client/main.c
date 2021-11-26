/*
 * DA authenticate client daemon
 *
 * This daemon acts as a DA authentication client that connects to the
 * authentication server via libcurl.
 * Meanwhile it provides an RDB RPC service to other modules of the
 * system, so that they do not need to deal with https/curl directly.
 *
 * The RDB RPC service detail is as follows.
 * Service point: service.authenticate
 * Command: activate
 * Parameters:
 *   service: name of the service to be activated (rdb|upgrade|...)
 *   enable: 1 - enable/activate or 0 - disable/deactivate the service
 *
 * Command: put
 * Parameters:
 *   endpoint:  name of the REST endpoint to put the data to
 *   value:  the string value to put to the REST endpoint
 *           (e.g. Status=OK&Voltage=3.14&Level=0.42)
 *
 * Command: get
 * Parameters:
 *   endpoint:  name of the REST endpoint to get.  The result is returned in the invoke buffer
 *
 * Upon daemon start, it automatically tries to activate rdb service first. Only
 * after rdb service is activated, it starts serving RDB RPC. Otherwise, the
 * daemon will quit.
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
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
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <curl/curl.h>
#include <rdb_ops.h>
#include <rdb_rpc_server.h>
#include <libsnextra.h>
#include <json/json.h>
#include "utils.h"

#define SERVER_RDB "service.authenticate.server"
#define PORT_RDB "service.authenticate.port"
#define SERVER_CA_RDB "service.authenticate.cafile"
#define CLIENT_CERT_RDB "service.authenticate.clientcrtfile"
#define CLIENT_KEY_RDB "service.authenticate.clientkeyfile"

#define RDB_BRIDGE_ENABLE "service.rdb_bridge.enable"
#define HANDSHAKE_STATE "handshake.state"
#define OWA_MODEL_RDB "owa.system.product"
#define OWA_CAP_RDB "owa.system.capabilities"

#define RPC_SERVICE_NAME "service.authenticate"
#define RPC_CMD "activate"
#define RPC_PARAMS { "service", "enable" }

#define RPC_PUT "put"
#define RPC_PUT_PARAMS { "endpoint", "value" }

#define RPC_GET "get"
#define RPC_GET_PARAMS { "endpoint" }

#define BUFSZ 512
#define DEF_KEYLEN 10
#if (DEF_KEYLEN * 2 + 1 > BUFSZ)
    #error BUFSZ is too small
#endif

// max number of retries to activate rdb service
#define MAX_RETRIES 60
// retry interval when activating rdb service
#define RETRY_WAIT_SECS 2

static struct rdb_session * rdb_s;
static rdb_rpc_server_session_t * rpc_s;

static CURL * curl;

volatile static sig_atomic_t terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

/*
 * build DA authenticate HTTPS request URL
 *
 * @param buf A buffer to store the result URL
 * @param bufsz The size of the buffer
 * @param endpoint The pre-built endpoint string
 * @return 0 on success; negative error code on failure
 */
static int build_url(char * buf, int bufsz, const char * endpoint)
{
    int ret;
    int port;
    ret = snprintf(buf, bufsz, "https://");
    if (ret < 0) {
        return ret;
    } else if (ret >= bufsz) {
        return -EOVERFLOW;
    }
    buf += ret;
    bufsz -= ret;

    ret = rdb_get_string(rdb_s, SERVER_RDB, buf, bufsz);
    if (ret) {
        return ret;
    }
    ret = strlen(buf);
    buf += ret;
    bufsz -= ret;

    *(buf++) = ':';
    bufsz--;

    rdb_get_int(rdb_s, PORT_RDB, &port);
    ret = snprintf(buf, bufsz, "%d", port);
    if (ret < 0) {
        return ret;
    } else if (ret >= bufsz) {
        return -EOVERFLOW;
    }
    buf += ret;
    bufsz -= ret;

    ret = snprintf(buf, bufsz, "%s", endpoint);
    if (ret < 0) {
        return ret;
    } else if (ret >= bufsz) {
        return -EOVERFLOW;
    }

    return 0;
}

/*
 * Set an curl option and check result
 *
 * @param opt The curl option to be set
 * @param val The value of the option
 * @return A negative error code on failure. Fall through on success
 */
#define SET_CURLOPT_CHK(opt, val) do { \
        CURLcode res = curl_easy_setopt(curl, opt, val); \
        if (res) { \
            BLOG_ERR("Failed to set %s (%d): %s\n", #opt, res, curl_easy_strerror(res)); \
            return -res; \
        } \
    } while(0)

/*
 * Set an curl option from an RDB variable and check result
 *
 * @param rdbk The RDB variable containing the value of the option
 * @param opt The curl option to be set
 * @return A negative error code on failure. Fall through on success
 */
#define SET_CURLOPT_FROM_RDB(rdbk, opt) do { \
        int ret = rdb_get_string(rdb_s, rdbk, buf, BUFSZ); \
        if (ret) { \
            BLOG_ERR("Failed to get RDB %s (%d)\n", rdbk, ret); \
            return ret; \
        } \
        SET_CURLOPT_CHK(opt, buf); \
    } while(0)

/*
 * Configure curl options for hello/activate/RESTAPI
 *
 * @param endpoint  The pre-built endpoint string
 * @param http_put  1 if this is a http PUT request (else GET)
 * @return 0 on success; negative error code on failure
 */
static int config_curl(const char * endpoint, int http_put)
{
    char buf[BUFSZ];
    int ret;

    // build and set URL
    ret = build_url(buf, BUFSZ, endpoint);
    if (ret) {
        BLOG_ERR("Failed to build url: %d\n", ret);
        return ret;
    }
    BLOG_INFO("URL=%s\n", buf);
    SET_CURLOPT_CHK(CURLOPT_URL, buf);

    // set server root CA cert
    SET_CURLOPT_FROM_RDB(SERVER_CA_RDB, CURLOPT_CAINFO);

    // set client key
    SET_CURLOPT_FROM_RDB(CLIENT_KEY_RDB, CURLOPT_SSLKEY);

    // generate & set client key passphrase
    ret = generate_kdf_key("da", -1, buf, DEF_KEYLEN * 2);
    if (ret) {
        BLOG_ERR("Failed to generate key\n");
        return ret;
    }
    buf[DEF_KEYLEN * 2] = '\0';
    // it does no harm to set passphrase on a key that is not encrypted
    SET_CURLOPT_CHK(CURLOPT_KEYPASSWD, buf);

    // set client certificate (chained up to/excl root CA)
    SET_CURLOPT_FROM_RDB(CLIENT_CERT_RDB, CURLOPT_SSLCERT);

    // use HTTP PUT/GET method
    SET_CURLOPT_CHK(CURLOPT_UPLOAD, http_put);

#ifdef SKIP_HOSTNAME_VERIFICATION
    // do not verify host name in CN or SAN
    SET_CURLOPT_CHK(CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    // connection between NIT and OWA should be quick
    SET_CURLOPT_CHK(CURLOPT_CONNECTTIMEOUT, 2L);

    return 0;
}

// all the following sizes include terminating zero
#define HELLO_LONG_FIELD_SZ 256
#define HELLO_SHORT_FIELD_SZ 32
#define HELLO_OUI_SZ 7
#define HELLO_UPTIME_SZ 10
#define HELLO_MAC_SZ 18
struct hello_resp_t {
    char manuf[HELLO_LONG_FIELD_SZ];
    char oui[HELLO_OUI_SZ];
    char model[HELLO_SHORT_FIELD_SZ];
    char desc[HELLO_LONG_FIELD_SZ];
    char class[HELLO_LONG_FIELD_SZ];
    char sn[HELLO_SHORT_FIELD_SZ];
    char hwver[HELLO_SHORT_FIELD_SZ];
    char swver[HELLO_SHORT_FIELD_SZ];
    char swdate[HELLO_SHORT_FIELD_SZ];
    char uptime[HELLO_UPTIME_SZ];
    char mac[HELLO_MAC_SZ];
    char cap[HELLO_LONG_FIELD_SZ];
};

/*
 * parse a hello response packet
 *
 * @param resp A hello response (json format) packet as a string
 * @param result A pointer to a hello_resp_t struct to hold the parsed result
 * @return 0 on success; -1 on failure
 */
static int parse_hello(const char * resp, struct hello_resp_t * result)
{
    json_object * jobj = json_tokener_parse(resp);
    if (!jobj) {
        BLOG_ERR("Bad json string: %s", resp);
        return -1;
    }
    memset(result, 0, sizeof(*result));
    json_object_object_foreach(jobj, key, val) {
        if (!json_object_is_type(val, json_type_string)) {
            BLOG_WARNING("%s is not a string, skipped", key);
            continue;
        }

        const char * sval = json_object_get_string(val);
        if (!sval) {
            BLOG_ERR("Failed to get %s", key);
            continue;
        }
        if (!strcmp(key, "Manufacturer")) {
            strncpy(result->manuf, sval, sizeof(result->manuf) - 1);
        } else if (!strcmp(key, "ManufacturerOUI")) {
            strncpy(result->oui, sval, sizeof(result->oui) - 1);
        } else if (!strcmp(key, "ModelName")) {
            strncpy(result->model, sval, sizeof(result->model) - 1);
        } else if (!strcmp(key, "Description")) {
            strncpy(result->desc, sval, sizeof(result->desc) - 1);
        } else if (!strcmp(key, "ProductClass")) {
            strncpy(result->class, sval, sizeof(result->class) - 1);
        } else if (!strcmp(key, "SerialNumber")) {
            strncpy(result->sn, sval, sizeof(result->sn) - 1);
        } else if (!strcmp(key, "HardwareVersion")) {
            strncpy(result->hwver, sval, sizeof(result->hwver) - 1);
        } else if (!strcmp(key, "SoftwareVersion")) {
            strncpy(result->swver, sval, sizeof(result->swver) - 1);
        } else if (!strcmp(key, "SoftwareVersionBuildDate")) {
            strncpy(result->swdate, sval, sizeof(result->swdate) - 1);
        } else if (!strcmp(key, "UpTime")) {
            strncpy(result->uptime, sval, sizeof(result->uptime) - 1);
        } else if (!strcmp(key, "MacAddress")) {
            strncpy(result->mac, sval, sizeof(result->mac) - 1);
        } else if (!strcmp(key, "Capabilities")) {
            strncpy(result->cap, sval, sizeof(result->cap) - 1);
        } else {
            BLOG_WARNING("%s is unknown, skipped", key);
            continue;
        }
    }
    json_object_put(jobj);
    return 0;
}

// a struct to be used as a buffer while receiving a GET response via libcurl
#define MAX_GET_RESP_SZ 4096
struct get_buf_t {
    char data[MAX_GET_RESP_SZ];
    int pos;
};

/*
 * libcurl call back for GET response
 *
 * @param ptr A pointer to the received data
 * @param size This is always 1
 * @param nmemb The size of the received data
 * @param userdata A pointer to get_buf_t struct where received data is appended to
 * @return The number of bytes actually consumed.
 */
static size_t get_cb(char * ptr, size_t size, size_t nmemb, void * userdata)
{
    struct get_buf_t * buf = (struct get_buf_t *) userdata;
    if (nmemb == 0) {
        return 0;
    }
    if (sizeof(buf->data) - buf->pos <= nmemb) {
        BLOG_ERR("Hello response is too long\n");
        return 0;
    }
    memcpy(buf->data + buf->pos, ptr, nmemb);
    buf->pos += nmemb;
    buf->data[buf->pos] = '\0';
    return nmemb;
}

/*
 * Config curl and perform an http GET operation on the given endpoint
 *
 * @param endpoint  The REST api endpoint to GET
 * @param buf  A buffer to store the results of the GET
 * @return 0 for success or error code
 */
static int http_get(char const *endpoint, struct get_buf_t *buf)
{
    int ret;
    CURLcode res;
    long resp_code;

    buf->data[0] = '\0';
    buf->pos = 0;

    ret = config_curl(endpoint, 0);
    if (ret) {
        BLOG_ERR("Failed to config url: %d\n", ret);
        return ret;
    }
    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    if (res) {
        BLOG_ERR("Failed to hook get_buf (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_cb);
    if (res) {
        BLOG_ERR("Failed to hook get_cb (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }

    res = curl_easy_perform(curl);
    if (res) {
        BLOG_ERR("curl_easy_perform() failed (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }
    res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code);
    if (res) {
        BLOG_ERR("Failed to get response code (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }
    BLOG_INFO("response: %ld\n", resp_code);
    if (resp_code != 200L) {
        BLOG_ERR("HTTP response failed (%ld)\n", resp_code);
        return -1;
    }

    return 0;
}

/*
 * get the OWA info including model and capabilities by sending a Hello message
 *
 * @return A const pointer to the struct holding the result, or NULL on failure
 */
static const struct hello_resp_t * get_owa_info(void)
{
    struct get_buf_t buf;
    static struct hello_resp_t result;

    buf.data[0] = '\0';
    buf.pos = 0;

    if (http_get("/api/v1/Hello", &buf)) {
        return NULL;
    }

    if (parse_hello(buf.data, &result)) {
        return NULL;
    }

    return &result;
}

/*
 * Config curl and perform an http PUT operation on the given endpoint
 *
 * @param endpoint  The REST api endpoint to PUT (including parameters)
 * @return 0 for success or error code
 */
static int http_put(char const *endpoint)
{
    int ret;
    CURLcode res;
    long resp_code;

    ret = config_curl(endpoint, 1);
    if (ret) {
        BLOG_ERR("Failed to config curl: %d\n", ret);
        return ret;
    }

    res = curl_easy_perform(curl);
    if (res) {
        BLOG_ERR("curl_easy_perform() failed (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }

    res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code);
    if (res) {
        BLOG_ERR("Failed to get response code (%d): %s\n", res, curl_easy_strerror(res));
        return -res;
    }

    BLOG_INFO("response: %ld\n", resp_code);
    if (resp_code != 200L) {
        BLOG_ERR("HTTP response failed (%ld)\n", resp_code);
        return -1;
    }

    return 0;
}

/*
 * Activate/deactivate a given service
 * @param service A string for the service name to be activated
 * @param enable 1 - enable/activate the service; 0 - disable/deactivate the service
 * @return 0 on success; negative error code on failure
 */
static int activate(const char * service, int enable)
{
    char buf[BUFSZ];

    int ret = snprintf(buf, BUFSZ,
		       "/api/v1/Activate?SecurityKey=OWA&service=%s&enable=%d",
		       service, enable);
    if (ret < 0 || ret >= BUFSZ) {
        BLOG_ERR("service string too long: %s\n", service);
        return -EOVERFLOW;
    }

    return http_put(buf);
}

/* Set the result buffer and length with given formatted string */
#define SET_RESULT(...) do { \
        if (result && result_len) { \
            int ret = snprintf(result, *result_len, __VA_ARGS__); \
            if (ret < 0) { \
                BLOG_ERR("Failed to write result\n"); \
                *result_len = 0; \
            } else if (ret >= *result_len) { \
                BLOG_ERR("Result buffer is too small\n"); \
            } else { \
                *result_len = ret + 1; \
            } \
        } \
    } while (0)

/*
 * RDB RPC command handler
 *
 * @param cmd The name of the command to be handled
 * @param params An array of RDB RPC parameters
 * @param params_len The number of parameters
 * @param result A buffer for a command result
 * @param result_len On input this gives the result buffer size. On output it
 * gives the length actually written into result (including terminating null if any)
 * @return 0 on success; a negative error code on failure
 */
static int cmd_handler(char *cmd, rdb_rpc_cmd_param_t params[], int params_len, char *result, int *result_len)
{
    int ix;
    const char *service = NULL;
    int enable = -1;
    int ret;

    BLOG_DEBUG("Handling command %s\n", cmd);
    assert(!strcmp(cmd, RPC_CMD));

    BLOG_DEBUG("  Number of params: %d\n", params_len);
    for (ix = 0; ix < params_len; ix++) {
        BLOG_DEBUG("    Param %d: %s=%s\n", ix, params[ix].name, params[ix].value);
        if (!strcmp(params[ix].name, "service")) {
            service = params[ix].value;
        } else if (!strcmp(params[ix].name, "enable")) {
            enable = (int)strtol(params[ix].value, NULL, 10);
        }
    }

    if (!service || (enable != 0 && enable != 1)) {
        BLOG_ERR("Bad rpc parameters\n");
        SET_RESULT("Bad parameters");
        return -1;
    }

    ret = activate(service, enable);
    if (ret) {
        SET_RESULT("Failed to activate");
        return ret;
    }

    SET_RESULT("Succeeded");
    return 0;
}

/*
 * RDB RPC PUT command handler
 *
 * @param cmd The name of the command to be handled
 * @param params An array of RDB RPC parameters
 * @param params_len The number of parameters
 * @param result A buffer for a command result
 * @param result_len On input this gives the result buffer size. On output it
 * gives the length actually written into result (including terminating null if any)
 * @return 0 on success; a negative error code on failure
 */
static int put_handler(char *cmd, rdb_rpc_cmd_param_t params[], int params_len, char *result, int *result_len)
{
    int ix;
    const char *endpoint = NULL;
    const char *value = NULL;
    char buf[BUFSZ];
    int ret;

    BLOG_DEBUG("Handling command %s\n", cmd);
    assert(!strcmp(cmd, RPC_PUT));

    BLOG_DEBUG("  Number of params: %d\n", params_len);
    for (ix = 0; ix < params_len; ix++) {
        BLOG_DEBUG("    Param %d: %s=%s\n", ix, params[ix].name, params[ix].value);
        if (!strcmp(params[ix].name, "endpoint")) {
            endpoint = params[ix].value;
        } else if (!strcmp(params[ix].name, "value")) {
            value = params[ix].value;
        }
    }

    if (!endpoint || !value) {
        BLOG_ERR("Bad rpc parameters\n");
        SET_RESULT("Bad parameters");
        return -1;
    }

    ret = snprintf(buf, BUFSZ, "%s?SecurityKey=OWA&%s", endpoint, value);
    if (ret < 0 || ret >= BUFSZ) {
        SET_RESULT("Failed to PUT");
        return -1;
    }

    ret = http_put(buf);
    if (ret) {
        SET_RESULT("Failed to PUT");
        return ret;
    }

    SET_RESULT("Succeeded");
    return 0;
}

/*
 * RDB RPC GET command handler
 *
 * @param cmd The name of the command to be handled
 * @param params An array of RDB RPC parameters
 * @param params_len The number of parameters
 * @param result A buffer for a command result
 * @param result_len On input this gives the result buffer size. On output it
 * gives the length actually written into result (including terminating null if any)
 * @return 0 on success; a negative error code on failure
 */
static int get_handler(char *cmd, rdb_rpc_cmd_param_t params[], int params_len, char *result, int *result_len)
{
    int ix;
    const char *endpoint = NULL;
    int ret;
    struct get_buf_t buf;

    BLOG_DEBUG("Handling command %s\n", cmd);
    assert(!strcmp(cmd, RPC_GET));

    BLOG_DEBUG("  Number of params: %d\n", params_len);
    for (ix = 0; ix < params_len; ix++) {
        BLOG_DEBUG("    Param %d: %s=%s\n", ix, params[ix].name, params[ix].value);
        if (!strcmp(params[ix].name, "endpoint")) {
            endpoint = params[ix].value;
        }
    }

    if (!endpoint) {
        BLOG_ERR("Bad rpc parameters\n");
        SET_RESULT("Bad parameters");
        return -1;
    }

    buf.data[0] = '\0';
    buf.pos = 0;

    ret = http_get(endpoint, &buf);
    if (ret) {
        /* no result if failure */
        SET_RESULT("%s", "");
        return ret;
    }

    SET_RESULT("%s", buf.data);
    return 0;
}

/*
 * Activate the RDB (bridge) service with retry on failure
 *
 * @return 0 on success; a negative error code on failure
 */
static int activate_rdb(void)
{
    int ret;
    int retry;
    for (retry = 0; !terminate && retry < MAX_RETRIES; retry++) {
        ret = activate("rdb", 1);
        if (!ret) {
            rdb_set_string(rdb_s, RDB_BRIDGE_ENABLE, "1");
            BLOG_NOTICE("Activated RDB after %d retries\n", retry);
            return 0;
        }
        BLOG_WARNING("Failed to activate RDB: #%d\n", retry);
        sleep(RETRY_WAIT_SECS);
    }
    BLOG_ERR("Failed to activate RDB after %d retries\n", retry);
    return -1;
}

// a comma separated list of OWA models (variants) that have rdb_bridge feature
#define RDB_BRIDGE_OWA_LIST_RDB "rdb_bridge.owa.list"

/*
 * Check if an OWA has rdb_bridge feature
 *
 * @param model The model name (variant) of the OWA under test
 * @return 1 if OWA has rdb_bridge; 0 otherwise
 */
static int owa_has_rdb_bridge(const char * model)
{
    int ret;
    char buf[BUFSZ];
    char * pch;
    ret = rdb_get_string(rdb_s, RDB_BRIDGE_OWA_LIST_RDB, buf, BUFSZ);
    if (ret) {
        BLOG_ERR("%s does not exist\n", RDB_BRIDGE_OWA_LIST_RDB);
        return 0;
    }
    BLOG_DEBUG("%s: %s\n", RDB_BRIDGE_OWA_LIST_RDB, buf);

    pch = strtok(buf, ",");
    while (pch) {
        if (!strcmp(model, pch)) {
            return 1;
        }
        pch = strtok(NULL, ",");
    }
    return 0;
}

/*
 * handshake with OWA and activate rdb_bridge if owa supports it
 *
 *
 */
static int handshake(void)
{
    int ret;
    int retry;
    const struct hello_resp_t * resp;

    rdb_set_string(rdb_s, HANDSHAKE_STATE, "");
    for (retry = 0; !terminate && retry < MAX_RETRIES; retry++) {
        resp = get_owa_info();
        if (resp && resp->model[0]) {
            rdb_set_string(rdb_s, OWA_MODEL_RDB, resp->model);
            BLOG_INFO("OWA model: %s\n", resp->model);
            rdb_set_string(rdb_s, OWA_CAP_RDB, resp->cap);
            BLOG_INFO("OWA capabilities: %s\n", resp->cap);
            if (owa_has_rdb_bridge(resp->model)) {
                // if OWA has rdb_bridge, handshake is considered successful only after rdb_bridge is activated
                ret = activate_rdb();
                if (!ret) {
                    rdb_set_string(rdb_s, HANDSHAKE_STATE, "succ");
                    return 0;
                } else {
                    break;
                }
            } else {
                rdb_set_string(rdb_s, HANDSHAKE_STATE, "succ");
                BLOG_NOTICE("Handshake with %s succeeded after %d retries\n", resp->model, retry);
                return 0;
            }
        }
        BLOG_WARNING("Handshake failed (OWA model unknown): #%d\n", retry);
        sleep(RETRY_WAIT_SECS);
    }
    rdb_set_string(rdb_s, HANDSHAKE_STATE, "fail");
    return -1;
}

int main(void)
{
    CURLcode res;

    int ret;
    int rdbfd;
    fd_set fdset;

    int name_len;
    char name_buf[MAX_NAME_LENGTH];

    char * rpc_params[] = RPC_PARAMS;
    char * rpc_put_params[] = RPC_PUT_PARAMS;
    char * rpc_get_params[] = RPC_GET_PARAMS;

    openlog("auth_client", LOG_CONS, LOG_USER);

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin_log;
    }

    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res) {
        BLOG_ERR("Failed to init curl_global (%d): %s\n", res, curl_easy_strerror(res));
        ret = -res;
        goto fin_log;
    }

    curl = curl_easy_init();
    if (!curl) {
        BLOG_ERR("Failed to init curl_easy\n");
        ret = -1;
        goto fin_curl_global;
    }

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        ret = -1;
        goto fin_curl_cleanup;
    }

    ret = handshake();
    if (ret) {
        BLOG_ERR("Handshake failed\n");
        if (terminate) {
            BLOG_NOTICE("Exiting on signal\n");
            ret = 0;
        }
        goto fin_rdb;
    }

    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdb fd\n");
        ret = -1;
        goto fin_rdb;
    }

    ret = rdb_rpc_server_init(RPC_SERVICE_NAME, &rpc_s);
    if (ret) {
        BLOG_ERR("Failed to init rdb rpc (%d)\n", ret);
        goto fin_rdb;
    }

    ret = rdb_rpc_server_add_command(rpc_s, RPC_CMD, rpc_params, ARRAY_SIZE(rpc_params), cmd_handler);
    if (ret) {
        BLOG_ERR("Failed to add rpc command %s (%d)\n", RPC_CMD, ret);
        goto fin_rpc_destroy;
    }

    ret = rdb_rpc_server_add_command(rpc_s, RPC_PUT, rpc_put_params, ARRAY_SIZE(rpc_put_params), put_handler);
    if (ret) {
        BLOG_ERR("Failed to add rpc command %s (%d)\n", RPC_CMD, ret);
        goto fin_rpc_destroy;
    }

    ret = rdb_rpc_server_add_command(rpc_s, RPC_GET, rpc_get_params, ARRAY_SIZE(rpc_get_params), get_handler);
    if (ret) {
        BLOG_ERR("Failed to add rpc command %s (%d)\n", RPC_CMD, ret);
        goto fin_rpc_destroy;
    }

    ret = rdb_rpc_server_run(rpc_s, rdb_s);
    if (ret) {
        BLOG_ERR("Failed to run rpc server (%d)\n", ret);
        goto fin_rpc_destroy;
    }

    while (!terminate) {
        FD_ZERO(&fdset);
        FD_SET(rdbfd, &fdset);
        ret = select(rdbfd + 1, &fdset, NULL, NULL, NULL);

        if (ret < 0) { // error
            int tmp = errno;
            BLOG_ERR("select returned %d, errno: %d\n", ret, tmp);
            if (tmp == EINTR) { // interrupted by signal
                BLOG_NOTICE("Exiting on signal\n");
                break;
            }
            goto fin_rpc_stop;
        }

        if (ret > 0) { // available
            name_len = sizeof(name_buf) - 1;
            ret = rdb_getnames(rdb_s, "", name_buf, &name_len, TRIGGERED);
            if (ret) {
                BLOG_ERR("Failed to get triggered RDB names (%d)\n", ret);
                goto fin_rpc_stop;
            }
            ret = rdb_rpc_server_process_commands(rpc_s, name_buf);
            if (ret) {
                BLOG_ERR("Failed to process commands (%d)\n", ret);
                goto fin_rpc_stop;
            }
        }
    }

    BLOG_DEBUG("Exiting normally");
    ret = 0;

fin_rpc_stop:
    ret = rdb_rpc_server_stop(rpc_s);
    if (ret) {
        BLOG_ERR("Failed to stop rpc server\n");
    }

fin_rpc_destroy:
    ret = rdb_rpc_server_destroy(&rpc_s);
    if (ret) {
        BLOG_ERR("Failed to destroy rpc server\n");
    }

fin_rdb:
    rdb_close(&rdb_s);

fin_curl_cleanup:
    curl_easy_cleanup(curl);

fin_curl_global:
    curl_global_cleanup();

fin_log:
    closelog();

    return ret;
}

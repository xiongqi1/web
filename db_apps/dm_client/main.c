/*
 * NetComm OMA-DM Client
 *
 * main.c
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

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>

#include <omadmclient.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <lextras.h>
#include <logger.h>
#include <logger-lua.h>

#include "log.h"
#include "lua_att.h"
#include "lua_class.h"
#include "lua_dm_path.h"
#include "lua_enum.h"
#include "lua_object.h"
#include "lua_rdb.h"
#include "lua_sched.h"
#include "lua_util.h"
#include "lua_wbxml.h"
#include "options.h"

#define HMAC_BASE64_LENGTH 24

#define TIMEOUT_CONNECT_SEC             60  /* Max time taken to open connection. */
#define TIMEOUT_TRANSFER_SEC            60  /* For DM sessions; max total transfer time. */
#define TIMEOUT_LOW_SPEED_TIME_SEC      60  /* For downloads; max time spent at low speed before timeout. */
#define TIMEOUT_LOW_SPEED_BYTES_PER_SEC 500 /* For downloads; min permitted speed in bytes per second. */

#define LOG_DEBUG(fmt, ...) log_debug("%s: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) log_trace("%s: " fmt, __FUNCTION__, ##__VA_ARGS__)

typedef struct sessionqueue_t {
    char*         serverid;
    int           sessionid;
    dmclt_item_t* alert;
    int           cbRef;
    struct sessionqueue_t* next;
} sessionqueue_t;

typedef struct {
    lua_State*         L;
    CURL*              curl;
    dmclt_session      dm;
    dmclt_buffer_t     sendbuf;
    dmclt_buffer_t     recvbuf;
    int                cbRef;
    char*              serverid;
    struct curl_slist* headers;
} session_t;

typedef struct {
    lua_State* L;
    CURL*      curl;
    FILE*      file;
    int        cbRef;
} download_t;

typedef struct command_t {
    lua_State*        L;
    int               pid;
    int               fd;
    int               cbRef;
    struct command_t* next;
} command_t;

static lua_State*        __L            = NULL;
static CURLM*            __curlm        = NULL;
static session_t*        __session      = NULL;
static sessionqueue_t*   __sessionqueue = NULL;
static command_t*        __commands     = NULL;
static int               __commandCount = 0;
static options_t         __options      = {0};

static volatile bool signalled_exit = false;
static volatile bool signalled_pipe = false;
static void handle_signal(int signum)
{
    if (signum == SIGPIPE) {
        /* Some socket read/write errors will generate a SIGPIPE that
         * terminates the process if not caught. Nothing needs to be
         * done to handle the signal; the offending syscall will then
         * return EPIPE for the calling code to handle. */
        signal(SIGPIPE, handle_signal);
        signalled_pipe = true;
    } else {
        signalled_exit = true;
    }
}

/* Log the contents of a packet buffer, either as a hex
 * dump or text depending on whether WBXML is used. */
static void log_packet(const char* desc, const void* buf, int len, bool wbxml)
{
    if (wbxml) {
        log_hex_debug(desc, buf, len);
    } else {
        log_xml_debug(desc, buf, len);
    }
}

/* Search for a substring within a string, returning a
 * pointer to the first character after the substring. */
static const char* header_find(const char* data, size_t dataLen, const char* name)
{
    size_t nameLen = strlen(name);
    for (; dataLen >= nameLen; dataLen--, data++) {
        if (!memcmp(data, name, nameLen)) {
            return data + nameLen;
        }
    }
    return NULL;
}

/* dm.Execute(command, [callback])
 *
 * fork() and exec() the given shell command. A pipe is opened to the
 * child process so that the call to curl_multi_wait() in client_loop()
 * has a file descriptor it can use to track the life of the process.
 *
 * The command is executed asynchronously; the calling function may
 * specify a callback to be executed once the child process exits. */
static int command_start(lua_State* L)
{
    const char* cmd = luaL_checkstring(L, 1);
    luaL_opttype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    int cbRef = luaR_ref(L);

    LOG_DEBUG("cmd=%s", cmd);

    command_t* c = malloc(sizeof(command_t));
    if (!c) {
        log_error("malloc() failed: %s.", strerror(errno));
    }
    else {
        int fds[2];
        if (pipe(fds)) {
            log_error("pipe() failed: %s.", strerror(errno));
        }
        else {
            pid_t pid = fork();
            if (pid < 0) {
                log_error("fork() failed: %s.", strerror(errno));
            }
            else if (pid == 0) {
                close(fds[0]);
                if (execl("/bin/sh", "sh", "-c", cmd, NULL)) {
                    log_error("execl() failed: %s.", strerror(errno));
                    exit(1);
                }
            }
            else {
                LOG_DEBUG("c=%#p", c);
                LOG_DEBUG("pid=%i", pid);
                LOG_DEBUG("fd=%i", fds[0]);
                close(fds[1]);
                c->L = L;
                c->pid = pid;
                c->fd = fds[0];
                c->cbRef = cbRef;
                c->next = __commands;
                __commands = c;
                __commandCount++;
                LOG_TRACE("exit; rc=0");
                return 0;
            }
            close(fds[0]);
            close(fds[1]);
        }
        free(c);
    }

    luaR_unref(L, cbRef);
    return luaL_error(L, "failed to execute command");
}

static int command_count_waitfds(void)
{
    return __commandCount;
}

static void command_get_waitfds(struct curl_waitfd* fds)
{
    for (command_t* cmd = __commands; cmd; cmd = cmd->next, fds++) {
        fds->fd = cmd->fd;
        fds->events = POLLHUP;
        fds->revents = 0;
    }
}

/* Check the list of active shell commands, reaping any processes
 * that have finished and passing the results to the corresponding
 * Lua callback, if one was provided. */
static void command_update(void)
{
    command_t* cPrev = NULL;
    for (command_t* c = __commands; c; cPrev = c, c = c->next) {
        int status;
        if (waitpid(c->pid, &status, WNOHANG) > 0) {
            LOG_DEBUG("pid=%i", c->pid);
            close(c->fd);
            if (c->cbRef == LUA_REFNIL) {
                LOG_DEBUG("no callback");
            }
            else {
                luaR_getref(c->L, c->cbRef);
                lua_pushboolean(c->L, WIFEXITED(status) && !WEXITSTATUS(status));
                lua_pushinteger(c->L, WIFEXITED(status) ? WEXITSTATUS(status) : 0);
                lua_pushinteger(c->L, WIFSIGNALED(status) ? WTERMSIG(status) : 0);
                if (lua_pcall(c->L, 3, 0, 0)) {
                    log_error("lua_pcall() failed: %s.", lua_tostring(c->L, -1));
                    lua_pop(c->L, 1);
                }
                luaR_unref(c->L, c->cbRef);
            }
            if (cPrev) {
                cPrev->next = c->next;
            } else {
                __commands = c->next;
            }
            __commandCount--;
            free(c);
        }
    }
}

/* Create a new download object, preparing the destination
 * file and a curl handle needed to perform the download. */
static download_t* download_create(lua_State* L, const char* path, int cbRef)
{
    LOG_TRACE("entry; path=%s, cbRef=%i", path, cbRef);

    download_t* dl = malloc(sizeof(download_t));
    if (!dl) {
        log_error("malloc() failed: %s.", strerror(errno));
    }
    else {
        dl->L = L;
        dl->cbRef = cbRef;
        dl->curl = curl_easy_init();
        if (!dl->curl) {
            log_error("curl_easy_init() failed.");
        }
        else {
            dl->file = fopen(path, "wb");
            if (!dl->file) {
                log_error("fopen() failed: %s.", strerror(errno));
            }
            else {
                LOG_TRACE("exit; dl=%#p", dl);
                return dl;
            }
            curl_easy_cleanup(dl->curl);
        }
        free(dl);
    }

    LOG_TRACE("exit; dl=%#p", NULL);
    return NULL;
}

/* CURLOPT_WRITEFUNCTION callback that is used to receive
 * and write data to file as part of a download task. */
static size_t download_write(void* ptr, size_t size, size_t count, void* userdata)
{
    download_t* dl = userdata;
    return fwrite(ptr, size, count, dl->file);
}

/* dm.Download(srcUrl, dstPath, maxSize, [callback])
 *
 * Download the specified URL to the specified path on
 * the device. The download is completed asynchronously;
 * the calling function may specify a callback to be
 * executed when the download is complete. */
static int download_start(lua_State* L)
{
    LOG_TRACE("entry; L=%#p", L);

    const char* url = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int maxSize = luaL_checkinteger(L, 3);
    luaL_opttype(L, 4, LUA_TFUNCTION);
    lua_settop(L, 4);
    int cbRef = luaR_ref(L);

    download_t* dl = download_create(L, path, cbRef);
    if (!dl) {
        luaR_unref(L, cbRef);
        luaL_error(L, "failed to start download");
    }

    LOG_DEBUG("dl=%#p", dl);
    LOG_DEBUG("url=%s", url);
    LOG_DEBUG("path=%s", path);

    curl_easy_setopt(dl->curl, CURLOPT_URL,             url);
    curl_easy_setopt(dl->curl, CURLOPT_PRIVATE,         dl);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA,       dl);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION,   download_write);
    curl_easy_setopt(dl->curl, CURLOPT_MAXFILESIZE,     maxSize);
    curl_easy_setopt(dl->curl, CURLOPT_SSL_VERIFYPEER,  !__options.insecure);
    curl_easy_setopt(dl->curl, CURLOPT_FORBID_REUSE,    true);
    curl_easy_setopt(dl->curl, CURLOPT_CONNECTTIMEOUT,  TIMEOUT_CONNECT_SEC);
    curl_easy_setopt(dl->curl, CURLOPT_LOW_SPEED_TIME,  TIMEOUT_LOW_SPEED_TIME_SEC);
    curl_easy_setopt(dl->curl, CURLOPT_LOW_SPEED_LIMIT, TIMEOUT_LOW_SPEED_BYTES_PER_SEC);
    if (!__options.insecure && __options.ca_path) {
        curl_easy_setopt(dl->curl, CURLOPT_CAPATH, __options.ca_path);
    }

    curl_multi_add_handle(__curlm, dl->curl);

    LOG_TRACE("exit; rc=0");
    return 0;
}

/* Handle a completed download task, passing the results to
 * the corresponding callback function if one was provided,
 * then cleaning up. */
static void download_continue(CURL* curl, CURLcode result)
{
    LOG_TRACE("entry; curl=%#p, result=%i", curl, result);

    long response;
    download_t* dl;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, (char**)&dl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    LOG_DEBUG("dl=%#p", dl);
    LOG_DEBUG("response=%i", response);

    fclose(dl->file);

    if (result) {
        log_error("Download failed: %s.", curl_easy_strerror(result));
    }
    else if (response != 200) {
        log_error("Download failed, got %i response from server.", response);
    }

    if (dl->cbRef == LUA_REFNIL) {
        LOG_DEBUG("no callback");
    }
    else {
        bool success = (!result && response == 200);
        luaR_getref(dl->L, dl->cbRef);
        lua_pushboolean(dl->L, success);
        lua_pushinteger(dl->L, result);
        lua_pushinteger(dl->L, response);
        if (success) {
            char* content;
            double length;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content);
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length);
            LOG_DEBUG("length=%u", (unsigned int)length);
            LOG_DEBUG("content=%s", content);
            lua_pushinteger(dl->L, length);
            lua_pushstring(dl->L, content ? content : "");
        }
        if (lua_pcall(dl->L, success ? 5 : 3, 0, 0)) {
            log_error("lua_pcall() failed: %s.", lua_tostring(dl->L, -1));
            lua_pop(dl->L, 1);
        }
        luaR_unref(dl->L, dl->cbRef);
    }

    curl_multi_remove_handle(__curlm, curl);
    curl_easy_cleanup(curl);
    free(dl);

    LOG_TRACE("exit");
}

static void session_destroy_alert(dmclt_item_t* alert)
{
    LOG_TRACE("entry; alert=%#p", alert);

    if (alert) {
        free(alert->target);
        free(alert->source);
        free(alert->type);
        free(alert->format);
        free(alert->data);
        free(alert);
    }

    LOG_TRACE("exit");
}

/* Creates a dmclt_item_t object and populates it with
 * generic alert data from the given Lua table. */
static dmclt_item_t* session_create_alert(lua_State* L, int index)
{
    LOG_TRACE("entry; L=%#p, index=%i", L, index);

    if (lua_isnil(L, index)) {
        LOG_TRACE("exit; alert=%#p", NULL);
        return NULL;
    }

    dmclt_item_t* alert = malloc(sizeof(dmclt_item_t));
    if (!alert) {
        luaL_error(L, "out of memory");
    }

    memset(alert, 0, sizeof(dmclt_item_t));

    char* getfield(const char* field, int mandatory)
    {
        char* value = NULL;
        lua_getfield(L, index, field);
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1)) {
                session_destroy_alert(alert);
                luaL_error(L,
                    "expected string for alert field '%s', got %s",
                    field, luaL_typename(L, -1));
            }
            value = strdup(lua_tostring(L, -1));
            if (!value) {
                session_destroy_alert(alert);
                luaL_error(L, "out of memory");
            }
        } else if (mandatory) {
            luaL_error(L, "expected mandatory alert field '%s', got nil", field);
        }
        lua_pop(L, 1);
        return value;
    }

    alert->target = getfield("correlator", false);
    alert->source = getfield("source", false);
    alert->type   = getfield("type", true);
    alert->format = getfield("format", true);
    alert->data   = getfield("data", true);

    LOG_TRACE("exit; alert=%#p", alert);
    return alert;
}

/* Call the corresponding callback function for a completed
 * session, if one was provided. */
static void session_callback(lua_State* L, int cbRef, int success)
{
    LOG_TRACE("entry; L=%#p, cbRef=%i", L, cbRef);

    if (cbRef == LUA_REFNIL) {
        LOG_DEBUG("no callback");
    }
    else {
        luaR_getref(L, cbRef);
        lua_pushboolean(L, success);
        if (lua_pcall(L, 1, 0, 0)) {
            log_error("lua_pcall() failed: %s.", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    LOG_TRACE("exit");
}

/* Create a new DM session, initialise it, then install callbacks
 * into it to pass DM tree operations to Lua. Also creates a curl
 * handle to use for communications with the server. */
static session_t* session_create(lua_State*    L,
                                 const char*   serverid,
                                 int           sessionid,
                                 dmclt_item_t* alert,
                                 int           cbRef)
{
    LOG_TRACE("entry; L=%#p, serverid=%s, sessionid=%i, alert=%#p, cbRef=%i",
          L, serverid, sessionid, alert, cbRef);

    session_t* session = malloc(sizeof(session_t));
    if (!session) {
        log_error("malloc() failed: %s.", strerror(errno));
    }
    else {
        memset(session, 0, sizeof(session_t));
        session->L = L;
        session->cbRef = cbRef;
        session->serverid = strdup(serverid);
        if (!session->serverid) {
            log_error("strdup() failed: %s.", strerror(errno));
        }
        else {
            session->curl = curl_easy_init();
            if (!session->curl) {
                log_error("curl_easy_init() failed.");
            }
            else {
                session->dm = omadmclient_session_init(__options.wbxml);
                if (!session->dm) {
                    log_error("omadmclient_session_init() failed.");
                }
                else {
                    if (!lua_object_install_all(L, session->dm)) {
                        log_error("lua_object_install_all() failed.");
                    }
                    else {
                        int rc = omadmclient_session_start(session->dm, serverid, sessionid);
                        if (rc) {
                            log_error("omadmclient_session_start() failed with code %i.", rc);
                        }
                        else if (alert) {
                            char* correlator = alert->target;
                            alert->target = NULL;
                            rc = omadmclient_add_generic_alert(session->dm, correlator, alert);
                            free(correlator);
                            if (rc) {
                                log_error("omadmclient_add_generic_alert() failed with code %i.", rc);
                            }
                        }
                        if (!rc) {
                            LOG_TRACE("exit; session=%#p", session);
                            return session;
                        }
                    }
                    omadmclient_session_close(session->dm);
                }
                curl_easy_cleanup(session->curl);
            }
            free(session->serverid);
        }
        free(session);
    }

    LOG_TRACE("exit; session=%#p", NULL);
    return NULL;
}

static void session_destroy(session_t* session)
{
    LOG_TRACE("entry; session=%#p", session);

    if (session) {
        omadmclient_session_close(session->dm);
        omadmclient_clean_buffer(&session->sendbuf);
        omadmclient_clean_buffer(&session->recvbuf);
        curl_easy_cleanup(session->curl);
        curl_slist_free_all(session->headers);
        luaR_unref(session->L, session->cbRef);
        free(session->serverid);
        free(session);
    }

    LOG_TRACE("exit");
}

/* CURLOPT_HEADERFUNCTION callback that is used to receive
 * and process incoming HTTP headers during a session. */
static size_t session_recvheader(char* data, size_t size, size_t count, void* userdata)
{
    dmclt_buffer_t* reply = (dmclt_buffer_t*)userdata;
    size *= count;
    if (header_find(data, size, "x-syncml-hmac:")) {
        const char* mac = header_find(data, size, "mac=");
        if (mac) {
            int len = size - (mac - data);
            if (len > HMAC_BASE64_LENGTH) {
                len = HMAC_BASE64_LENGTH;
            }
            reply->auth_type = DMCLT_AUTH_TYPE_HMAC;
            reply->auth_data = (unsigned char*)strndup(mac, len);
            reply->auth_data_length = len;
            if (!reply->auth_data) {
                log_error("strndup() failed: %s.", strerror(errno));
                return 0;
            }
        }
    }
    return size;
}

/* CURLOPT_WRITEFUNCTION callback that is used to receive
 * incoming data during a session. */
static size_t session_recvdata(void* data, size_t size, size_t count, void* userdata)
{
    dmclt_buffer_t* reply = (dmclt_buffer_t*)userdata;
    size *= count;
    reply->data = realloc(reply->data, reply->length + size);
    if (!reply->data) {
        log_error("Out of memory.");
        return 0;
    }
    memcpy(reply->data + reply->length, data, size);
    reply->length += size;
    return size;
}

/* Called by session_create(), session_dequeue() and session_continue() to
 * prepare the next packet to be sent to the server as part of a session,
 * or to inform the calling function if there is nothing more to be sent. */
static int session_prepare_curl(session_t* session, int* success)
{
    LOG_TRACE("entry; session=%#p, success=%#p", session, success);

    omadmclient_clean_buffer(&session->sendbuf);
    omadmclient_clean_buffer(&session->recvbuf);

    int rc = omadmclient_get_next_packet(session->dm, &session->sendbuf);
    if (rc) {
        if (rc != DMCLT_ERR_END) {
            log_error("omadmclient_get_next_packet() failed with code %i.", rc);
        }
        *success = rc == DMCLT_ERR_END;
        LOG_TRACE("exit; rc=false, *success=%s", *success ? "true" : "false");
        return false;
    }

    log_info("Sending %u bytes to '%s'.", session->sendbuf.length, session->sendbuf.uri);

    curl_slist_free_all(session->headers);
    session->headers = curl_slist_append(NULL,
        __options.wbxml ? "Content-Type: application/vnd.syncml+wbxml"
                        : "Content-Type: application/vnd.syncml+xml"
    );

    if (session->sendbuf.auth_type == DMCLT_AUTH_TYPE_HMAC) {
        char hdr[80 + strlen(session->sendbuf.auth_user)];
        snprintf(hdr, sizeof(hdr), "x-syncml-hmac: algorithm=MD5, username=\"%s\", mac=%.*s",
            session->sendbuf.auth_user,
            session->sendbuf.auth_data_length,
            session->sendbuf.auth_data
        );
        session->headers = curl_slist_append(session->headers, hdr);
        log_debug("Outgoing HMAC: %.*s",
            session->sendbuf.auth_data_length,
            session->sendbuf.auth_data
        );
    }

    log_packet("Outgoing packet", session->sendbuf.data, session->sendbuf.length, __options.wbxml);

    curl_easy_setopt(session->curl, CURLOPT_URL,                 session->sendbuf.uri);
    curl_easy_setopt(session->curl, CURLOPT_POST,                1);
    curl_easy_setopt(session->curl, CURLOPT_HTTPHEADER,          session->headers);
    curl_easy_setopt(session->curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)session->sendbuf.length);
    curl_easy_setopt(session->curl, CURLOPT_COPYPOSTFIELDS,      (void*)session->sendbuf.data);
    curl_easy_setopt(session->curl, CURLOPT_SSL_VERIFYPEER,      !__options.insecure);
    curl_easy_setopt(session->curl, CURLOPT_HEADERFUNCTION,      session_recvheader);
    curl_easy_setopt(session->curl, CURLOPT_HEADERDATA,          &session->recvbuf);
    curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION,       session_recvdata);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA,           &session->recvbuf);
    curl_easy_setopt(session->curl, CURLOPT_PRIVATE,             session);
    curl_easy_setopt(session->curl, CURLOPT_FORBID_REUSE,        true);
    curl_easy_setopt(session->curl, CURLOPT_TIMEOUT,             TIMEOUT_TRANSFER_SEC);
    curl_easy_setopt(session->curl, CURLOPT_CONNECTTIMEOUT,      TIMEOUT_CONNECT_SEC);
    if (!__options.insecure && __options.ca_path) {
        curl_easy_setopt(session->curl, CURLOPT_CAPATH, __options.ca_path);
    }

    LOG_TRACE("exit; rc=true");
    return true;
}

/* Called by session_create() to queue a session for later
 * if one is already in progress. */
static void session_enqueue(lua_State*    L,
                            const char*   serverid,
                            int           sessionid,
                            dmclt_item_t* alert,
                            int           cbRef)
{
    LOG_TRACE("entry; L=%#p, serverid=%s, sessionid=%i, alert=%#p, cbRef=%i",
          L, serverid, sessionid, alert, cbRef);

    sessionqueue_t* next = malloc(sizeof(sessionqueue_t));
    if (next) {
        next->serverid = strdup(serverid);
        if (next->serverid) {
            next->sessionid = sessionid;
            next->alert = alert;
            next->cbRef = cbRef;
            next->next = NULL;
            if (!__sessionqueue) {
                __sessionqueue = next;
            } else {
                sessionqueue_t* tail = __sessionqueue;
                while (tail->next) {
                    tail = tail->next;
                }
                tail->next = next;
            }
            LOG_TRACE("exit");
            return;
        }
    }

    session_destroy_alert(alert);
    luaR_unref(L, cbRef);
    luaL_error(L, "out of memory");
}

/* Called at the completion of a session to de-queue and prepare
 * the next session if one is waiting. */
static session_t* session_dequeue(lua_State* L)
{
    LOG_TRACE("entry; L=%#p", L);

    session_t* session = NULL;
    sessionqueue_t* next = __sessionqueue;

    for (sessionqueue_t* next2 = NULL; next; next = next2) {
        next2 = next->next;
        LOG_DEBUG("serverid=%s", next->serverid);
        LOG_DEBUG("sessionid=%i", next->sessionid);
        session = session_create(L, next->serverid, next->sessionid, next->alert, next->cbRef);
        session_destroy_alert(next->alert);
        if (!session) {
            session_callback(L, next->cbRef, false);
        }
        free(next->serverid);
        free(next);
        if (session) {
            __sessionqueue = next2;
            int success;
            if (session_prepare_curl(session, &success)) {
                break;
            }
            session_callback(L, session->cbRef, success);
            session_destroy(session);
            session = NULL;
        }
    }

    LOG_TRACE("exit; session=%#p", session);
    return session;
}

/* dm.DoSession(serverid, sessionid, [alert], [callback])
 *
 * Start a DM session to the given server ID, including an optional
 * generic alert. If a session is already in progress the new session
 * will be queued and begin immediately afterwards.
 *
 * The session is performed asynchronously; the calling function
 * may provide a callback to be executed after the session is
 * complete. */
static int session_start(lua_State* L)
{
    LOG_TRACE("entry; L=%#p", L);

    const char* serverid = luaL_checkstring(L, 1);
    int sessionid = luaL_checkinteger(L, 2);
    luaL_opttype(L, 3, LUA_TTABLE);
    luaL_opttype(L, 4, LUA_TFUNCTION);

    lua_settop(L, 4);
    dmclt_item_t* alert = session_create_alert(L, 3);
    int cbRef = luaR_ref(L);

    LOG_DEBUG("serverid=%s", serverid);
    LOG_DEBUG("sessionid=%i", sessionid);

    if (__session) {
        session_enqueue(L, serverid, sessionid, alert, cbRef);
    } else {
        int success;
        session_t* session = session_create(L, serverid, sessionid, alert, cbRef);
        session_destroy_alert(alert);
        if (!session) {
            session_callback(L, cbRef, false);
        }
        else if (!session_prepare_curl(session, &success)) {
            session_callback(L, session->cbRef, success);
            session_destroy(session);
        }
        else {
            curl_multi_add_handle(__curlm, session->curl);
            __session = session;
        }
    }

    LOG_TRACE("exit; rc=0");
    return 0;
}

/* Called by client_loop() to process replies during a DM session
 * and then either prepare the next packet to be sent, or close
 * the session if there is nothing more to be done. */
static void session_continue(CURL* curl, CURLcode result)
{
    LOG_TRACE("entry; curl=%#p, result=%i", curl, result);

    long response;
    session_t* session;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, (char**)&session);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    curl_multi_remove_handle(__curlm, curl);

    LOG_DEBUG("session=%#p", session);
    LOG_DEBUG("response=%i", response);

    lua_State* L = session->L;
    int finished = true;
    int success = false;

    if (result) {
        log_error("Session failed: %s.", curl_easy_strerror(result));
    }
    else if (response != 200) {
        log_error("Session failed, got %i response from server.", response);
    }
    else {
        log_info("Got reply, %i bytes.", session->recvbuf.length);
        if (session->recvbuf.auth_type == DMCLT_AUTH_TYPE_HMAC) {
            log_debug("Incoming HMAC: %.*s",
                session->recvbuf.auth_data_length,
                session->recvbuf.auth_data
            );
        }
        log_packet("Incoming packet", session->recvbuf.data, session->recvbuf.length, __options.wbxml);
        int rc = omadmclient_process_reply(session->dm, &session->recvbuf);
        if (rc) {
            log_error("omadmclient_process_reply() failed: %s.", omadmclient_strerror(rc));
        }
        else {
            finished = !session_prepare_curl(session, &success);
        }
    }

    if (finished) {
        session_callback(L, session->cbRef, success);
        session_destroy(session);
        session = session_dequeue(L);
    }
    if (session) {
        curl_multi_add_handle(__curlm, session->curl);
    }
    __session = session;

    LOG_TRACE("exit; __session=%#p", __session);
}

/* dm.GetServerID()
 *
 * Returns the server ID for the current session, or nil
 * if no session is in progress. */
static int session_getserverid(lua_State* L)
{
    LOG_TRACE("entry; L=%#p", L);

    if (__session) {
        lua_pushstring(L, __session->serverid);
    } else {
        lua_pushnil(L);
    }

    LOG_TRACE("exit; rc=1; serverid=%s", __session ? __session->serverid : NULL);
    return 1;
}

/* Session callback installed by session_console_start() to log
 * the completion of a console-initiated session. */
static int session_console_finish(lua_State* L)
{
    if (lua_toboolean(L, 1)) {
        log_info("Console-initiated session complete.");
    } else {
        log_error("Console-initiated session failed.");
    }
    return 0;
}

/* Event callback that is installed to trigger a console-initiated
 * session. This function is scheduled by client_loop() to execute
 * 1000ms after startup if a server ID is provided on the command
 * line using the '--server' option. */
static int session_console_start(lua_State* L)
{
    if (__options.server_id) {
        log_notice("Starting console-initiated session with server ID '%s'.", __options.server_id);
        lua_settop(L, 0);
        lua_pushstring(L, __options.server_id);
        lua_pushinteger(L, 1);
        lua_pushnil(L);
        lua_pushcfunction(L, session_console_finish);
        session_start(L);
    }
    return 0;
}

/* Create and initialise a new Lua state, loading in order:
 *  - standard Lua libraries
 *  - DM client Lua libraries
 *  - scripted objects (/usr/lib/dm/objects/*.lua)
 * After that it will run the Initialise() hook for any
 * scripted objects registered during this process. */
static int client_initialise()
{
    log_notice("Starting client.");

    if (__options.insecure) {
        log_warning("Certificate validation is disabled.");
    }

    CURLM* curlm = curl_multi_init();
    if (!curlm) {
        log_error("curl_multi_init() failed.");
    }
    else {
        lua_State* L = luaL_newstate();
        if (!L) {
            log_error("luaL_newstate() failed.");
        }
        else {
            luaL_openlibs(L);
            luaopen_logger_C(L);

            luaopen_att(L);
            luaopen_class(L);
            luaopen_dm_path(L);
            luaopen_enum(L);
            luaopen_object(L);
            luaopen_rdb(L);
            luaopen_schedule(L);
            luaopen_util(L);
            luaopen_wbxml(L);

            // FIXME: Module loader is not informed.
            lua_newtable(L);
            lua_pushcfunction(L, download_start);
            lua_setfield(L, -2, "Download");
            lua_pushcfunction(L, session_start);
            lua_setfield(L, -2, "DoSession");
            lua_pushcfunction(L, command_start);
            lua_setfield(L, -2, "Execute");
            lua_pushcfunction(L, session_getserverid);
            lua_setfield(L, -2, "GetServerID");
            lua_setglobal(L, "dm");

            /* Extend 'package.path' to search our modules. */
            lua_getglobal(L, "package");
            lua_pushstring(L, __options.module_path);
            lua_pushliteral(L, "/?.lua;");
            lua_getfield(L, -3, "path");
            lua_concat(L, 3);
            lua_setfield(L, -2, "path");
            lua_pop(L, 1);

            /* Add extra module search paths for host builds. */
            #ifdef HOST_LUA_MODULE_PATH
                lua_getglobal(L, "package");
                lua_pushliteral(L, HOST_LUA_MODULE_PATH);
                lua_getfield(L, -2, "path");
                lua_concat(L, 2);
                lua_setfield(L, -2, "path");
                lua_pop(L, 1);
            #endif
            #ifdef HOST_LUA_MODULE_CPATH
                lua_getglobal(L, "package");
                lua_pushliteral(L, HOST_LUA_MODULE_CPATH);
                lua_getfield(L, -2, "cpath");
                lua_concat(L, 2);
                lua_setfield(L, -2, "cpath");
                lua_pop(L, 1);
            #endif

            log_info("Loading scripted objects.");

            if (!luaL_dodir(L, __options.object_path)) {
                log_error("luaL_dodir() failed: %s.", lua_tostring(L, -1));
            }
            else {
                if (!lua_object_initialise_all(L)) {
                    log_error("lua_object_initialise_all() failed.");
                }
                else {
                    __L = L;
                    __curlm = curlm;
                    return true;
                }
            }
            lua_close(L);
        }
        curl_multi_cleanup(curlm);
    }

    return false;
}

/* Client event loop, watches and dispatches events for:
 *  - curl handles
 *  - rdb subscriptions
 *  - scheduled events
 *  - console commands
 */
static void client_loop()
{
    log_notice("Entering event loop.");

    lua_schedule_event(__L, 1000, session_console_start);

    while (!signalled_exit)
    {
        /* Perform any pending curl jobs. */
        int remaining;
        int rc = curl_multi_perform(__curlm, &remaining);
        if (rc) {
            log_error("curl_multi_perform() failed: %s.", curl_multi_strerror(rc));
        }

        /* Handle any completed curl jobs. */
        CURLMsg* msg;
        do {
            msg = curl_multi_info_read(__curlm, &remaining);
            if (msg && msg->msg == CURLMSG_DONE) {
                if (__session && __session->curl == msg->easy_handle) {
                    session_continue(msg->easy_handle, msg->data.result);
                } else {
                    download_continue(msg->easy_handle, msg->data.result);
                }
            }
        } while (msg);

        /* Wait for curl jobs and RDB notifications until the next
         * scheduled event, for a maximum duration of 1 second. */
        int wait_ms = lua_schedule_calculate_wait_ms(__L, 1000);
        int fdCount = command_count_waitfds();
        struct curl_waitfd fds[fdCount + 1];
        command_get_waitfds(fds);
        fds[fdCount].fd = lua_rdb_getfd(__L);
        fds[fdCount].events = CURL_WAIT_POLLIN;
        fds[fdCount].revents = 0;

        rc = curl_multi_wait(__curlm, fds, fdCount + 1, wait_ms, NULL);
        if (rc) {
            log_error("curl_multi_wait() failed: %s.", curl_multi_strerror(rc));
        }

        /* Dispatch pending RDB notifications, and run one scheduler cycle. */
        if (!lua_rdb_notify(__L)) {
            log_error("lua_rdb_notify() failed: %s.", lua_tostring(__L, -1));
            lua_pop(__L, 1);
        }
        if (!lua_schedule_run(__L)) {
            log_error("lua_schedule_run() failed: %s.", lua_tostring(__L, -1));
            lua_pop(__L, 1);
        }
        command_update();

        /* For the curious. */
        if (signalled_pipe) {
            log_warning("Received SIGPIPE, ignoring.");
            signalled_pipe = false;
        }
    }

    if (signalled_exit) {
        log_warning("Received signal, breaking.");
    }
}

static void client_shutdown()
{
    log_notice("Shutting down client.");

    if (__session) {
        curl_multi_remove_handle(__curlm, __session->curl);
        session_destroy(__session);
    }

    lua_object_shutdown_all(__L);
    lua_close(__L);

    int rc = curl_multi_cleanup(__curlm);
    if (rc) {
        log_error("curl_multi_cleanup() failed: %s.", curl_multi_strerror(rc));
    }
}

int main(int argc, char** argv)
{
    parse_options(&__options, argc, argv);

    logger_opts logopts = {
        .min_level = __options.log_level,
        .use_syslog = __options.syslog,
        .use_console = !__options.quiet,
        .no_date = false,
        .no_source = false,
        .no_colors = false
    };

    logger_initialise(&logopts, "dm_client");

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, handle_signal);

    int rc = curl_global_init(CURL_GLOBAL_ALL);
    if (rc) {
        log_error("curl_global_init() failed: %s.", curl_easy_strerror(rc));
    } else {
        if (!client_initialise()) {
            rc = 1;
        } else {
            client_loop();
            client_shutdown();
        }
        curl_global_cleanup();
    }

    if (rc) {
        log_error("Exiting due to errors.");
        return 1;
    }

    log_notice("Exiting successfully.");
    return 0;
}

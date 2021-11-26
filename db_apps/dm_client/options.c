/*
 * NetComm OMA-DM Client
 *
 * options.c
 * Command-line option parsing.
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

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <logger.h>

#include "options.h"

#ifndef DM_LUA_MODULE_PATH
#define DM_LUA_MODULE_PATH "/usr/lib/dm/modules"
#endif
#ifndef DM_LUA_OBJECT_PATH
#define DM_LUA_OBJECT_PATH "/usr/lib/dm/objects"
#endif

#define CMD_ERROR(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)

static const char* argv0 = NULL;
static const char* getopt_str = "c:hL:m:o:Qs:Sw";
static const struct option getopt_opts[] = {
    { "ca-path",     required_argument, 0, 'c' },
    { "help",        no_argument,       0, 'h' },
    { "insecure",    no_argument,       0, 'i' },
    { "log-level",   required_argument, 0, 'L' },
    { "module-dir",  required_argument, 0, 'm' },
    { "object-dir",  required_argument, 0, 'o' },
    { "quiet",       no_argument,       0, 'Q' },
    { "server",      required_argument, 0, 's' },
    { "syslog",      no_argument,       0, 'S' },
    { "wbxml",       no_argument,       0, 'w' },
    { 0, 0, 0, 0 }
};

void exit_with_usage(int code)
{
    CMD_ERROR(
        "Usage: %s [OPTION]... [SERVER OPTIONS]...\n\n"
        "Options:\n"
        "    --help             Display this information.\n"
        "    --module-dir=PATH  Path where Lua modules are stored.\n"
        "    --object-dir=PATH  Path where scripted objects are stored.\n"
        "    --wbxml            Use WBXML instead of XML for communication.\n"
        "    --ca-path=PATH     Path where CA certificates are stored.\n"
        "    --insecure         Allow TLS connections without valid certificates.\n"
        "    --server=SERVERID  Start a session for the specified server ID.\n"
        "    --syslog           Output log messages to syslog.\n"
        "    --quiet            Do not output logs to the console.\n"
        "    --log-level=LEVEL  Minimum log level to output. Supported options, in\n"
        "                       order, are 'trace', 'debug', 'info', 'notice',\n"
        "                       'warning', and 'error', with the default being\n"
        "                       'info'.",
        argv0
    );
    exit(code);
}

static void parse_string(const char** dst, const char* src, const char* name)
{
    if (strlen(src) < 1) {
        CMD_ERROR("Empty string provided for %s.", name);
        exit_with_usage(1);
    }

    *dst = src;
}

static void parse_log_level(int* dst, const char* src)
{
    if (strlen(src) < 1) {
        CMD_ERROR("Empty string provided for log level.");
        exit_with_usage(1);
    }
    if (!strcmp(src, "error")) {
        *dst = LOGGER_ERROR;
    }
    else if (!strcmp(src, "warning")) {
        *dst = LOGGER_WARNING;
    }
    else if (!strcmp(src, "notice")) {
        *dst = LOGGER_NOTICE;
    }
    else if (!strcmp(src, "info")) {
        *dst = LOGGER_INFO;
    }
    else if (!strcmp(src, "debug")) {
        *dst = LOGGER_DEBUG;
    }
    else if (!strcmp(src, "trace")) {
        *dst = LOGGER_TRACE;
    }
    else {
        CMD_ERROR("Unsupported log level '%s'.", src);
        exit_with_usage(1);
    }
}

void parse_options(options_t* options, int argc, char** argv)
{
    argv0 = argv[0];
    opterr = 0;
    options->log_level = LOGGER_INFO;
    options->module_path = DM_LUA_MODULE_PATH;
    options->object_path = DM_LUA_OBJECT_PATH;

    int optval;
    while ((optval = getopt_long(argc, argv, getopt_str, getopt_opts, NULL)) > 0)
    {
        switch (optval)
        {
            case '?':
                CMD_ERROR("Unrecognised argument '%s'.", argv[optind-1]);
                exit_with_usage(1);
            case 'h':
                exit_with_usage(0);
            case 'i':
                options->insecure = true;
                break;
            case 'L':
                parse_log_level(&options->log_level, optarg);
                break;
            case 'm':
                parse_string(&options->module_path, optarg, "lua module path");
                break;
            case 'o':
                parse_string(&options->object_path, optarg, "lua object path");
                break;
            case 'c':
                parse_string(&options->ca_path, optarg, "CA certificate path");
                break;
            case 'Q':
                options->quiet = true;
                break;
            case 's':
                parse_string(&options->server_id, optarg, "server ID");
                break;
            case 'S':
                options->syslog = true;
                break;
            case 'w':
                options->wbxml = true;
                break;
        }
    }
}

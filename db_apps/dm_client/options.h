/*
 * NetComm OMA-DM Client
 *
 * options.h
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

#ifndef __OMADM_OPTIONS_H_20180402__
#define __OMADM_OPTIONS_H_20180402__

    #include <stdbool.h>

    typedef struct {
        /* Lua options. */
        const char* module_path; /* Path to directory containing module scripts. */
        const char* object_path; /* Path to directory containing object scripts. */

        /* Client options. */
        const char* server_id; /* Automatically start a session for the specified server ID. */
        bool        wbxml;     /* Use WBXML instead of XML. */
        bool        insecure;  /* Allow TLS connections without valid certificates. */
        const char* ca_path;   /* Path to directory containing CA certificates. */

        /* Logging options. */
        int         log_level; /* Minumum log level to output. */
        bool        syslog;    /* Log to syslog. */
        bool        quiet;     /* Don't log to console. */
    } options_t;

    /* Parse command-line options and populate the given options struct.
     * Intended to be called before any other components of the client
     * have been initialised; will terminate on an error. */
    void parse_options(options_t* options, int argc, char** argv);

    /* Print usage text to stderr then exit with the specified code. */
    void exit_with_usage(int code);

#endif

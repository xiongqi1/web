#ifndef OPTIONS_H_12453010042019
#define OPTIONS_H_12453010042019
/*
 * Option handler for RDB bridge daemon
 *
 * Original codes copied from cdcs_apps/padd
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

#include <unistd.h>

/* System defaults */
#define DEFAULT_DEBUG  0       /* Default : debug mode off  */
#define DEFAULT_DAEMON 0       /* Default : do not daemonise */
#define DEFAULT_MODE   0       /* Default : server mode */

/* TCP/IP defaults */
#define DEFAULT_PORT      "2048"  /* Default TCP port */
#define DEFAULT_RTIME     10      /* Retry period in seconds. */
#define DEFAULT_KEEPALIVE 1      /* Seconds between TCP keealive packets (0 for off) */
#define DEFAULT_KEEPALIVE_CNT 4   /* Number of failed keepalive probes before closing connection. */

/// Option structure
struct options {
	/* system */
	int debug_mode;
	int daemonize;

	/* TCP/IP */
	const char *tcp_port;  /* Remote server TCP port number or TCP port number for incoming client */
	const char *server;    /* Server IP address to connect in client mode */
	int retry_time;        /* Retry period in second unit */
	int keepalive;         /* Seconds between TCP keealive packets (0 for off). */
	int keepalive_cnt;     /* How many failed keepalive probes cause a disconnect. */
	int client_mode;       /* nonzero in Client mode */
};

extern struct options options;

/**
 * Option handler
 *
 * @param   argc    Argument count
 * @param   argv    Argument variables
 * @param   opt     Option structure pointer
 *
 * @retval  0         ok
 * @retval  non-zero  fail
 */
int HandleOptions(int argc, char *argv[], struct options *opt);

/**
 * Dump options
 *
 * @param   opt     Option structure pointer
 *
 * @retval  none
 */
void DumpOptions(struct options *opt);

#endif

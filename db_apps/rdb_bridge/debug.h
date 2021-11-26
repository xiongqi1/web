#ifndef DEBUG_H_12320011042019
#define DEBUG_H_12320011042019
/*
 * Debugging functions for RDB bridge daemon
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

#include <stdio.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <time.h>

#include <assert.h>

extern int daemonized; /* if true, send output to syslog. */
extern int debug_mode; /* if false than debug messages get suppressed */

/* Use this for messages to the user. */
#define MSG(fmt,...) do { \
	if (daemonized) \
		syslog (LOG_NOTICE, fmt, ##__VA_ARGS__); \
	else \
		fprintf(stderr, fmt "\n" , ##__VA_ARGS__); \
} while (0)

#define ERR(fmt,...) do { \
	if (daemonized) \
		syslog (LOG_ERR, fmt, ##__VA_ARGS__); \
	else \
		fprintf(stderr, fmt "\n" , ##__VA_ARGS__); \
} while (0)

#define DBG(...) do {\
	if (debug_mode>0) MSG(__VA_ARGS__); \
} while (0)

#endif

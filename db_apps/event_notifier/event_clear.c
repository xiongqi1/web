/*!
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "event_rdb_util.h"
#include "event_rdb_names.h"
#include "event_type.h"
#include "event_util.h"

#define APPLICATION_NAME "event_clear"

static int event_noti_max_size = 100;

static void clear_event_noti_buffer(void)
{
	char rdb_name_buf[RDB_VAR_SIZE];
	int idx;

	syslog(LOG_INFO, "clear all event notification buffer, size = %d...", event_noti_max_size);
	for (idx = 0; idx < event_noti_max_size; idx++) {
		sprintf((char *)rdb_name_buf, "%s.%d.name", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.clients", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.retry_cnt", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.text", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.type", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.sms_dests", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.email_dests", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.tcp_dests", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.tcp_port", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.udp_dests", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
		sprintf((char *)rdb_name_buf, "%s.%d.udp_port", RDB_EVENT_NOTI_EVENT_PREPIX, idx);(void) rdb_setVal(rdb_name_buf, "");
	}
}

int running=1;
int verbosity=0;

static int init()
{
	const char *p;

	openlog(APPLICATION_NAME ,LOG_PID | LOG_PERROR, LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity? LOG_DEBUG:LOG_INFO));

	#ifdef LOG_LEVEL_CHECK
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"loglevel check");
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"syslog LOG_ERR");
	syslog(LOG_INFO,"syslog LOG_INFO");
	syslog(LOG_DEBUG,"syslog LOG_DEBUG");
	#endif

	/* signal handler set */
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);

	if(rdb_init()<0) {
		syslog(LOG_ERR,"rdb_init() failed");
		goto err;
	}

	/* initialize max noti event size */
	p=rdb_getVal(RDB_EVENT_NOTI_MAX_SIZE);
	if (p) event_noti_max_size = atoi(p);
	syslog(LOG_INFO, "event noti buffer max size = %d", event_noti_max_size);
	return 0;
err:
	return -1;
}

static int print_usage()
{
	fprintf(stderr,
		"Usage: "APPLICATION_NAME" [-v] [-V]\n"

		"\n"
		"Options:\n"

		"\t-v verbose mode (debugging log output)\n"
		"\t-V Display version information\n"
		"\n"
	);

	return 0;
}

int main(int argc,char* argv[])
{
	int ret;

	/* Parse Options */
	while ((ret = getopt(argc, argv, "vVh")) != EOF) {
		switch (ret) {
			case 'v': verbosity=1; break;
			case 'V': fprintf(stderr, "%s: build date / %s %s\n",APPLICATION_NAME, __TIME__,__DATE__); return 0;
			case 'h':
			case '?': print_usage(argv); return 2;
		}
	}
	init();
	clear_event_noti_buffer();
	syslog(LOG_INFO, "finished...");
	return 0;
}

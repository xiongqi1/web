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

#include "daemon.h"
#include "event_rdb_util.h"
#include "event_rdb_names.h"
#include "event_type.h"
#include "event_util.h"

#define APPLICATION_NAME "event_notifier"
#define LOCK_FILE "/var/lock/subsys/"APPLICATION_NAME

static char event_noti_cmd[RDB_VAR_SIZE];

static int get_event_noti_result(void)
{
	const char *p;
	p = rdb_getVal(RDB_EVENT_NOTI_RESULT);
	if (p && strcmp(p, "1") == 0)
		return 0;
	return -1;
}

static int send_event_notification_to_single_client(int noti_idx, const char *noti_client)
{
	memset(event_noti_cmd, 0x00, RDB_VAR_SIZE);
	sprintf(event_noti_cmd, "enotifier %d %s", noti_idx, noti_client);
	system((const char*)&event_noti_cmd[0]);
	return get_event_noti_result();
}

static void send_event_notification(int noti_idx, char *noti_clients)
{
	char noti_clients_local[RDB_VAL_SIZE];
	char noti_client[16];
	char *token, *saveptr;
	strcpy(noti_clients_local, noti_clients);
	noti_clients[0] = 0;
	token=NULL;
	while( (token=strtok_r(!token?(char *)noti_clients_local:NULL,";",&saveptr))!=NULL ) {
		strcpy((char *)&noti_client, token);
		syslog(LOG_DEBUG, "noti_client = %s", noti_client);
		/* if failed to send a notification to a client then leave the client name to retry later */
		if (send_event_notification_to_single_client(noti_idx, noti_client) < 0) {
			noti_clients += sprintf(noti_clients, "%s;", noti_client);
			syslog(LOG_DEBUG, "failed to send a noti idx '%d' to noti_clients '%s'", noti_idx, noti_client);
		}
	}
}

static int event_noti_max_size = 100;
#define GET_P_THEN_INCREMENT_RD_IDX do {\
	p = rdb_getVal(rdb_name_buf);\
	if (!p) {\
		sprintf((char *)rdb_name_buf, "%s.%d.retry_cnt", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);\
		(void) rdb_setVal(rdb_name_buf, "0");\
		event_valid[event_noti_rd_idx++] = 0;\
		if (event_noti_rd_idx >= event_noti_max_size) event_noti_rd_idx = 0;\
		continue; }\
	} while (0)
#define MAX_NOTI_CNT_PER_POLLING_PERIOD	10
static int polling_event_noti_buffer(void)
{
	const char *p;
	char rdb_name_buf[RDB_VAR_SIZE];
	char rdb_val_buf[RDB_VAL_SIZE];
	int event_type, event_noti_rd_idx = 0, rd_idx_backup = 0, event_noti_wr_idx = 0;
	char event_noti_clients[RDB_VAL_SIZE];
	char *event_valid = NULL;
	int processed_noti_cnt = 0;

	p = rdb_getVal(RDB_EVENT_NOTI_WR_IDX); if (p) event_noti_wr_idx = atoi(p);
	p = rdb_getVal(RDB_EVENT_NOTI_RD_IDX); if (p) event_noti_rd_idx = rd_idx_backup = atoi(p);
	if (event_noti_rd_idx == event_noti_wr_idx) return 1;

	syslog(LOG_DEBUG, "event_noti_rd_idx = %d, event_noti_wr_idx = %d", event_noti_rd_idx, event_noti_wr_idx);
	event_valid = malloc(event_noti_max_size+1);
	if (!event_valid) return 1;
	memset(event_valid, 1, event_noti_max_size+1);
	while (event_noti_rd_idx != event_noti_wr_idx) {
		/* read current retry count from RDB variable */
		sprintf((char *)rdb_name_buf, "%s.%d.retry_cnt", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		GET_P_THEN_INCREMENT_RD_IDX;
		event_valid[event_noti_rd_idx] = atoi(p);
		syslog(LOG_DEBUG, "current retry count[%d] = %s", event_noti_rd_idx, p);
		if (event_valid[event_noti_rd_idx] == 0) {
			syslog(LOG_DEBUG, "skip already sent index[%d]", event_noti_rd_idx);
			event_noti_rd_idx++; if (event_noti_rd_idx >= event_noti_max_size) event_noti_rd_idx = 0;
			continue;
		}

		/* read event type from RDB variable */
		sprintf((char *)rdb_name_buf, "%s.%d.type", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		GET_P_THEN_INCREMENT_RD_IDX;
		event_type = atoi(p);

		/* read event notification client list from RDB variable
		 * when elogger sends event noti, service.eventnoti.event.[wr_idx].clients value is same as
		 * service.eventnoti.type.[evt_type].client. After successfully sending a event noti to a
		 * client, the client name is removed from service.eventnoti.event.[wr_idx].clients.
		 */
		sprintf((char *)rdb_name_buf, "%s.%d.clients", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		GET_P_THEN_INCREMENT_RD_IDX;
		strcpy(event_noti_clients, p);

		/* read event notification text from RDB variable */
		sprintf((char *)rdb_name_buf, "%s.%d.text", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		GET_P_THEN_INCREMENT_RD_IDX;

		syslog(LOG_DEBUG, "rd_idx %d, evt type %d, evt clients '%s', evt text '%s'",
				event_noti_rd_idx, event_type, event_noti_clients, p);

		send_event_notification(event_noti_rd_idx, (char *)&event_noti_clients[0]);

		sprintf((char *)rdb_name_buf, "%s.%d.clients", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		(void) rdb_setVal(rdb_name_buf, event_noti_clients);

		/* if succeed for all clients, then set retry count to 0 to mark or decrement retry count */
		if (strlen(event_noti_clients) == 0) {
			event_valid[event_noti_rd_idx] = 0;
		} else {
			event_valid[event_noti_rd_idx]--;
			if (event_valid[event_noti_rd_idx] == 0) {
				syslog(LOG_ERR, "failed for max retry numbers, give up this noti[%d]", event_noti_rd_idx);
			}
		}
		sprintf((char *)rdb_name_buf, "%s.%d.retry_cnt", RDB_EVENT_NOTI_EVENT_PREPIX, event_noti_rd_idx);
		sprintf((char *)rdb_val_buf, "%d", event_valid[event_noti_rd_idx]);
		(void) rdb_setVal(rdb_name_buf, rdb_val_buf);

		event_noti_rd_idx++; if (event_noti_rd_idx >= event_noti_max_size) event_noti_rd_idx = 0;
		syslog(LOG_DEBUG, "event_noti_rd_idx = %d, event_noti_wr_idx = %d", event_noti_rd_idx, event_noti_wr_idx);

		processed_noti_cnt++;
		if (processed_noti_cnt >= MAX_NOTI_CNT_PER_POLLING_PERIOD) {
			syslog(LOG_DEBUG, "processed %d in this polling period, remaining will be processed in next polling period", MAX_NOTI_CNT_PER_POLLING_PERIOD);
			break;
		}

	}

	/* advance event notificaton read pointer to retry next time from this index */
	while (rd_idx_backup != event_noti_wr_idx) {
		if (event_valid[rd_idx_backup] > 0) break;
		rd_idx_backup++; if (rd_idx_backup>= event_noti_max_size) rd_idx_backup = 0;
	}
	syslog(LOG_DEBUG, "advance event_noti_rd_idx to %d", rd_idx_backup);
	sprintf(rdb_val_buf, "%d", rd_idx_backup);
	(void) rdb_setVal(RDB_EVENT_NOTI_RD_IDX, rdb_val_buf);
	free(event_valid);
	if (processed_noti_cnt >= MAX_NOTI_CNT_PER_POLLING_PERIOD)
		return 1;
	else
		return (rd_idx_backup == event_noti_wr_idx);
}

int rdbfd=-1;
int running=1;
int verbosity=0;

static int init(int be_daemon)
{
	const char *p;

	if (be_daemon)
	{
		daemon_init(APPLICATION_NAME, NULL, 0, (verbosity? LOG_DEBUG:LOG_INFO));
		syslog(LOG_INFO, "daemonized");
	} else {
		openlog(APPLICATION_NAME ,LOG_PID | LOG_PERROR, LOG_LOCAL5);
		setlogmask(LOG_UPTO(verbosity? LOG_DEBUG:LOG_INFO));
	}

	#ifdef LOG_LEVEL_CHECK
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"loglevel check");
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"syslog LOG_ERR");
	syslog(LOG_INFO,"syslog LOG_INFO");
	syslog(LOG_DEBUG,"syslog LOG_DEBUG");
	#endif

	syslog(LOG_INFO, "initializing...");
	ensure_singleton(APPLICATION_NAME, LOCK_FILE);

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

	rdbfd=rdb_fd(rdb);

	/* initialize max noti event size */
	p=rdb_getVal(RDB_EVENT_NOTI_MAX_SIZE);
	if (p) event_noti_max_size = atoi(p);
	syslog(LOG_INFO, "event noti buffer max size = %d", event_noti_max_size);

	syslog(LOG_INFO, "initialized...");
	return 0;
err:
	release_resources(LOCK_FILE);
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
		"\t-d don't detach from controlling terminal (don't daemonise)\n"
		"\t-i without initial delay\n"
		"\n"
	);

	return 0;
}

/* wait for this delay before first notification in order to send notifications in order */
static int default_init_delay = 60;
int main(int argc,char* argv[])
{
	struct timeval tv;
	int retval, ret, be_daemon = 1, polling_int = 1, init_delay = 0;

	/* Parse Options */
	while ((ret = getopt(argc, argv, "vVdhi")) != EOF) {
		switch (ret) {
			case 'v': verbosity=1; break;
			case 'V': fprintf(stderr, "%s: build date / %s %s\n",APPLICATION_NAME, __TIME__,__DATE__); return 0;
			case 'd': be_daemon = 0; break;
			case 'i': default_init_delay = 1; break;
			case 'h':
			case '?': print_usage(argv); return 2;
		}
	}

	init(be_daemon);
	syslog(LOG_ERR,"default_init_delay = %d",default_init_delay);

	/* select loop */
	while(running) {
		tv.tv_sec = polling_int;
		tv.tv_usec = 0;
		retval = select(0, NULL, NULL, NULL, &tv);
		/* bypass if interrupted or exit if error condition */
		if (retval<0) {
			if(errno==EINTR) {continue;}
			syslog(LOG_ERR,"select() failed - %s",strerror(errno));
			goto err;
		}

		if (init_delay >= 0) {
			init_delay += polling_int;
			if (init_delay > default_init_delay) {
				syslog(LOG_ERR,"default_init_delay %ds passed, now start sending notification...",default_init_delay);
				init_delay = -1;
			} else {
				continue;
			}
		}

		/* polling event notification buffer */
		if (!polling_event_noti_buffer()) {
			polling_int++; if (polling_int > 10) polling_int = 1;
			syslog(LOG_ERR,"failed to send all notifications, increase polling_int to %d", polling_int);
		}
	}

	/* finish daemon */
	fini(LOCK_FILE);
	return 0;
err:
	return -1;
}

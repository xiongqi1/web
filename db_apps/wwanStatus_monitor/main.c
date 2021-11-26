/* 3G status Monitoring server */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "comms.h"
#include "log.h"
#include "rdb_ops.h"


#define DAEMON_NAME		"wwanStatus_monitor"
#define VERSION			"1.1.0.0"

const char shortopts[] = "p:vh";

static void sig_handler(int signum);
extern int main_loop(void);

void init_gParam(void) {
	gParam.g_running = 1;
	gParam.g_verbosity = 0;
	gParam.g_deviceSN = 0ULL;
	gParam.g_serverPort = DEFAULT_SERVER_PORT;
}

static void usage(char **argv)
{
	fprintf(stderr, "\n%s is used to monitor status of 3G wireless module\n", argv[0]);
	fprintf(stderr, "\nVersion: %s\n", VERSION);
	fprintf(stderr, "\nUsage: %s [-p serverPort] [-v] [-u] [-i ifname]\n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-p local TCP port the server is listening. Default value is %d\n", DEFAULT_SERVER_PORT);
	fprintf(stderr, "\t-v increase the verbosity\n");
	fprintf(stderr, "\t-h display this message\n");
	fprintf(stderr, "\n");
	exit(0);
}

static uint64_t generate_serialNum() {
	uint64_t	serialNum = 0ULL;
	
	FILE* fp = popen("echo \"$(rdb_get systeminfo.mac.eth0)_D9\" | passgen '%15n'", "r");

	if (!fp)
		return 0;

	char achBuf[64];
	if (fgets(achBuf, sizeof(achBuf), fp))
		serialNum = atoll(achBuf);

	fclose(fp);

	return serialNum;
}

static void sig_handler(int signum)
{
	SYSLOG_DEBUG("caught signal %d", signum);
	switch (signum)
	{
		case SIGHUP:
			break;

		case SIGUSR1:
			setlogmask(LOG_UPTO(LOG_DEBUG));
			break;
			
		case SIGUSR2:
			setlogmask(LOG_UPTO(LOG_ERR));
			break;

		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			gParam.g_running = 0;
			break;
	}
}

int main(int argc, char**argv) {
	int ret;

	init_gParam();
	gParam.g_deviceSN = generate_serialNum();

	while ((ret = getopt(argc, argv, shortopts)) != EOF) {
		switch (ret) {
			case 'p':
				gParam.g_serverPort = atoi(optarg);
				break;
			
			case 'v':
				gParam.g_verbosity++ ;
				break;
				
			default:
				usage(argv);
				break;
		}
	}
	

	openlog(DAEMON_NAME, LOG_PID , LOG_LOCAL5);
	setlogmask(LOG_UPTO(gParam.g_verbosity + LOG_ERR));

	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);

	if(rdb_open_db() < 0)
	{
		SYSLOG_ERR("ERROR! failed to open database driver");
		goto error;
	}

	SYSLOG_DEBUG("Generated Device Serial Number= %llu", gParam.g_deviceSN);

	main_loop();
	
error:
	rdb_close_db();
	closelog();
	SYSLOG_DEBUG("exiting ... ");

	return ret;
}
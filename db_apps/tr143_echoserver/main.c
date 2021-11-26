 /* UDP Echo server */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "comms.h"
#include "log.h"
#include "rdb_event.h"



const char shortopts[] = "s:p:c:v?";

#define DAEMON_NAME       "tr143_echoserver"
#define VERSOIN	"1.0.0.0"

#define DEFAULT_ALLOWED_CLIENT_ADDR  "0.0.0.0"
#define DEFAUTL_SERVER_ADDR	"0.0.0.0"
#define DEFAUTL_SERVER_PORT	5000
#define MAX_PACKET_SIZE		65505


static void sig_handler(int signum);
extern int main_loop(TParameters *pParameters);

TParameters g_parameters;

void init_gparameters(void) {
	g_parameters.m_running = 0;
	g_parameters.m_force_rdb_read = 0;
	g_parameters.m_verbosity = 0;
}

void usage(char **argv)
{
	fprintf(stderr, "\n%s is used to ping ACS server\n", argv[0]);
	fprintf(stderr, "\nVersion: %s\n", VERSOIN);
	fprintf(stderr, "\nUsage: %s -s <serverIP> [-p serverPort] [-c AllowedclientAddr] [-v]  \n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-s local IPv4 address the server is binding to\n");
	fprintf(stderr, "\t-p local UDP port the server is listening(UDPPort in tr069 profile). Default value is 5000\n");
	fprintf(stderr, "\t-a Allowed Client IPv4 address(SourceIPAddress in tr069 profile).\n");
	fprintf(stderr, "\t-v increase the verbosity\n");
	fprintf(stderr, "\t-? display this message\n");
	fprintf(stderr, "\n");
}

int main(int argc, char**argv) {
	int ret;
	char * serverIP = DEFAUTL_SERVER_ADDR;
	int serverPort = DEFAUTL_SERVER_PORT;
	char * allowedIP = DEFAULT_ALLOWED_CLIENT_ADDR;
	int retValue = 0;

	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 's':
				serverIP = optarg;
				break;
			case 'p':
				serverPort = atoi(optarg);
				break;
			case 'a':
				allowedIP = optarg;
				break;
			case 'v':
				g_parameters.m_verbosity++ ;
				break;
			case '?':
				usage(argv);
				return 2;
		}
	}

    // initialize temporary logging
	openlog(DAEMON_NAME, LOG_PID , LOG_LOCAL5);
	setlogmask(LOG_UPTO(g_parameters.m_verbosity + LOG_ERR));

    /* handle signals and daemonize */
    //SYSLOG_DEBUG("setting signal handlers...");
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);

    // initialize rdb
	retValue = rdb_init(1);
	if(retValue != 0)
	{
		SYSLOG_ERR("ERROR! cannot initialize rdb");
		goto lab_end;
	}

	retValue = open_rdb();
	if(retValue != 0)
	{
		SYSLOG_ERR("ERROR! cannot subcrible rdb");
		goto lab_end;
	}

	main_loop(&g_parameters);
	
lab_end:
	close_rdb();
	closelog();
	SYSLOG_DEBUG("exiting ... ");

	return(retValue);
}

static void sig_handler(int signum)
{
	//pid_t	pid;
	//int stat;

	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notfication via SIGHUP. So we don't whinge
	on SIGHUP. */
//	if (signum != SIGHUP)
//		SYSLOG_DEBUG("caught signal %d", signum);

	switch (signum)
	{
		case SIGHUP:
			break;

		case SIGUSR1:
// 			g_parameters.m_verbosity++;
			setlogmask(LOG_UPTO(LOG_DEBUG));
			break;
			
		case SIGUSR2:
			if(g_parameters.m_verbosity > 0)
// 				g_parameters.m_verbosity--;
			setlogmask(LOG_UPTO(LOG_ERR));
			break;

		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			SYSLOG_DEBUG("caught signal %d", signum);

			g_parameters.m_running = 0;
			break;

		case SIGCHLD:
			break;
	}
}


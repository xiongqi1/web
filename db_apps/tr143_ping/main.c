#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include "log.h"
#include "ping.h"
#include "rdb_event.h"
#include "utils.h"
#include "main_loop.h"
#include "luaCall.h"

#define DAEMON_NAME       "tr143_ping"
#define LOCK_FILE	"/var/lock/" DAEMON_NAME



#define LOCAL_LUA_IFUP_SCRIPT 	"tr143_ifup.lua"
#define LOCAL_LUA_IFDOWN_SCRIPT 	"tr143_ifdown.lua"


#define LUA_IFUP_SCRIPT 	"/usr/bin/tr143_ifup.lua"
#define LUA_IFDOWN_SCRIPT 	"/usr/bin/tr143_ifdown.lua"

#define DEFAULT_INTERVAL		1000 //ms
#define DEFAULT_STRAGGLERTIMEOUT	5000 //ms

#define VERSOIN	"2.2.0.0"
#define DEFAULT_PACKET_COUNT	10

/// 1.0.1.0 initial version
/// 1.0.2.0 add more command line option
/// 1.0.3.0 add T option, change full path for script
/// 1.0.4.0 change all rdb variable in low case
/// 1.0.5.0 fix problem in tr143_ifup.lua, add debug flags
/// 1.0.6.0 multi session support
/// 1.0.7.0 update command line
/// 1.0.8.0 add multithread receiving to avoid any delay
/// 1.0.9.0 make program terminate properly when ctl^C or server stop
/// 1.0.10.0 IPDV error when packet arrive in wrong order, or integer turnover
///			replace  IPDV min/avg/max with IPDV jitter min/avg/max
/// 		dump RTT/IPDV1/IPDV2 raw data into file
///			add send/receive packets loss
/// 1.1.0.0 add console test mode
/// 1.1.1.0 add TC1_CIR and TC4_PIR rate policing for TR143 mode
/// 1.1.2.0 dump raw data
/// 1.1.3.0 1) add smart waiting for first packet,
///         2) change raw data format
///         3) change debug message report
/// 1.1.4.0 try to reduce the algorithm execution impact on network performance measurement
/// 1.1.5.0 fix the bug in count send/receive loss
/// 1.1.6.0 add interface option 5, and set it as default
///         5--use same name of interface and keep created one
/// 1.1.7.0 synchronize change of file luaCall.c in tr143_http
/// 1.2.0.0 redesign RTD algorithm, add extra field, rdb to trace packet loss.
/// 1.3.0.0 replace syslog with cdcs_syslog, update for new RDB
/// 2.0.0.0 introduce diagnostics.udpecho.x.changed
/// 2.1.0.0 fix command line problem
/// 2.2.0.0 sync with WNTDv2 source


extern int     opterr ,             /* if error message should be printed */
optind ,             /* index into parent argv vector */
optopt,                 /* character checked for validity */
optreset;               /* reset getopt */
extern char    *optarg;                /* argument associated with option */


static void sig_handler(int signum);

TParameters g_parameters;
int			g_loglevel=0;

#define RDB_PREFIX_DIAGNOSTICS "diagnostics."

#define RDB_PREFIX_INSTALLDIAG "installdiag."

char rdb_prefix[40];

void install_signal_handler(void)
{
	//NTCLOG_DEBUG("setting signal handlers...");
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGPWR, sig_handler);
}

int
main (int argc, char **argv)
{

    int retValue = 0;
    int c;
	int tmp;
    char *ifup_script = LUA_IFUP_SCRIPT;
	char *ifdown_script = LUA_IFDOWN_SCRIPT;
	char *hostname = NULL;
	char *if_ip =NULL;
	char *if_mask =NULL;
	char *if_route =NULL;
	int   if_tag =-1;
	int   if_cos=-1;

    int port = -1;
	int PingInterval_ms = -1;
	int StragglerTimeout_ms = -1;
	int packetCount = DEFAULT_PACKET_COUNT;
	int packetSize = -1;
    opterr = 0;
	memset(&g_parameters, 0, sizeof(g_parameters));
	strcpy(rdb_prefix, RDB_PREFIX_DIAGNOSTICS);
#ifdef _PLATFORM_Arachnid
	g_parameters.m_if_build_method =5;
#endif

	g_parameters.m_if_ops =IF_DOWN_STOP;

    while ((c = getopt (argc, argv, "v:i:1ec:z:w:l:p:dkI:S:M:T:C:B:Dh")) != -1)
    {


        switch (c)
        {
		case 'v': //  "\t-v verbosity -- 	default:0 - no log\n"
                  //  "\t                 use -v 7 to enable runtime error logging\n"
                  //  "\t                 use -v 8 to enable debug level logging \n"
			g_loglevel = Atoi(optarg, &tmp);
			if(!tmp || g_loglevel < 0)
			{
				fprintf (stderr, "Invalid option -v\n");
				exit(1);
			}
			break;
		case 'i': //            "\t-i id -- session ID, it is embedded in rdb variable(>= 0)\n"
			g_parameters.m_session_id = Atoi(optarg, &tmp);
			if(!tmp || g_parameters.m_session_id < 0)
			{
				fprintf (stderr, "Invalid option -i\n");
				exit(1);
			}
			break;
		case '1': //'"\t-1 --- TR143 console test mode\n"
			g_parameters.m_console_test_mode=1;
			strcpy(rdb_prefix, RDB_PREFIX_INSTALLDIAG);
			break;
		case 'e': //"\t-e --- remove rdb variable after program exit\n"
			g_parameters.m_remove_rdb=1;
			break;
		case 'c': //"\t-c count --- Stop after sending count ECHO_REQUEST packets\n"
			packetCount = Atoi( optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -%cc\n",c);
				exit(1);
			}

			break;
        case 'z': //            "\t-z size --- Packet size\n"
			packetSize= Atoi( optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -%c\n", c);
				exit(1);
			}
			break;

        case 'w': //"\t-w msec --Wait interval between sending each packet(default %dms)\n"
        	PingInterval_ms = Atoi( optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -%c\n", c);
				exit(1);
			}
			break;

		case 'l': //"\t-l msec --Wait after last packet is sent for replies to arrive\n"
                  //  "\t\t		(Default: %dms)"
            StragglerTimeout_ms = Atoi( optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -%c\n", c);
				exit(1);
			}
			break;
        case 'p': //            "\t-p port --port to ping\n"
            port = Atoi( optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -%c\n", c);
				exit(1);
			}
			break;
		case 'd'://	"\t-d --dump raw data\n"
			g_parameters.m_dump_raw_data =1;
			break;
		case 'k'://                    "\t-k --	keep the created interface\n"
			g_parameters.m_if_ops = IF_DOWN_NONE;
			break;
        case 'I': //        "\t-I ip --interface address\n"
			if_ip = optarg;
			break;


		case 'M': //			"\t-M ip --interface mask\n"
			if_mask = optarg;
			break;
		case 'S':	//		"\t-S ip --SmartEdgeAddress\n"
			if_route = optarg;
			break;
		case 'T': //	"\t-T tag--MPLSTag\n"
			if_tag = Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -T\n");
				exit(1);
			}
			break;
		case 'C':// '"\t-C value--CoS\n"
			if_cos = Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -C\n");
				exit(1);
			}
			break;
		case 'B':	//	"\t-B -- The method to build interface with requested address\n"
					//"\t   	0 -- create new interface(delete all conflicted interface)\n"
					//"\t   	1 -- use same if_name interface\n"
					//"\t   	2 -- use similary if_name interface\n"
					//"\t   	3 -- use any interface( with requested address)\n"
			g_parameters.m_if_build_method =  Atoi(optarg, &tmp);
			if(!tmp ||g_parameters.m_if_build_method>= IF_BUILD_MAX)
			{
				fprintf (stderr, "Invalid option -B\n");
				exit(1);
			}
			break;
		case 'D': //'"\t-D -- script debug\n"
			g_parameters.m_script_debug=1;
			break;
        case '?':
            if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                         "Unknown option character `\\x%x'.\n",
                         optopt);
            return 1;
        case 'h':
            fprintf(stdout, "%s is used to ping ACS server\n"
                    "Version: %s\n"
                    "Usage: [-v verbosity] [-c count] [-z packSize] [-i interval] [-l straggler]\n"
                    " [-I interfaceaddress] [-M interfacemask] [-S smartedgeaddress]\n"
                    " [-p serverport] serveraddress\n"
                    "Where the servseraddress is UDPEcho plus Server IP address\n"
                    "When serveraddress is specified, it start command line test mode\n"
                    "The options are: \n"
					"\t-v verbosity -- default:0 - Log to syslog only \n"
                    "\t                if it is non 0 - Log to syslog and stderr \n"
                    "\t                use -v 8 to enable debug level logging \n"
                    "\t-i id -- (>= 0) session ID, it is embedded in rdb variable\n"
                    "\t          default:  no ID embedded in rdb variable \n"
					"\t-1 --- TR143 console test mode\n"
                    "\t-e --- remove rdb variable after program exit\n"
                    "\t-c count --- Packet count\n"
                    "\t-z size --- Packet size(default:20)\n"
                    "\t-w msec --Wait interval between sending each packet(default %dms)\n"
                    "\t-l msec --Wait after last packet is sent for replies to arrive\n"
                    "\t	        (Default: %dms)\n"
                    "\t-p port--Server Port.(Default:5000)\n"
                    "\t-k --	keep the created interface\n"
                    "\t-d --	dump raw data into '/NAND/udpecho.rawdata.txt'\n"
                    "\t-I ip --interface address\n"
					"\t-M ip --interfacemask\n"
					"\t-S ip --smartedgeaddress\n"
					"\t-T value--mplstag\n"
					"\t-C value--Cos value\n"
					"\t-B -- The method to build interface with requested address\n"
#ifdef _PLATFORM_Arachnid
					"\t   	0 -- create new interface, delete duplicated one\n"
					"\t   	1 -- use same name of interface\n"
					"\t   	2 -- use similar name of interface\n"
					"\t   	3 -- use same address of interface\n"
					"\t   	4 -- use same address of interface, delete at exit\n"
					"\t   	5 -- (default)use same name of interface and keep created one\n"
#else
					"\t   	0 -- (default)create new interface, delete duplicated one\n"
					"\t   	1 -- use same name of interface\n"
					"\t   	2 -- use similar name of interface\n"
					"\t   	3 -- use same address of interface\n"
					"\t   	4 -- use same address of interface, delete at exit\n"
					"\t   	5 -- use same name of interface and keep created one\n"
#endif
					"\t-D -- script debug\n"
                    "\t-h -- display this screen\r\n",
                    argv[0], VERSOIN, DEFAULT_INTERVAL, DEFAULT_STRAGGLERTIMEOUT);
        default:
            exit(1);
        }

    }

    if(argv[optind] )
    {
    	hostname = argv[optind];
    	g_parameters.m_cmd_line_mode=1;
    }
	if(argv[0][0]=='.')
	{
		ifup_script = LOCAL_LUA_IFUP_SCRIPT;
		ifdown_script = LOCAL_LUA_IFDOWN_SCRIPT;
	}
	
	if(g_parameters.m_if_build_method == 5 && g_parameters.m_if_ops == IF_DOWN_STOP)
	{
		g_parameters.m_if_ops=IF_DOWN_CLEAR;
	}

    /*
	 * By default we log up to WARNING to syslog and nothing to stderr
	 * if loglevel is set via the command line then
	 * we log up to both syslog and stderr up to the specified level
	 */

	if (g_loglevel == 0)
		NTCLOG_INIT(DAEMON_NAME, 0, LOG_NOTICE, LOG_NONE);
	else
		NTCLOG_INIT(DAEMON_NAME, 0, g_loglevel-1, g_loglevel-1);

	NTCLOG_NOTICE("starting ...");


    /* handle signals and daemonize */
	install_signal_handler();
  

    // initialize rdb
    retValue = rdb_init(g_parameters.m_session_id, 1);
    if(retValue != 0)
    {
        NTCLOG_ERR("ERROR! cannot initialize rdb");
        goto lab_end;
    }

	retValue = open_rdb(g_parameters.m_session_id,
						RDB_VAR_SUBCRIBE);
	if(retValue != 0)
    {
        NTCLOG_ERR("ERROR! cannot subcrible rdb");
        goto lab_end;
    }


	// set command line options into RDB parameters
	packetsize2rdb(g_parameters.m_session_id, packetSize);
	host2rdb(g_parameters.m_session_id, hostname);
	port2rdb(g_parameters.m_session_id, port);
	interval_timeout2rdb(g_parameters.m_session_id, PingInterval_ms);
	straggler_timeout2rdb(g_parameters.m_session_id, StragglerTimeout_ms);
	if_ip2rdb(g_parameters.m_session_id, if_ip);
	if_mask2rdb(g_parameters.m_session_id, if_mask);
	if_routr2rdb(g_parameters.m_session_id, if_route);
	if_MPLSTag2rdb(g_parameters.m_session_id, if_tag);
	if_Cos2rdb(g_parameters.m_session_id, if_cos);
	// only in commandline mode, the test starts immediately.
	if(g_parameters.m_cmd_line_mode)
	{
		// historically the command line need set packetcount
		packetcount2rdb(g_parameters.m_session_id, packetCount);
		rdb_set_i_str(Diagnostics_UDPEcho_DiagnosticsState, g_parameters.m_session_id, "Requested");
		rdb_set_i_str(Diagnostics_UDPEcho_Changed, g_parameters.m_session_id, "");
	}

	g_parameters.m_ifup_script = ifup_script;
	g_parameters.m_ifdown_script = ifdown_script;


	main_loop(&g_parameters);


lab_end:

	rdb_end(g_parameters.m_session_id, g_parameters.m_remove_rdb);

	close_rdb();

	if( g_parameters.m_remove_rdb && g_parameters.m_stats_name[0])
	{
		remove(g_parameters.m_stats_name);
	}

	NTCLOG_CLOSE;

    NTCLOG_NOTICE("exiting ... ");


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
//		NTCLOG_DEBUG("caught signal %d", signum);

	switch (signum)
	{
		case SIGHUP:
			break;

		case SIGUSR1:
			//update_debug_level();
			break;

		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			NTCLOG_NOTICE("caught signal %d", signum);

			g_parameters.m_running = 0;
			break;

		case SIGCHLD:
			break;
	}
}

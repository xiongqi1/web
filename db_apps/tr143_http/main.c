#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include "log.h"

#include "http.h"
#include "rdb_event.h"
#include "utils.h"
#include "main_loop.h"
#include "luaCall.h"

#define DAEMON_NAME       "tr143_http"
#define LOCK_FILE	"/var/lock/" DAEMON_NAME

#define LOCAL_LUA_IFUP_SCRIPT 	"tr143_ifup.lua"
#define LOCAL_LUA_IFDOWN_SCRIPT 	"tr143_ifdown.lua"

#define LUA_IFUP_SCRIPT 	"/usr/bin/tr143_ifup.lua"
#define LUA_IFDOWN_SCRIPT 	"/usr/bin/tr143_ifdown.lua"



//"http://tr143.tidalfusion.com.au:5000/?size=500"
#define DEFAULT_URL	"http://59.167.232.89:80/?size=500"

#define DEFAULT_PORT	"5000"

#define CONNECT_TIMEOUT	10000
#define SESSION_TIMEOUT  30000// Timeout after 30 seconds stall on recv

#define VERSION	"2.3.0.0"

/// 1.0.1.0 initial version
/// 1.0.2.0 add more command line option
/// 1.0.3.0 add T option, change full path for script
/// 1.0.4.0 change all rdb variables in low case
/// 1.0.5.0 fix problem in tr143_ifup.lua, add debug flags
/// 1.0.6.0 multi session support
/// 1.0.7.0 update cli option
/// 1.0.8.0 update total byte calculation, extract them from interface instead of curl
/// 1.0.9.0 set MTU based on 'unid.max_frame_size' -22
/// 1.1.0.0 add console test mode, calculue average throughput for console test mode
/// 1.1.1.0 add maxthroughput and minthroughput
///			estimate max/min throughput from total bytes of data(send/recv)
/// 1.1.2.0 add TC1_CIR and TC4_PIR rate policing for TR143 mode
/// 1.1.3.0 change debug log
/// 1.1.4.0 when no data receive for session timeout, terminate
/// 1.1.5.0 add interface option 5, and set it as default
/// 		 5--use same name of interface and keep created one
/// 1.1.6.0 fix float point error on small size of downloading
/// 1.2.0.0 implment function to retrieve raw RX/TX.
///			check upload size after finishing.
///			report sample throughput from raw RX/TX
/// 1.2.1.0 throughput issue, remove first sample
/// 1.2.2.0 change upload.TC4_PIR to 5,500,000Mbps
/// 1.2.3.0 max/min throughput issue
/// 1.2.4.0 test is too short, throughput cannot get calculated issue
/// 1.2.5.0 filter out some invalid URL before do NSLookup
/// 1.3.0.0 synchronize changes for tr143_ping
/// 1.4.0.0 replace syslog with cdcs_syslog, update for new RDB
/// 2.0.0.0 introduce diagnostics.[download|upload].x.changed
/// 2.1.0.0 fix command line problem
/// 2.2.0.0 sync with source in WNTDv2
/// 2.3.0.0 avoid abnormal min and max throughput value


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
	int filesize =-1;
    char *url= NULL;
	char *if_ip =NULL;
	char *if_mask =NULL;
	char *if_route =NULL;
	int   if_tag =-1;
	int   if_cos=-1;

	char *ifup_script = LUA_IFUP_SCRIPT;
	char *ifdown_script = LUA_IFDOWN_SCRIPT;

	strcpy(rdb_prefix, RDB_PREFIX_DIAGNOSTICS);

    opterr = 0;
	memset(&g_parameters, 0, sizeof(g_parameters));

	g_parameters.m_connect_timeout_ms= CONNECT_TIMEOUT;
	g_parameters.m_session_timeout_ms= SESSION_TIMEOUT;
	g_parameters.m_session_id =0;
	g_parameters.m_enable_sample_window =1;
#ifdef _PLATFORM_Arachnid
	g_parameters.m_if_build_method =5;
#endif
	g_parameters.m_if_ops =IF_DOWN_STOP;

    while ((c = getopt (argc, argv, "v:i:edu1z:c:s:t:kI:M:S:T:C:B:Dh")) != -1)
    {


        switch (c)
        {
		case 'v': //  "\t-v verbosity -- 	default:0 - no log\n"
                  //  "\t                 use -v 7 to enable runtime error logging\n"
                  //  "\t                 use -v 8 to enable debug level logging \n"
			g_loglevel = Atoi(optarg, &tmp);
			if( !tmp || g_loglevel < 0)
			{
				fprintf (stderr, "Invalid option -v\n");
				exit(1);
			}
			break;
        case 'i': //            "\t-i id -- session ID, it is embedded in rdb variable(>= 0)\n"
			g_parameters.m_session_id = Atoi(optarg, &tmp);
			if(!tmp || g_parameters.m_session_id <= 0)
			{
				fprintf (stderr, "Invalid option -i\n");
				exit(1);
			}
			break;
        case 'e': //"\t-e --- remove rdb variable after program exit\n"
			g_parameters.m_remove_rdb=1;
			break;

		case 'd': //"\t-d --- enable download test\n"
			g_parameters.m_enabled_function |=RDB_GROUP_DOWNLOAD;
			break;
		case 'u': //'"\t-u --- enable upload test\n"
			g_parameters.m_enabled_function |=RDB_GROUP_UPLOAD;
			break;
		case '1':  //"\t-1 --- TR143 console test mode\n"
			g_parameters.m_console_test_mode =1;
			g_parameters.m_enable_sample_window =0;
			strcpy(rdb_prefix, RDB_PREFIX_INSTALLDIAG);
			break;
		case 'z': //'"\t-z size --- upload file size\n"
			filesize = Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -z\n");
				exit(1);
			}
			break;
        case 'c': //            "\t-c timeout--connect timeout(default " CONNECT_TIMEOUT "ms)\n"
			g_parameters.m_connect_timeout_ms = Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -c\n");
				exit(1);
			}
			break;
        case 's': //        "\t-s timeout--http session timeout(default ##SESSION_TIMEOUT## ms)\n"
			g_parameters.m_session_timeout_ms= Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -s\n");
				exit(1);
			}
			break;
		case 'k'://                    "\t-k --	keep the created interface\n"
			g_parameters.m_if_ops = IF_DOWN_NONE;
			break;
		case 't': //'"\t-t seconds -- time interval for sample window. default:5\n"
			g_parameters.m_sample_interval= Atoi(optarg, &tmp);
			if(!tmp)
			{
				fprintf (stderr, "Invalid option -w\n");
				exit(1);
			}
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
            fprintf(stdout, "%s is used to download/upload test on ACS server\n"
                    "Version: %s\n"
                    "Usage: [-v verbosity] [-i id] [-e] [-d ] [-u ] [-1]  [-z size]\n"
                    " [-c timout] [-s timeout] [-t seconds] [-k]\n"
					" [-I interfaceaddress] [-M interfacemask] [-S smartedgeaddress]\n"
					" [-T mplstag] [-C Cos] [-B 0-4 ] url\n"
					"Where the url  is the http download or upload URL\n"
					"When url is specified, it start command line test mode\n"
                    "The option is: \n"
					"\t-v verbosity -- default:0 - Log to syslog only \n"
                    "\t                if it is non 0 - Log to syslog and stderr \n"
                    "\t                use -v 8 to enable debug level logging \n"
                    "\t-i id -- ( > 0) session ID, it is embedded in rdb variable\n"
                    "\t          default:  no ID embedded in rdb variable \n"
                    "\t-e --- remove rdb variable after program exit\n"
                    "\t-d --- enable download test\n"
                    "\t-u --- enable upload test\n"
					"\t-1 --- TR143 console test mode\n"
                    "\t-z size --- upload file size\n"
                    "\t-c timeout--connect timeout(default %d ms)\n"
                    "\t-s timeout--http session timeout(default %d ms)\n"
                    "\t            0  -- wait forever)\n"
                    "\t-t seconds -- moving window size in seconds. default:5\n"
                    "\t-k --	keep the created interface\n"
					"\t-I ip --interface address\n"
					"\t-M ip --interface mask\n"
					"\t-S ip --smartedgeaddress\n"
					"\t-T mplstag -- mpls-label\n"
					"\t-C CoS  -- default rx/tx CoS\n"
					"\t-B -- The method to build interface with requested address\n"
#ifdef _PLATFORM_Arachnid
					"\t   	0 -- create new interface,delete conflicted ones\n"
					"\t   	1 -- use same name of interface\n"
					"\t   	2 -- use similar name of interface\n"
					"\t   	3 -- use same address of interface\n"
					"\t   	4 -- use same address of interface, delete at exit\n"
					"\t   	5 -- (default)use same name of interface and keep created one\n"
#else
					"\t   	0 -- (default)create new interface,delete conflicted ones\n"
					"\t   	1 -- use same name of interface\n"
					"\t   	2 -- use similar name of interface\n"
					"\t   	3 -- use same address of interface\n"
					"\t   	4 -- use same address of interface, delete at exit\n"
					"\t   	5 -- use same name of interface and keep created one\n"
#endif

					"\t-D -- script debug\n"
                    "\t-h -- display this screen\r\n"
                    , argv[0], VERSION,CONNECT_TIMEOUT,SESSION_TIMEOUT);
        default:
            exit(1);
        }

    }
	if(g_parameters.m_enabled_function == 0)
	{
		  fprintf(stdout, "Either -d or -u must be specified\n");
		  exit(1);
	}

    if(argv[optind] )
    {
    	url = argv[optind];
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
    retValue = rdb_init(g_parameters.m_session_id, g_parameters.m_enabled_function, 1);
    if(retValue != 0)
    {
        NTCLOG_ERR("ERROR! cannot initialize rdb");
        goto lab_end;
    }

	retValue = open_rdb(g_parameters.m_session_id,
						g_parameters.m_enabled_function,
						RDB_VAR_SUBCRIBE);
	if(retValue != 0)
    {
        NTCLOG_ERR("ERROR! cannot subcrible rdb");
        goto lab_end;
    }


	if(g_parameters.m_enabled_function &RDB_GROUP_DOWNLOAD)
	{

		url_download2rdb(g_parameters.m_session_id, url);

		ifd_ip2rdb(g_parameters.m_session_id, if_ip);
		ifd_mask2rdb(g_parameters.m_session_id, if_mask);
		ifd_routr2rdb(g_parameters.m_session_id, if_route);
		ifd_MPLSTag2rdb(g_parameters.m_session_id, if_tag);
		ifd_Cos2rdb(g_parameters.m_session_id, if_cos);
		if_sample_interval(g_parameters.m_session_id, g_parameters.m_sample_interval);
		// only in commandline mode, the test starts immediately.
		if(g_parameters.m_cmd_line_mode)
		{
			rdb_set_i_str(Diagnostics_Download_DiagnosticsState, g_parameters.m_session_id, "Requested");
			rdb_set_i_str(Diagnostics_Download_Changed, g_parameters.m_session_id, "");
		}
	}
	if(g_parameters.m_enabled_function &RDB_GROUP_UPLOAD)
	{
		url_upload2rdb(g_parameters.m_session_id, url);
		ifu_ip2rdb(g_parameters.m_session_id, if_ip);
		ifu_mask2rdb(g_parameters.m_session_id, if_mask);
		ifu_routr2rdb(g_parameters.m_session_id, if_route);
		ifu_size2rdb(g_parameters.m_session_id, filesize);
		ifu_MPLSTag2rdb(g_parameters.m_session_id, if_tag);
		ifu_Cos2rdb(g_parameters.m_session_id, if_cos);
		if_sample_interval(g_parameters.m_session_id, g_parameters.m_sample_interval);
		// only in commandline mode, the test starts immediately.
		if(g_parameters.m_cmd_line_mode)
		{
			rdb_set_i_str(Diagnostics_Upload_DiagnosticsState, g_parameters.m_session_id, "Requested");
			rdb_set_i_str(Diagnostics_Upload_Changed, g_parameters.m_session_id, "");
		}
	}


	g_parameters.m_ifup_script = ifup_script;
	g_parameters.m_ifdown_script = ifdown_script;

	main_loop(&g_parameters);


lab_end:

	rdb_end(g_parameters.m_session_id, g_parameters.m_enabled_function, g_parameters.m_remove_rdb);

	close_rdb();

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

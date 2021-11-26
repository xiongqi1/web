#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include "rdb_comms.h"
#include "parameters.h"
#include "session.h"
#include "daemon.h"
#include "utils.h"

#define DAEMON_NAME       "ethoamd"
//#define LOCK_FILE	"/var/lock/" DAEMON_NAME

//#define MY_MDA_DEVNAME		"br0"			/* device Name */
//#define MY_MDA_DEVNAME		"eth1"			/* device Name */
//#define MY_MDA_DEVNAME2		"eth0"			/* device Name */

#define VERSION "4.0.0.0"
/// 1.0.1.0 initial version
/// 1.0.2.0 add RMP, CCM
///			multiply RMP from command line
///			MDA and LMP configuration changed detection
///			bug fixed for unit test
/// 1.0.3.0 assoiated with mpls for interface changed,
///			RMP object index start from 1
///			command line for LMP MAC address
///			LMP MAC is overwritten only when it is invalid
/// 1.0.4.0 change to default syslog library
/// 		new AVCID to bridge interface algorithm
/// 1.0.5.0 change some default value
///			fix the direction problem,
/// 1.1.0.0 add Y1731 group of function, enable CCM
/// 1.2.0.0 merge with TMS 6.8 version
/// 1.2.1.0 reload parameter whenever peerMode is set
/// 1.2.2.0 initial startup problem. dynamically load lmpmac address
/// 1.2.3.0 add vid /vidtype int RDB watch list
/// 1.2.4.0 disable Y1731 part and recompile, Y1731 part will be released later
/// 1.2.5.0 make dot1ag.lmp.macadr writable
/// 1.2.6.0 change mda.changedif format. fix seqment fault in ethoam_ifup.lua
/// 1.2.7.0 the deleting interface problem.'
/// 1.3.0.0 port to TMS6.81
/// 1.3.1.0 solve LBM packet passing issue. TT1084
/// 2.0.0.0 support Multi MEP mode
/// 2.1.0.0 update  Local MAC RDB 'dot1ag.lmp.macadr' at first
/// 2.2.0.0 use systeminfo.mac.ethaddr instead of systeminfo.mac.eth0
/// 2.3.0.0 cannot detect bridge interface changes.
/// 2.4.0.0 dot1ag._index and dot1ag.rmp._index need to be persist
/// 3.0.0.0 Y1731 implementation on TMS6.81
/// 3.0.1.0 Update dot1ag.x.mda.status messages
/// 3.0.2.0 make  DMM/LBM/LTM restartable
/// 3.1.0.0 support both Antelope and Arachind
/// 3.1.1.0 the attached inteface is locked when API failed.
/// 3.1.2.0 add egress port to control LTM, LBM,DMM traffic direction
/// 3.2.0.0 support Y1731 SLM
/// 3.3.0.0 Antelope: new mac address assignment algorithm
/// 3.4.0.0 multi SLM session, TLV for SLM, LBM and DMM
/// 3.5.0.0 fix the multi sessions stability issue on WNTDV3.
/// 3.6.0.0 fix bug reported by Ericson CSR3456027 on AVC attached issue
/// 3.7.0.0 fix the sync issue with AVC
/// 3.8.0.0 implement dot1ag.{1-4}.changed to trigger parameters loading
/// 3.9.0.0 the issue on priority mode. TT38574
/// 4.0.0.0 issue related to dot1ag.{1-4}.changed

extern int     opterr ,             /* if error message should be printed */
optind ,             /* index into parent argv vector */
optopt,                 /* character checked for validity */
optreset;               /* reset getopt */
extern char    *optarg;                /* argument associated with option */


static void sig_handler(int signum);

#define DEFAULT_LMPID	100
#define DEFAULT_RMPID	0
#define DEFAULT_TIMEOUT	5000
#define DEFAULT_TTL	64
#define DEFAULT_INTERFACE	"eth1"
#define DEFAULT_COS		0
#define DEFAULT_MDLEVEL 2
#define DEFAULT_DIRECTION 1
#define DEFAULT_PORT	2

TParameters g_defparameters=
{	.m_used =1,
	.m_lmpid= DEFAULT_LMPID,
	.m_timeout= DEFAULT_TIMEOUT,
	.m_port = DEFAULT_PORT,
	.m_ttl =DEFAULT_TTL,
	.m_if_cos = DEFAULT_COS,
	.m_mda_mdlevel = DEFAULT_MDLEVEL,
	.m_direction = DEFAULT_DIRECTION,
	.m_port = DEFAULT_PORT,
};
TConfig		g_config;




Session		*g_session[MAX_SESSION];

#define CHECK_PARAM(tmp, c)\
	if(tmp<0) {\
		fprintf(stderr, "Invalid \"%c\" value\r\n", c);\
		return -1;\
	}

int
main (int argc, char **argv)
{
	int tmp;
	int err =0;
    int retValue = 0;
    int c;
	const char *mda_mdid =NULL;
	const char *mda_maid =NULL;
	int cmd_line_mode =0;
	int session_ID =0;
	int rmpid;
	int dot1ag_index_exist=0;
	// alloc for default session
	TParameters *pParametersList;
	TParameters *pParameters;
    opterr = 0;
    g_config.m_max_session =1;
	pParametersList = malloc(sizeof(TParameters)*MAX_SESSION);
    if(pParametersList ==NULL) 
    {
		fprintf (stderr,
					 "No Enough memory\n");
		return 1;    
    }
	memset(pParametersList, 0, sizeof(TParameters)*MAX_SESSION);
    pParameters = &pParametersList[0];
	memcpy(pParameters, &g_defparameters, sizeof(TParameters));
	

    while ((c = getopt (argc, argv, "S:B:TEI:i:l:r:s:y1:2:ct:d:p:o:C:D:A:L:V:Y:f:gv:eh")) != -1)
    {
		//printf("c=%c, optarg=%s\n", c, optarg);
        switch (c)
        {
        case 'S': //  "\t-S {ID	-- Multi MEP mode Start, ID must be(1-4), default: single MEP mode\n"
			session_ID = Atoi(optarg, &tmp);
			if(tmp<0 || (session_ID < 1 && session_ID >MAX_SESSION))
			{
				fprintf(stderr, "Invalid \"%c\" value\r\n", c);
				return -1;
			}
			pParameters = &pParametersList[session_ID];

			if (pParameters->m_used)
			{
			    fprintf(stderr, "Session has been defined already\r\n");
                return -1;
			}
			g_config.m_max_session= MAX_SESSION;
			memcpy(pParameters, &g_defparameters, sizeof(TParameters));
			break;

        case 'B': //'"\t-B packet_number -- commmand line mode, send LBM number packet immediately\n"
			pParameters->m_LBMsToSend = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->m_enableMEP = 1;
			g_config.m_run_once=1;
			pParameters->load_parameter =1;
			break;

		case 'T'://	"\t-T -- commmand line mode, send LTM packet immediately\n"
			pParameters->m_sendLTM =1;
			pParameters->m_enableMEP =1;
			g_config.m_run_once=1;
			pParameters->load_parameter =1;
			break;

		case 'E': //'"\t-E -- enable MEP\n"
			pParameters->m_enableMEP=1;
			pParameters->load_parameter =1;
			break;
		case 'I': //"\t-I AVCID	-- AVCID to bind \n"
			strncpy(pParameters->m_avcid, optarg, MAX_AVCID_LEN);
			pParameters->m_avcid[MAX_AVCID_LEN] =0;
			pParameters->set_avcid =1;
			cmd_line_mode =1;
			pParameters->load_parameter =1;
			break;
		case 'i': // '"\t-i interface -- network interface to bind\n"
			strcpy(pParameters->m_if_name, optarg);
			pParameters->set_ifname =1;
			cmd_line_mode =1;
			pParameters->load_parameter =1;
			break;
		case 'l': //	"\t-l lmpid -- (LBM/LTM) LMPID, default:100\n"
			pParameters->m_lmpid= Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;

		case 'r'://	"\t-r rmpid -- (LBM/LTM) RMPID, default:10\n"

			rmpid = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			{
				int i=0;
				if(rmpid <=0 || pParameters->m_rmpid_num >= MAX_RMP_OBJ_NUM)
				{
					tmp =-1;
					CHECK_PARAM(tmp, c);
				}
				for(i =0; i< pParameters->m_rmpid_num; i++)
				{
					if(pParameters->m_rmpid[i] == rmpid)
					{
						tmp =-1;
						CHECK_PARAM(tmp, c);
					}
				}
				pParameters->m_rmpid[pParameters->m_rmpid_num ++ ] = rmpid;
			}

			pParameters->load_parameter =1;
			break;

		case 's': //	"\t-s imeout -- (LBM/LTM) timeout for reply. default:5000 ms\n"
			pParameters->m_timeout = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;
		case 'y': //"\t-y    -- enable Y1731 protocol\n"
			pParameters->m_Y1731_Action = 1;
			CHECK_PARAM(tmp, c);
			pParameters->m_enableMEP =1;
			pParameters->load_parameter =1;
			break;
		case '1': //"\t-1 count -- send number of  DMM message\n"
			pParameters->m_sendDmm= Atoi(optarg, &tmp);

			CHECK_PARAM(tmp, c);
			pParameters->m_Y1731_Action =1;
			pParameters->load_parameter =1;
			break;
		case '2': // "\t-2 count -- send number of  SLM message\n"
			pParameters->m_sendSlm= Atoi(optarg, &tmp);

			CHECK_PARAM(tmp, c);
			pParameters->m_Y1731_Action =1;
			pParameters->load_parameter =1;
			break;

		case 'c': //'"\t-c -- enable CCM, default: disabled\n"
			pParameters->m_enableCCM =1;
			pParameters->m_enableMEP =1;
			pParameters->load_parameter =1;
			break;

		case 't'://	"\t-t set_ttl -- TTL value in the packet, default:1\n"
			pParameters->m_ttl = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;

		case 'd': //"\t-d direction -- LMP direction: 1 = up, 2(-1) = dn, 0 = MIP\n"
			pParameters->m_direction = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;
        case 'p': //            "\t-p port	-- LMP port, default:1\n"
			pParameters->m_port= Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;


		case 'o': //'"\t-o addr  -- target MAC address for LBM/LTM\n"
			tmp= str2MAC(pParameters->m_destmac, optarg);
			if(!tmp) tmp =-1;
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;

		case 'C'://			"\t-C value--cos\n"
			pParameters->m_if_cos = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;
		case 'D'://			"\t-D ID	-- MD string ID. default: MD1\n"
			mda_mdid = optarg;
			pParameters->load_parameter =1;
			break;
		case 'A'://			"\t-A ID	-- MA string ID. default: MA1\n"
			mda_maid = optarg;
			pParameters->load_parameter =1;
			break;
		case 'L'://			"\t-L level -- MD level. default:!\n"
			pParameters->m_mda_mdlevel = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->load_parameter =1;
			break;
		case 'V': //'"\t-V vid   -- LMP vid, default: same as MD level\n"
			pParameters->m_vid = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->set_vid=1;
			pParameters->load_parameter =1;
			break;
        case 'Y': // "\t-Y vidtype -- LMP vid type, 0--none, 1--CVLAN, 2-SVLAN\n"
			pParameters->m_vidtype = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			pParameters->set_vid=1;
			pParameters->load_parameter =1;
			break;
		case 'f': //		"\t-f TLV_data -- tlv data file\n"
			pParameters->m_tlv_data_file = optarg;
			break;
			
			
        case 'g': //            "\t-g 		-- script debug\n"
			g_config.m_script_debug = 1;
			break;


		case 'v': /*"\t-v verbosity_level, default:0 - Log to syslog only \n"
                    "\t                       if vebosity is non 0 - Log to syslog and stderr \n"
                    "\t                       use -v 8 to enable debug level logging \n"*/
			g_config.m_verbosity = Atoi(optarg, &tmp);
			CHECK_PARAM(tmp, c);
			break;
		case 'e': //"\t-e --- remove rdb variable after program exit\n"
			g_config.m_remove_rdb=1;
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
            fprintf(stdout, "%s is a daemon program to test networks OAM\n"
                    "Version: %s\n"
                    "Usage: [-SBTEilrstdpmoCDALgveh]  AVCID\n"
                    "The option is: \n"
                    "\t-S {ID   -- Multi MEP mode Start, ID must be(1-4).\n"
                    "\t             if not specified: it is in single MEP mode\n"
                    "\t-B number-- command line mode, send number of LBM packet immediately\n"
                    "\t             if '-v' not set, it will output full debug message\n"
                    "\t-T       -- command line mode, send LTM packet immediately\n"
                    "\t             if '-v' not set, it will output full debug message\n"
                    "\t-E       -- enable MEP\n"
   					"\t-I AVCID	-- AVCID to bind \n"
   					"\t-i interface -- network interface to bind\n"
   					"\t             \'AVCID\' will overwrite this option\n"
                    "\t-l lmpid -- (LBM/LTM/LMM/DMM..) LMPID, default:1\n"
                    "\t-r rmpid -- Create one or more RMP objects(<32) with RMPID='rmpid'\n"
                    "\t            (0, 1-8191), 0-- invalid rmpid(default)\n"
                    "\t-s ms    -- (LBM/LTM/LMM/DMM..) set timeout for reply. default:5000 ms\n"
#ifdef NCI_Y1731
                    "\t-y       -- enable Y1731 protocol\n"
                    "\t-1 count -- send number of  DMM message\n"
                    "\t-2 count -- send number of  SLM message\n"
#endif
                    "\t-c       -- enable CCM, default: disabled\n"
                    "\t-t ttl   -- TTL value in the packet, default:64\n"
                    "\t-d dir   -- LMP direction: 1 = up(default), 2 or -1 = dn, 0 = MIP\n"
                    "\t-p port  -- LMP port, default:1\n"
                    "\t-o MAC   -- target MAC address for LBM/LTM/LMM/DMM..\n"
                    "\t           -r(rmpid) will overwrite this option\n"

					"\t-C value -- user specified CoS to replace GRE default CoS \n"

					"\t-D ID    -- MD string ID. default: \"MD1\"\n"
					"\t-A ID    -- MA string ID. default: \"MA1\"\n"
					"\t-L level -- MD level. default:5\n"
                    "\t-V vid   -- LMP vid, default: 0\n"
                    "\t-Y vidtype -- LMP vid type, 0--none(default), 1--CVLAN, 2-SVLAN\n"
					
#if 0
                    "\t-f TLV_data -- tlv data file\n"
					"\t\t		SendID format(hex) \n"
					"\t\t		SendID: classtype, classid, mgmtDom, mgmtAdr\n"
					"\t\t		example:sendID: 10, 01020304, 1234, abcd\n"				
					"\t\t		Origanization data format(hex): \n"
					"\t\t		Org:oui, type, value\n"
					"\t\t		example:Org: 0102ff, 01, 1020304050a0b0\n"
#endif
					"\t\n"
					"\t }Multi MEP mode End\n"
					"\t\n"
                    "\t-g       -- script debug\n"
                    "\t-v log_level, default:0 - Log to syslog only \n"
                    "\t            if vebosity is non 0 - Log to syslog and stderr \n"
                    "\t            use -v 8 to enable debug level logging \n"
                    "\t-e       -- remove rdb variable after program exit\n"
                    "\t-h       -- display this screen\r\n",
                    argv[0], VERSION);
        default:
            exit(1);
        }

    }

    if(argv[optind] )
    {
    	strncpy(pParameters->m_avcid, argv[optind], MAX_AVCID_LEN);
    	pParameters->m_avcid[MAX_AVCID_LEN] =0;
		pParameters->set_avcid =1;
		cmd_line_mode =1;
		pParameters->load_parameter =1;

    }
	//2 -- start log
	if ( pParameters->m_LBMsToSend || pParameters->m_sendLTM )
	{ // must be in command line mode
		if(g_config.m_verbosity ==0)g_config.m_verbosity = 8;
	}

   /*
	 * By default we log up to WARNING to syslog and nothing to stderr
	 * if pParameters->m_verbosity is set via the command line then
	 * we log up to both syslog and stderr up to the specified level
	 */

	if (g_config.m_verbosity == 0)
		NTCLOG_INIT(DAEMON_NAME, 0, LOG_NOTICE, LOG_NONE);
	else
		NTCLOG_INIT(DAEMON_NAME, 0, g_config.m_verbosity-1, g_config.m_verbosity-1);

	NTCLOG_NOTICE("starting ...");

	/*
	 * If not running in debug mode daemonize
	 */

	if(g_config.m_verbosity == 0)
	{
		daemon_init(DAEMON_NAME, NULL, 1, LOG_NOTICE);
	}



    /* handle signals and daemonize */
    //3 -- NTCSLOG_DEBUG("setting signal handlers...");
    signal(SIGHUP, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGCHLD, sig_handler);

	//4 -- open rdb
	if ((rdb_open(RDB_DEVNAME, &g_rdb_session)) < 0) {
		NTCLOG_ERR("ERROR! cannot initialize rdb");
		goto lab_err;
	}

	//5 -- check DOT1AG_INDEX exist and create it if not
	dot1ag_index_exist = rdb_exist(DOT1AG_INDEX);

	//5.1 -- initialize and create all global rdb(include DOT1AG_INDEX)
	rdb_init_global();

	//5.2-- multi session id list check
	session_ID =0;
	if(g_config.m_max_session > 1)
	{
		int i;
		int idlist[MAX_SESSION];
		int newidcount =0;
		int overwrite_rdb =0;
		// map dot1ag._index into parameters array
		// 2 condition to overwrite exist rdb index
		// 1) dot1ag._index does not exist-- factory reset
		// 2) load parameter from command line
		int idcount = rdb_get_index_list(DOT1AG_INDEX, 0, idlist, MAX_SESSION);

		if(idcount < 0 || dot1ag_index_exist ==0 )
		{
			overwrite_rdb = 1;
		}

		for(i=1; i< MAX_SESSION ; i++)
		{
			if(pParametersList[i].load_parameter)
			{
				overwrite_rdb = 1;
				break;
			}
		}

		if(overwrite_rdb )
		{	
			//merge dot1ag._index and command line
			for(i=0; i<idcount ; i++)
			{
				pParametersList[idlist[i]].m_used =1;
			}
			// save parameters array into  dot1ag._index
			for(i=1; i< MAX_SESSION ; i++)
			{
				if(pParametersList[i].m_used )
				{
				
					idlist [newidcount++]=i;
				}
			}

			rdb_set_index_list(DOT1AG_INDEX, 0, idlist, newidcount);
		}
		else
		{
			// only setup sesssion from rdb dot1ag._index
			for(i=1; i< MAX_SESSION ; i++)
			{
				pParametersList[i].m_used  =0;
			}
			for(i=0; i<idcount ; i++)
			{
				pParametersList[idlist[i]].m_used =1;
			}
		
		}
		
		session_ID=1;  // session 0 -- single mep ,1-4 multi meps
	}
	
	//6 get local mac
	//_PLATFORM_Antelope function has been removed 

	// 7 for each session
	for(; session_ID < g_config.m_max_session; session_ID++)
	{
		Session *pSession;
		TParameters *pParameters = &pParametersList[session_ID];
		if(pParameters->m_used == 0) continue;
		
		
		//2) open this session
		retValue = open_session(&pSession, session_ID,  &g_config);
		if(retValue != 0)
		{
			NTCSLOG_ERR("ERROR! cannot open session");
			goto lab_err;
		}
		g_session[session_ID] = pSession;
		pSession->m_sessions = g_session;
		
		RMP_retrieve(pSession);
		
		//3) set up local mep mac

		// _PLATFORM_Antelope function has been removed 

		//3)build up RMP object
		if(pParameters->m_rmpid_num >0)
		{
			int i;
			// delete all old RMP RDB object
			RMP_del_all(pSession);
			//create new RMPID and RDB
			for(i =0; i< pParameters->m_rmpid_num ; i++)
			{
				err= RMP_set(pSession, i+1, pParameters->m_rmpid[i], 0);
				if(err)goto lab_err;
			}
		}
		
		//4) check LBM and LTM parameters
		//NTCSLOG_DEBUG("start set rdb\n");
		if(pParameters->m_LBMsToSend ||pParameters->m_sendLTM )
		{
			if ( !pParameters->set_avcid && ! pParameters->set_ifname)
			{
				NTCSLOG_ERR("'AVCID' or network interface is missing");
				goto lab_err;
			}
			if(ZEROCHK_MACADR(pParameters->m_destmac) && pParameters->m_rmpid_num ==0)
			{
				NTCSLOG_ERR("Destination MAC or RMPID is missing");
				goto lab_err;
			}
		}
		//if(!ZEROCHK_MACADR(pParameters->m_lmpmac))	RDB_SET_P1_MAC(LMP_macAdr, pParameters->m_lmpmac);

		// 5) write all parameter into RDB
		if (pParameters->load_parameter)
		{
			// lmp setting
			RDB_SET_P1_INT(LMP_CoS, pParameters->m_if_cos);
			if(pParameters->set_avcid) RDB_SET_P1_STR(MDA_AVCID, pParameters->m_avcid);
			RDB_SET_P1_INT(LMP_mpid, pParameters->m_lmpid);
			RDB_SET_P1_INT(LMP_direction, pParameters->m_direction);
			RDB_SET_P1_INT(LMP_port, pParameters->m_port);


			// MDA setting
			TRY_RDB_SET_P1_STR(MDA_MdIdType2or4, mda_mdid);
			TRY_RDB_SET_P1_STR(MDA_MaIdType2, mda_maid);
			RDB_SET_P1_INT(MDA_MdLevel, pParameters->m_mda_mdlevel);
			if(pParameters->set_vid)
			{
				RDB_SET_P1_INT(LMP_vid, pParameters->m_vid);
				RDB_SET_P1_INT(MDA_PrimaryVid, pParameters->m_vid);
				if (pParameters->m_vid && pParameters->m_vidtype ==0)pParameters->m_vidtype =1;
				RDB_SET_P1_INT(LMP_vidtype, pParameters->m_vidtype);
			}
		}
		// 5) notify the main_loop by RDB
		RDB_SET_P1_STR(DOT1AG_Changed, "");

		//6) check CCM. 
		
		if(!cmd_line_mode)
		{
			TRY_RDB_GET_P1_BOOLEAN(LMP_CCMenable, &pParameters->m_enableCCM);
		}
		// notify RCB changes to main_loop
		TRY_RDB_SET_P1_BOOLEAN(LMP_CCMenable, pParameters->m_enableCCM);

#ifdef NCI_Y1731
		//7) Check Y1731
		if (pParameters->m_Y1731_Action)
		{
			TRY_RDB_SET_P1_BOOLEAN(Y1731_Mda_Enable, 1);
#if 0			
			if (pParameters->m_sendLmm)
			{
				RDB_SET_P1_INT(Y1731_Lmm_rmpid, pParameters->m_rmpid[0]);
				if(!ZEROCHK_MACADR(pParameters->m_destmac)) RDB_SET_P1_MAC(Y1731_Lmm_destmac, pParameters->m_destmac);
			}
			else
			{
				RDB_SET_P1_INT(Y1731_Lmm_rmpid, 0);
			}
#endif			
			if (pParameters->m_sendDmm)
			{
				RDB_SET_P1_INT(Y1731_Dmm_rmpid, pParameters->m_rmpid[0]);
				if(!ZEROCHK_MACADR(pParameters->m_destmac)) RDB_SET_P1_MAC(Y1731_Dmm_destmac, pParameters->m_destmac);
			}
			else
			{
				RDB_SET_P1_INT(Y1731_Dmm_rmpid, 0);
			}
			if (pParameters->m_sendSlm)
			{
				RDB_SET_P1_INT(Y1731_Slm_rmpid, pParameters->m_rmpid[0]);
				if(!ZEROCHK_MACADR(pParameters->m_destmac)) RDB_SET_P1_MAC(Y1731_Slm_destmac, pParameters->m_destmac);
			}
			else
			{
				RDB_SET_P1_INT(Y1731_Slm_rmpid, 0);
			}
				
#if 0				
			TRY_RDB_SET_P1_BOOLEAN(Y1731_Lmp_AISForced, pParameters->m_Y1731_Action&Y1731_ACTION_FORCE_AIS);
			TRY_RDB_SET_P1_BOOLEAN(Y1731_Lmp_AISAuto, pParameters->m_Y1731_Action&Y1731_ACTION_AUTO_AIS);
			TRY_RDB_SET_P1_BOOLEAN(Y1731_Lmm_Send, pParameters->m_sendLmm);
#endif			
			TRY_RDB_SET_P1_INT(Y1731_Dmm_Send, pParameters->m_sendDmm);
			TRY_RDB_SET_P1_INT(Y1731_Slm_Send, pParameters->m_sendSlm);

		}
		else
		{
			if (pParameters->load_parameter)
			{
				TRY_RDB_SET_P1_BOOLEAN(Y1731_Mda_Enable, 0);
			}
			
		}

#endif
		//8) Send LBM/LTM
		if(pParameters->m_LBMsToSend)
		{
			TRY_RDB_SET_P1_INT(LBM_LBMsToSend, pParameters->m_LBMsToSend);
			if(pParameters->m_rmpid_num >0 )
			{
				RDB_SET_P1_INT(LBM_rmpid, pParameters->m_rmpid[0]);
			}
			else
			{
				RDB_SET_P1_INT(LBM_rmpid, 0);
			}
			RDB_SET_P1_INT(LBM_timeout, pParameters->m_timeout);
			if(!ZEROCHK_MACADR(pParameters->m_destmac)) RDB_SET_P1_MAC(LBM_destmac, pParameters->m_destmac);

		}
		else if (pParameters->m_sendLTM)
		{

			TRY_RDB_SET_P1_INT(LTM_send, pParameters->m_sendLTM);
			if(pParameters->m_rmpid_num >0)
			{
				RDB_SET_P1_INT(LTM_rmpid, pParameters->m_rmpid[0]);
			}
			else
			{
				RDB_SET_P1_INT(LTM_rmpid,0);
			}
			RDB_SET_P1_INT(LTM_timeout, pParameters->m_timeout);
			//if(priority) RDB_SET_P1_INT(LTM_priority, pParameters->m_priority);
			RDB_SET_P1_INT(LTM_ttl, pParameters->m_ttl);
			if(!ZEROCHK_MACADR(pParameters->m_destmac)) RDB_SET_P1_MAC(LTM_destmac, pParameters->m_destmac);
		}
		//9) release pParameters memory
		
	}// for each session, for(; session_ID < g_config.m_max_session; session_ID++)

	free(pParametersList);
	pParametersList =NULL;
	
	//8 -- main loog
	main_loop(&g_config);

lab_err:
	//9 -- cleanup
	if(pParametersList)free(pParametersList);

	close_all_session(&g_config);

	rdb_end_global(g_config.m_remove_rdb);

	rdb_close(&g_rdb_session);

	daemon_fini();

	closelog();


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
//		NTCSLOG_DEBUG("caught signal %d", signum);

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
			NTCLOG_DEBUG("caught signal %d", signum);
			signal(SIGHUP,  SIG_DFL);
			g_config.m_running = 0;
			break;

		case SIGCHLD:
			break;
	}
}

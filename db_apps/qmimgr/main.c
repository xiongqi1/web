#include "g.h"

static void usage(FILE* fd)
{
	fprintf(fd,
		"\nUsage: qmimgr [options]\n" \
				"\n Options:\n" \
				"\t-i instance (default: %d)\n" \
				"\t-p qmi port (default: %s)\n" \
				"\t-a AT port\n" \
				"\t-v verbosity level (default: %d)\n" \
				"\t   -v 2 : errors\n" \
				"\t   -v 3 : + normal operations\n" \
				"\t   -v 4 : + database activities\n" \
				"\t   -v 5 : + qmi activities\n" \
				"\t   -v 6 : + qmi traffic dump\n" \
				"\t   -v 7 : all\n" \
				"\t-l specify last USB port\n"
				"\t-d Disable daemonization\n"
				"\t-x launch without SMS feature\n"
				"\n"
				"\n * feature enable or disable options\n"
				"\t-t enable features (to work together with other managers)\n"
				"\t-f disable features (to work together with other managers)\n"
				"\n"

		"RDBs:\n"
			"\twwan.0.voice.command.ctrl : voice control command\n"
			"\twwan.0.voice.command.noti : voice notification\n",

    			instance, qmi_port, verbosity);
}

#ifdef MODULE_PRI_BASED_OPERATION
	char* rdb_module_priid;
	int vzw_pripub;
	//int isSynchronized;
	int count;
#endif

int main(int argc,char* argv[])
{
	int	opt;
	int termbysignal;

/*
	const char* abc="(2,\"Telstra Mobile\",\"Telstra\",\"50501\",2),(1,\"Telstra Mobile\",\"Telstra\",\"50511\",0)";
	int i=0;
	char buf[128];

	while(__strtok(abc,",","()",i++,buf,sizeof(buf))>=0) {
		printf("%s\n",buf);
	}

	exit(0);
*/

	// init minilib
	minilib_init();

	// set default configuration - /dev/cdc-wdm0
	qmi_port="/dev/qcqmi0";
	at_port=NULL;
	verbosity=LOG_OPERATION;
	verbosity_cmdline=0;
	instance=0;
	last_port=NULL;
	be_daemon=1;
	sms_disabled=0;

	// create featurehash
	if( init_feature()<0 ) {
		fprintf(stderr,"failed to initialize feature hash\n");
		exit(-1);
	}


	while ((opt = getopt(argc, argv, "xdi:a:p:v:l:Vht:f:?")) != EOF)
	{
		switch (opt)
		{
			case 't':
			case 'f':
				if( add_feature(optarg,opt=='t')<0 ) {
					fprintf(stdout,"failed to add feature - feature=%s,opt=%c\n",optarg,opt);
					exit(-1);
				}

				break;

			case 'v':
				verbosity=atoi(optarg);
				verbosity_cmdline=1;
				break;

			case 'i':
				instance = atoi(optarg);
				break;

			case 'a':
				at_port = optarg;
				break;


			case 'p':
				qmi_port = optarg;
				break;

			case 'l':
				last_port = optarg;
				break;

			case 'd':
				be_daemon=0;
				break;

			case 'x':
				sms_disabled=1; //disable SMS feature.
				break;

			case 'h':
				usage(stdout);
				exit(0);

			case 'V':
				fprintf(stdout,"version v%d.%d",QMIMGR_VERSION_MJ,QMIMGR_VERSION_MN);
				exit(0);

			case ':':
				fprintf(stderr,"missing argument - %c\n",opt);
				usage(stderr);
				exit(-1);

			case '?':
				fprintf(stderr,"unknown option - %c\n",opt);
				usage(stderr);
				exit(-1);

			default:
				usage(stderr);

				exit(-1);

				break;
		}
	}

	/* get string variant of instance */
	wwan_prefix_len=snprintf(wwan_prefix,WWAN_PREFIX_LENGTH,"wwan.%d.",instance);

	// by default, enable all feature if no option is specified
	if(is_enabled_feature(FEATUREHASH_ALL)<0) {
		if( add_feature(FEATUREHASH_ALL,1)<0 ) {
			fprintf(stdout,"failed to enable all feature by default\n");
			exit(-1);
		}
	}

	if (sms_disabled == 1) {
		change_feature(FEATUREHASH_CMD_SMS,0);
	}

	// init
	if( qmimgr_init()<0 ) {
		return -1;
	}

	#ifdef V_SMS_QMI_MODE_y
	/* nothing to do */
	#else
	if(is_enabled_feature(FEATUREHASH_CMD_SMS)) {
		fprintf(stdout,"qmimgr is compiled with no SMS feature (V_SMS_QMI_MODE), use '-f sms' option\n");
		SYSLOG(LOG_ERROR,"qmimgr is compiled with no SMS feature (V_SMS_QMI_MODE), use '-f sms' option");
		exit(-1);
	}
	#endif

	#ifdef MODULE_PRI_BASED_OPERATION
	rdb_module_priid=strdupa(_get_str_db("priid_carrier",""));
	/* detect vzw pri */
	vzw_pripub=!strcmp(rdb_module_priid,"VZW");
	//isSynchronized = _get_int_db("link.profile.wwan.isSynchronized",0);
	count=0;
	#endif

	// main loop
	termbysignal=!(qmimgr_loop()<0);

	// release
	qmimgr_fini(termbysignal);

	// free featurehash
	fini_feature();

	return 0;
}

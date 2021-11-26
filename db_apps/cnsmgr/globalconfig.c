#include "globalconfig.h"

#include "featurehash.h"

#include <unistd.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cnsmgr_config _globalConfig = {0, };


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalconfig_printUsage(void)
{
	printf("Call-Direct CnS manager\n\n");

	printf("Usage: %s [OPTION]...\n", _globalConfig.szCurProcName);
	printf("\t-h                 \tprint usage\n");

	printf("\t-V                 \tdisplay version\n");
	printf("\t-v                 \tincrease debugging messsage level (max. 4 times, default - ERR#3,WARN#4,NOTI#5,INFO#6,DBG#7)\n");

	printf("\t-p <CnS port>      \tspecify CnS port name (default - MC8785V:/dev/ttyUSB0, MS8780:/dev/ttyUSB1 )\n");
	printf("\t-l <last port>     \tspecify the last port name that makes cnsmgr wait until the port appears\n");
	printf("\t-i <instance no.>  \tselect instance number for database keys (default 0)\n");
	printf("\t-d                 \tDisable daemonization\n");

	printf("\nFor maximum debugging display, run as folowing:\n");
	printf("\t# %s -vvvv&\n", _globalConfig.szCurProcName);
	printf("\nFor database debugging message, run as folowing:\n");
	printf("\t# %s -vvv&\n", _globalConfig.szCurProcName);
	printf("\t# logread -f\n\n");
	
	printf("\t-s                 \t share with simple_at_manager\n\n");
	printf("\t-x                 \tDisable SMS\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL globalconfig_init(int cArg, char** pArg)
{
	int opt;
	static const char szDefCnsDevName[] = "/dev/ttyUSB0";

	_globalConfig.szCurProcName = pArg[0];

	// initiate default various
	_globalConfig.iDbgPrior = LOG_INFO;
	_globalConfig.szCnsDevName = szDefCnsDevName;
	_globalConfig.fDaemon = TRUE;
	_globalConfig.fSmsDisabled = FALSE;

	// parse command line
	while (__isSucc(opt = getopt(cArg, pArg, "Vvdi:r:l:p:hxt:f:")))
	{
		switch (opt)
		{
			case 't':
			case 'f':
// 				printf("optarg=%s\n",optarg);
				if( add_feature(optarg,opt=='t')<0 ) {
					fprintf(stdout,"failed to add feature - feature=%s,opt=%c\n",optarg,opt);
					exit(-1);
				}
				break;
			
			case 'V':
				printf("%s version %s\n", _globalConfig.szCurProcName, CNS_MANAGER_VERSION);
				break;

			case 'v':
				_globalConfig.iDbgPrior++;
				break;
				
			case 'd':
				_globalConfig.fDaemon = FALSE;
				break;

			case 'i':
				_globalConfig.iInstance = atol(optarg);
				break;

			case 'p':
				_globalConfig.szCnsDevName = optarg;
				break;

			case 'l':
				_globalConfig.szLastDevName = optarg;
				break;

			case 'x':
				_globalConfig.fSmsDisabled = TRUE;
				break
				;
			case 'h':
				globalconfig_printUsage();
				__goToError();

			case ':':
				globalconfig_printUsage();

				fprintf(stderr,"Option -%c requires an operand\n", optopt);
				__goToError();

			case '?':
				globalconfig_printUsage();

				fprintf(stderr,"Unrecognized option: -%c\n", optopt);
				__goToError();

		}
	}

	// by default, enable all feature if no option is specified
	if(is_enabled_feature(FEATUREHASH_ALL)<0) {
		if( add_feature(FEATUREHASH_ALL,1)<0 ) {
			fprintf(stdout,"failed to enable all feature by default\n");
			exit(-1);
		}
	}
	
	return TRUE;

error:
	return FALSE;
}


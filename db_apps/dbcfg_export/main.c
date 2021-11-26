#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <crypt.h>
#include <ctype.h>
#include <libgen.h>
#include <getopt.h>

#include "base.h"
#include "sslencrypt.h"

// extern
#include "rdb_ops.h"
#include "textedit.h"
#include "uricodec.h"

///////////////////////////////////////////////////////////////////////////////

#define FILENAME_CFG_IN			"/usr/local/cdcs/conf/system.conf"
#define FILENAME_CFG_OUT		"/usr/local/cdcs/conf/override.conf"

#define DBCFG_EXPORT_PREPASSWORD	"config_password"
#define DBCFG_EXPORT_HEAD		"01234567890123456789012345678901234567890123456789012345678901234567890123456789"

///////////////////////////////////////////////////////////////////////////////

//#define CONFIG_FULL_FORMAT_CFG

enum action_t
{
	ACTION_EXPORT, ACTION_IMPORT
};


struct option_t
{
	BOOL fReboot;

	const char* szOutCfg;
	const char* szInCfg;

	const char* szKey;
};

///////////////////////////////////////////////////////////////////////////////

static struct option_t _progOpt = {0, };

static char _szProgBaseName[PATH_MAX];
static int _progAction;

///////////////////////////////////////////////////////////////////////////////

static sslencrypt* _pSSL;

///////////////////////////////////////////////////////////////////////////////
void printUsage()
{

	if(_progAction==ACTION_EXPORT)
	{
		fprintf(stderr, "NetComm Wireless database config export tool\n");
		fprintf(stderr, "\n");

		fprintf(stderr, "Usage> dbcfg_export [options]");
		fprintf(stderr, "\n");

		fprintf(stderr, "\t -p \t\t Specify encryption password (default:empty)\n");
		fprintf(stderr, "\t -o \t\t Output configuration to a file (default:stdout)\n");
		fprintf(stderr, "\t -i \t\t Input configuration from a file (default:%s)\n", FILENAME_CFG_IN);
		fprintf(stderr, "\t -h, --help \t This message\n");
		
		fprintf(stderr, "\n");
	}
	else if(_progAction==ACTION_IMPORT)
	{
		fprintf(stderr, "NetComm Wireless database config import tool\n");
		fprintf(stderr, "\n");

		fprintf(stderr, "Usage> dbcfg_import -i <encrypted config. file> [options]");
		fprintf(stderr, "\n");

		fprintf(stderr, "\t -r \t\t Reboot after export has finished.\n");
		fprintf(stderr, "\t -p \t\t Specify encryption password (default:empty)\n");
		fprintf(stderr, "\t -o \t\t Output configuration to a file (default:%s)\n", FILENAME_CFG_OUT);
		fprintf(stderr, "\t -i \t\t Input configuration from a file\n");
		fprintf(stderr, "\t -h, --help \t This message\n");
		fprintf(stderr, "\n");

		fprintf(stderr, "Exit code>\n");
		fprintf(stderr, "\t 255 \t General failure\n");
		fprintf(stderr, "\t 254 \t Decryption error detected\n");
		fprintf(stderr, "\t 253 \t Wrong formation found\n");
		fprintf(stderr, "\t 252 \t Incorrect configuration environment fields(!)\n");
		fprintf(stderr, "\t 251 \t Maximum line length exceeded.\n");
	}
}

///////////////////////////////////////////////////////////////////////////////
static const char* getToken(const char* achMsg,char* pBuf,int cbBuf, int chEnd)
{
	const char* pPtr=achMsg;
	int fCopying=0;
	char* pDst=pBuf;

	while(*pPtr && (pDst-pBuf)<cbBuf)
	{
		if(!isspace(*pPtr))
			fCopying=1;
		

		if(fCopying)
		{
			if(chEnd!=-1 && *pPtr==chEnd)
				break;

			*pDst++=*pPtr;
		}

		pPtr++;
	}

	*pDst++=0;

	return pPtr;
}

///////////////////////////////////////////////////////////////////////////////
int importCfg(void)
{
	int nExitCode=-1;

	FILE* pFOut = stdout;

	// unload
	textedit_unload();

	// load configuration file
	if (textedit_load(_progOpt.szInCfg) < 0)
	{
		fprintf(stderr, "failed to load cfg(%s)\n", _progOpt.szInCfg);
		goto error;
	}

	char achNextDoor[PATH_MAX];
	achNextDoor[0]=0;

	// open out configuration
	if (_progOpt.szOutCfg)
	{
		sprintf(achNextDoor,"%s-%d",_progOpt.szOutCfg,getpid());

		if (!(pFOut = fopen(achNextDoor, "w+t")))
		{
			fprintf(stderr, "failed to create output file - %s\n", _progOpt.szOutCfg);
			goto error;
		}
	}

	// variables for parse
	char szName[MAX_NAME_LENGTH+1];
	char achRawValue[TEXTEDIT_MAX_LINE_LENGTH];
	int stat;


	int fIgnoreComment=0;
	
	char* pContent;

	// parse configuration file
	texteditline* pNextL= textedit_findFirst();
	texteditline* pL;
	int iLine = 0;
	while ((pL=pNextL)!=NULL)
	{
		iLine++;
		pNextL = textedit_findNext();

		pContent=textedit_get_line_ptr(pL);
		
		if(pContent[0]=='#' || pContent[0]==0)
		{
			int fCommentHead=strstr(pContent,DBCFG_EXPORT_HEAD)!=NULL;

			if(!fIgnoreComment && fCommentHead)
				fIgnoreComment=1;
			else if (fIgnoreComment && fCommentHead)
				fIgnoreComment=0;
				
			if(!fCommentHead && !fIgnoreComment)
				fprintf(pFOut, "%s\n", pContent);
		}
		else if (pContent[0]=='!')
		{
			char achName[256];
			char achPassword[256];

			// get name
			const char* pPtr=&pContent[1];
			achName[0]=0;
			pPtr=getToken(pPtr,achName,sizeof(achName),'=');

			// get password
			pPtr++;
			getToken(pPtr,achPassword,sizeof(achPassword),-1);

			// if password
			if(strcasestr(achName,DBCFG_EXPORT_PREPASSWORD))
			{
				// do not override if the key is manually input
				if(!_progOpt.szKey)
					sslencrypt_setKey(_pSSL,achPassword);
			}
			else
			{
				fprintf(stderr, "configuration environment error found at line #%d\n", iLine);
				nExitCode=-4;
				goto error;
			}

		}
		else
		{
#ifdef CONFIG_FULL_FORMAT_CFG
			int nUser;
			int nGroup;
			int nPerm;

			int nEmbeddedFlag=0;
			int nReadFlags;

			// read elements from the line
			if(readCfgFormat(pL,szName,&nUser,&nGroup,&nPerm,&nReadFlags,achRawValue,sizeof(achRawValue))<0) {
				nEmbeddedFlag=1;
			}
			else {
				nEmbeddedFlag=0;

				nUser=0;
				nGroup=0;
				nPerm=0;
#endif					
				stat=readRawExtCfgFormat(pL,szName,achRawValue,sizeof(achRawValue));
				if (stat < 0)
				{
					fprintf(stderr, "configuration error found at line #%d\n", iLine);
					nExitCode=-3;
					goto error;
				}
#ifdef CONFIG_FULL_FORMAT_CFG
			}
#endif
			char achNewValue[RDBMANAGER_DATABASE_VARIABLE_LENGTH];

			// get flags
			int nFlags=PERSIST;
			if(sslencrypt_isSslEncrypted(achRawValue)>=0)
				nFlags|=CRYPT;

			int stat;
			// decode
			if(nFlags & CRYPT)
				stat=sslencrypt_sslDecrypt(_pSSL, achRawValue, achNewValue, sizeof(achNewValue));
			else
				stat=uriDecode(achRawValue, achNewValue, sizeof(achNewValue));

			if(stat<0)
			{
				fprintf(stderr, "failed to decode at line #%d\n", iLine);
				nExitCode=-2;
				goto error;
			}

#ifdef CONFIG_FULL_FORMAT_CFG
			/* use flags from config file - not based on encrypted status */
			if(nEmbeddedFlag) {
				nFlags=nReadFlags;
			}
#endif

			// output
			int cbLine = writeCfgFormatF(pFOut,szName,0,0,0,nFlags,achNewValue);
			if (!(cbLine < TEXTEDIT_MAX_LINE_LENGTH))
			{
				fprintf(stderr, "maximum line length exceeded (line#%d)\n", iLine);
				nExitCode=-5;
				goto error;
			}
		}
	}

	// close if not standard out
	if (pFOut != stdout)
		fclose(pFOut);

	textedit_unload();

	// rename
	if(strlen(achNextDoor))
		rename(achNextDoor,_progOpt.szOutCfg);

	if(_progOpt.fReboot)
	{
		system("reboot");
	}
	else
	{
		fprintf(stderr, "The unit needs to be rebooted.\n");
	}
	
	return 0;

error:
	if (pFOut && pFOut != stdout)
		fclose(pFOut);

	textedit_unload();

	if(strlen(achNextDoor))
		remove(achNextDoor);

	return nExitCode;
}

///////////////////////////////////////////////////////////////////////////////
int exportCfg(void)
{
	FILE* pFOut = stdout;

	// unload
	textedit_unload();

	// load configuration file
	if (textedit_load(_progOpt.szInCfg) < 0)
	{
		fprintf(stderr, "failed to load cfg(%s)\n", _progOpt.szInCfg);
		goto error;
	}


	// open out configuration
	if (_progOpt.szOutCfg)
	{
		if (!(pFOut = fopen(_progOpt.szOutCfg, "w+t")))
		{
			fprintf(stderr, "failed to create output file - %s\n", _progOpt.szOutCfg);
			goto error;
		}
	}

	// add comment
	const char* szHeadComment=
		"#! " DBCFG_EXPORT_HEAD "\n" 
		"#\n" 
		"#   Do not delete the exclamation marked lines at the begining and the end\n" 
		"#\n" 
		"# This file is an exported configuration from NetComm Bovine platform based device.\n"
		"# Private fields are encrypted but any configuraiton entry can be manually replaced by\n"
		"#   a plain-text variable or URI-encoded text.\n"
		"#\n"
		"# ** CAUTION\n"
		"#   Dollar sign($) is not allowed at the start of variables.\n"
		"#   Use %%24 instead of dollar sign.\n"
		"#\n" 
		"# ** UNLOCK\n"
		"#    To unlock this configuration file, uncomment and input the password\n"
		"#    in the following line. The line should start with an exclamation mark\n"
		"#\n"
		"#! " DBCFG_EXPORT_PREPASSWORD "=<password>\n"
		"#\n" 
		"#! " DBCFG_EXPORT_HEAD "\n";

	fprintf(pFOut, szHeadComment);

	// variables for parse
	char szName[MAX_NAME_LENGTH+1];
	char achRawValue[TEXTEDIT_MAX_LINE_LENGTH];
	int nUser;
	int nGroup;
	int nPerm;
	int nFlags;
	int stat;
	
	char* pContent;

	
	// parse configuration file
	texteditline* pNextL = textedit_findFirst();
	texteditline* pL;
	int iLine = 0;
	while ( (pL=pNextL)!=NULL)
	{
		iLine++;
		pNextL = textedit_findNext();

		pContent=textedit_get_line_ptr(pL);
		
		if(pContent[0]=='#' || pContent[0]==0)
		{
			fprintf(pFOut, "%s\n", pContent);
		}
		else
		{
			// read elements from the line
			stat = readCfgFormat(pL, szName, &nUser, &nGroup, &nPerm, &nFlags, achRawValue,sizeof(achRawValue));
			if (stat < 0)
			{
				fprintf(stderr, "configuration error found at line #%d\n", iLine);
				continue;
			}

			char achNewValue[RDBMANAGER_DATABASE_VARIABLE_LENGTH];
			int cbNeeded = 0;

			// encrypt or uri encode
			if (nFlags & CRYPT)
				sslencrypt_sslEncrypt(_pSSL, achRawValue, achNewValue, sizeof(achNewValue), &cbNeeded);
			else
				uriEncode(achRawValue, achNewValue, sizeof(achNewValue));

			int cbLine;

#ifdef CONFIG_FULL_FORMAT_CFG
			cbLine = fprintf(pFOut, "%s;%d;%d;0x%x;0x%x;%s\n", szName, nUser, nGroup, nPerm, nFlags,achNewValue);
#else
			cbLine = fprintf(pFOut, "%s;%s\n", szName, achNewValue);
#endif
			if (!(cbLine < TEXTEDIT_MAX_LINE_LENGTH))
				fprintf(stderr, "maximum line length exceeded (line#%d)\n", iLine);
		}

		
	}

	// close if not standard out
	if (pFOut != stdout)
		fclose(pFOut);

	textedit_unload();
	
	return 0;


error:
	if (pFOut && pFOut != stdout)
		fclose(pFOut);

	textedit_unload();

	return -1;
}

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{

	// get program basename
	char pProgFullName[PATH_MAX];
	strcpy(pProgFullName,argv[0]);

	// get basename
	const char* pProgBaseName = basename(pProgFullName);
	strcpy(_szProgBaseName, pProgBaseName);


	// get action
	if (strcasestr(_szProgBaseName, "exp"))
	{
		_progAction = ACTION_EXPORT;
	}
	else if (strcasestr(_szProgBaseName, "imp"))
	{
		_progAction = ACTION_IMPORT;
	}
	else
	{
		fprintf(stderr, "Unknown executable file name\n");
		exit(-1);
	}

	// create SSL
	_pSSL = sslencrypt_create(RDBMANAGER_DATABASE_VARIABLE_LENGTH);

	// get options
	int opt;
	while ((opt = getopt_long(argc, argv, "rp:i:o:h", long_opts, NULL)) != EOF)
	{
		switch (opt)
		{
			case 'r':
				_progOpt.fReboot = TRUE;
				break;

			case 'p':
				_progOpt.szKey=optarg;
				sslencrypt_setKey(_pSSL, _progOpt.szKey);
				break;

			case 'o':
				_progOpt.szOutCfg = optarg;
				break;

			case 'i':
				_progOpt.szInCfg = optarg;
				break;

			case 'h':
				printUsage();
				exit(-1);

			case '?':
				fprintf(stderr, "parameter operand missing\n");
				printUsage();
				exit(-1);

			default:
				fprintf(stderr, "Unknown option detected - %c\n", (char)opt);
				printUsage();
				exit(-1);
		}
	}

	int stat = -1;

	// get action
	switch (_progAction)
	{
		case ACTION_IMPORT:
			// check complusory options
			if(!_progOpt.szInCfg)
			{
				fprintf(stderr, "configuration file not specified\n");
				printUsage();
				exit(-1);
			}

			if(!_progOpt.szOutCfg)
				_progOpt.szOutCfg=FILENAME_CFG_OUT;

			stat=importCfg();
			break;

		case ACTION_EXPORT:
			// default options
			if(!_progOpt.szInCfg)
				_progOpt.szInCfg=FILENAME_CFG_IN;

			stat=exportCfg();
			break;
	}

	// destroy SSL
	sslencrypt_destroy(_pSSL);

	return stat;
}

///////////////////////////////////////////////////////////////////////////////

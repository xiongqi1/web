/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>

#include <dirent.h>
#include <errno.h>

#include "base.h"
#include "daemon.h"
#include "configjob.h"
#include "templatejob.h"

#include "rdb_ops.h"
#include "DD_ioctl.h"
#include "errno.h"

#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')

#define xtod(c) ((c>='0' && c<='9') ? c-'0' : ((c>='A' && c<='F') ? \
                 c-'A'+10 : ((c>='a' && c<='f') ? c-'a'+10 : 0)))

#define MAXLISTLEN 10000
#define MAXVALUELEN 10000

int g_fSigTerm = 0;
int g_fSigRefresh = 0;

int _fPatched = 0;

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static void addonfilepath(char **path)
{
        DIR *dir;
        struct dirent *dp;
        char *file_name;

        dir = opendir(RDBMANAGER_CFG_ADDON_DIR);
	if (dir == NULL)
	{
		syslog(LOG_ERR, "Failed to open addon directory - %s", strerror(errno));
		*path = NULL;
		return;
	}

        while ((dp=readdir(dir)) != NULL) {
                file_name = dp->d_name;
                if (strstr((const char*)file_name,RDBMANAGER_CFG_FILENAME_ADDON_BEGIN))
                {
                        *path = (char *)malloc(strlen(RDBMANAGER_CFG_ADDON_DIR)+strlen(file_name)+1);
                        strncpy(*path,RDBMANAGER_CFG_ADDON_DIR,sizeof(RDBMANAGER_CFG_ADDON_DIR));
			*(*path+sizeof(RDBMANAGER_CFG_ADDON_DIR)) = 0;
                        strncat(*path,file_name,strlen(file_name));
                        closedir(dir);
                        return;
                }
        }

	syslog(LOG_ERR, "Failed to find addon file - %s", strerror(errno));
        *path = NULL;
        closedir(dir);
}


const char shortopts[] = "pdvV?sctf:";

static int usage(char **argv)
{
	fprintf(stderr, "\nUsage: %s [-p] [-d] [-v] \n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-p Print contents of database and exit\n");
	fprintf(stderr, "\t-d Don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-v Increase verbosity\n");
	fprintf(stderr, "\t-s non-fork mode - statistics mode only\n");
	fprintf(stderr, "\t-c non-fork mode - configuration mode only\n");
	fprintf(stderr, "\t-f in configuration mode, patch config using a patch file given\n");
	fprintf(stderr, "\t-t non-fork mode - template manager mode only\n");
	fprintf(stderr, "\t-V Display version information\n");

	fprintf(stderr,
			"\n Signals:\n"
			"\t SIGHUP         flush RDB variables to storage [RDB config]\n"
			"\t SIGINT/SIGTERM terminate manager\n"
	);
	
	fprintf(stderr, "\n");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static int rdb_dump_data(void)
{
	char	names[MAXLISTLEN+1];
	char	value[MAXVALUELEN+1];
	char	*name;
	int	len;

	len = sizeof(names) - 1;
	rdb_get_names("", names, &len, 0);

	//printf("Names %d = %s\n", len, names);

	printf("\n");
	name = strtok(names, "&");
	while (name)
	{
		len = MAXVALUELEN;
		rdb_get_single(name, value, sizeof(value));

		int len;
		int flags;
		int perm;
		int uid;
		int gid;

		rdb_get_info(name, &len, &flags, &perm, &uid, &gid);

		value[sizeof(value)-1] = '\0';
		printf("%s(0x%08x);0x%08x;%d;%d;%s\n", name, flags, perm, uid, gid, value);

		name = strtok(NULL, "&");
	}
	printf("\n");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static void sig_handler_hup(int signum)
{
	g_fSigRefresh=1;

	syslog(LOG_INFO, "got SIGHUP");
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void sig_handler_ignore(int signum)
{
	syslog(LOG_DEBUG, "got SIG- %d",signum);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static void sig_handler(int signum)
{
	g_fSigTerm = 1;

	syslog(LOG_NOTICE, "terminating : got SIG - %d", signum);
}


// return -1 on error
// return -1 if patch_file_name is not found
// return number of replacements - 0 meaning nothing was replaced
#define PATCH_SEPARATOR_CHAR ';'

//
// Patch configuration variables - this occurs in 2 scenarios:
// 1) Upgrade f/w to new f/w that uses a new RDB name (instead of the old one)
// 2) Restore config saved by old version of firmware into a router running
//  a new version of f/w
//
// Implements 1:1 replacement of variables based on patch rules file in a format of
// old_var1;new_var1
// old_var2;new_var2
//
static int patchCfg(char *patch_file_name)
{
    FILE *pFRules;
    int patch_count = 0;
    char *old_rdb_name, *new_rdb_name;
    char value[MAX_VALUE_LEN_RDB];
    int flags;

    if (rdb_open_db() < 0)
    {
        return -1;
    }

    // no patch file found
    pFRules = fopen(patch_file_name, "rt");
    if (!pFRules)
    {
        return -1;
    }

	// read patch rules file line by line
    char *line = NULL;
    ssize_t read_bytes;
    size_t len = 0;

    while ((read_bytes = getline(&line, &len, pFRules)) != -1)
    {
        // process line - split into 2 values - old RDB var name and new RDB var name

        // ONLY if the following is strictly met, we replace one RDB variable with
        // a variable with a new name, retaining the value
        old_rdb_name = line;
        new_rdb_name = strchr(line, PATCH_SEPARATOR_CHAR);

        if (!old_rdb_name || !new_rdb_name)
        {
            break;
        }

        char *new_line = strchr(new_rdb_name, '\n');
        if (new_line)
        {
            // RDB names cannot include new lines - get rid of it
            *new_line = 0;
        }

        // 1) terminate for old name,
        // 2) point past separator,
        *new_rdb_name++ = 0;

        // 3) and make sure the new RDB var name is not empty
        if (*new_rdb_name == 0)
        {
            break;
        }

        // check that the old RDB variable exists and get its value and flags
        if (rdb_get_single(old_rdb_name, value, sizeof(value)) == 0)
        {
            value[sizeof(value)-1] = 0; // terminate as get_single doesn't seem to bother
            if (rdb_get_flags(old_rdb_name, &flags) == 0)
            {
                //
                // the last three arguments are not strictly correct but there is
                // no way to replicate the old var's permissions and UID and GID
                // through rdb api, so we create them with default values
                // which is all that we ever use
                //
                // Also note that if the new_ rdb variable does not exist, it is
                // created, otherwise its value gets overwritten (this is how
                // rdb_update_single works)
                //
                if (rdb_update_single(new_rdb_name, value, flags, DEFAULT_PERM, 0, 0) == 0)
                {
                    rdb_delete_variable(old_rdb_name);
                    patch_count++;
                }
            }
        }
    }

    free(line);
    fclose(pFRules);

    // log one message
    syslog(LOG_INFO, "Patched using file %s, replaced %d RDB variables", patch_file_name, patch_count);

    return patch_count;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	int	ret = 0;
	int	verbosity = LOG_NOTICE;
	int	be_daemon = 1;
	int	dump = 0;

	int statOnly = 0;
	int cfgOnly = 0;
	int rdbReady = 0;
	int templateOnly = 0;
	char *patchFile = NULL;

	char *addonFile = NULL;

	// Parse Options
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 't':
				templateOnly = 1;
				break;

			case 's':
				statOnly = 1;
				break;

			case 'c':
				cfgOnly = 1;
				break;

			case 'f':
                		patchFile = optarg;
                		break;

			case 'd':
				be_daemon = 0;
				break;
			case 'v':
				verbosity++ ;
				break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0],
				        VER_MJ, VER_MN, VER_BLD);
				break;
			case 'p':
				be_daemon = 0;
				dump = 1;
				break;
			case '?':
				usage(argv);
				return 2;
		}
	}

	// dump
	if (dump)
	{
		if (rdb_open_db() < 0)
			pabort("can't open device");
		rdb_dump_data();
		rdb_close_db();

		exit(0);
	}

	// initialize temporary logging
	openlog(DAEMON_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity));
	syslog(LOG_INFO, "starting");

	int cmdIdx;

	if(!statOnly && !templateOnly)
	{
		syslog(LOG_INFO, "preinitializing database....");

		if (rdb_open_db() >= 0)
		{
			rdbReady = getRdbReady(RDB_MANAGER_MODE_CONFIG);
			rdb_close_db();
		}

		if (!rdbReady)
		{
			// do preinit - rdb_manager
			sync();
			rename(RDBMANAGER_CFG_FILENAME_OVERRIDE, RDBMANAGER_CFG_FILENAME_CUR);

			addonfilepath(&addonFile);

			if (doConfigJobPrepare(RDBMANAGER_CFG_FILENAME_CUR, RDBMANAGER_CFG_FILENAME_NEW, RDBMANAGER_CFG_FILENAME_DEF, addonFile, PERSIST, 0) < 0)
			{
				pabort("failed to prepare rdb configuration manager");
				exit(-1);
			}

			if(addonFile)
			{
				remove(addonFile);
				free(addonFile);
			}

			if (patchFile && (patchCfg(patchFile) > 0))     // should return > 0 if anything changed
			{
				_fPatched = 1;
			}

			syslog(LOG_INFO, "finished - config. database variables are ready");

			/* set rdb ready flag */
			setRdbReady(RDB_MANAGER_MODE_CONFIG, 1);
		}
		else
		{
			/* Prepare but do not load from any file if we found config has been done earlier.
			 * rdb_manager comes to this point when database has been preinitialized but
			 * rdb_manager crashes unexpectedly and gets restarted.
			 *
			 * Instead of loading anything from files, current data in RDB should be seen as the
			 * latest and rdb_manager should use the data, so the changes to data between
			 * rdb_manager crashes and restarts will not be overwritten. */
			if (doConfigJobPrepare(NULL, NULL, NULL, NULL, PERSIST, 0) < 0)
			{
				pabort("failed to prepare rdb configuration manager for reinitialization");
				exit(-1);
			}
		}
	}

	if (cfgOnly)
	{
		cmdIdx = 0;
	}
	else if (statOnly)
	{
		cmdIdx = 1;
	}
	else if (templateOnly)
	{
		cmdIdx = 2;
	}
	else
	{
		if (fork() != 0)
		{
			cmdIdx = 0;
		}
		else if (fork() != 0)
		{
			cmdIdx = 1;
		}
		else
		{
			cmdIdx = 2;
		}
	}

	char achLFile[256] = {0, };

	if (be_daemon)
	{
		sprintf(achLFile, "/var/lock/subsys/%s%d", DAEMON_NAME, cmdIdx);
		daemonize(achLFile, RUN_AS_USER);
	}

	const char* daemonNames[]={
		DAEMON_NAME "-cfg",
		DAEMON_NAME "-sta",
		DAEMON_NAME "-tpl"
	};

	closelog();

	// Initialize the seperate logging
	openlog(daemonNames[cmdIdx], LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity));
	syslog(LOG_NOTICE, "daemon starting - %d mode", cmdIdx);

	// Configure signals
	signal(SIGHUP, sig_handler_hup);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGCHLD, sig_handler_ignore);
	signal(SIGTTIN, sig_handler_ignore);
	signal(SIGTTOU, sig_handler_ignore);


	if (cmdIdx == 0)
	{
		doConfigJob(RDBMANAGER_CFG_FILENAME_CUR, RDBMANAGER_CFG_FILENAME_NEW, RDBMANAGER_CFG_FILENAME_DEF,
			PERSIST, RDBMANAGER_CFG_WRITEBACK_DELAY, RDB_MANAGER_MODE_CONFIG);

		/* clear rdb ready flag */
		setRdbReady(RDB_MANAGER_MODE_CONFIG, 0);
		rdb_close_db();

	}
	else if (cmdIdx == 1)
	{
		if (rdb_open_db() >= 0)
		{
			rdbReady = getRdbReady(RDB_MANAGER_MODE_STATS);
			rdb_close_db();
		}

		if (!rdbReady)
		{
			/* start statistics */
			if (doConfigJobPrepare(RDBMANAGER_STATISTIC_FILENAME_CUR, RDBMANAGER_STATISTIC_FILENAME_NEW, NULL, NULL, STATISTICS, 0) < 0)
			{
				pabort("failed to prepare rdb configuration manager");
				exit(-1);
			}

			setRdbReady(RDB_MANAGER_MODE_STATS, 1);
		}
		else
		{
			/* Prepare but do not load from any file if we found config has been done earlier.
			 * rdb_manager comes to this point when database has been preinitialized but
			 * rdb_manager crashes unexpectedly and gets restarted.
			 *
			 * Instead of loading anything from files, current data in RDB should be seen as the
			 * latest and rdb_manager should use the data, so the changes to data between
			 * rdb_manager crashes and restarts will not be overwritten. */
			if (doConfigJobPrepare(NULL, NULL, NULL, NULL, STATISTICS, 0) < 0)
			{
				pabort("failed to prepare rdb configuration manager for reinitialization");
				exit(-1);
			}
		}

		doConfigJob(RDBMANAGER_STATISTIC_FILENAME_CUR, RDBMANAGER_STATISTIC_FILENAME_NEW, NULL,
			STATISTICS, RDBMANAGER_STATISTIC_WRITEBACK_DELAY, RDB_MANAGER_MODE_STATS);

		/* clear rdb ready flag */
		setRdbReady(RDB_MANAGER_MODE_STATS, 0);
		rdb_close_db();
	}
	else
		doTemplateJob();

	syslog(LOG_NOTICE, "exiting - %d mode", cmdIdx);

	//close log
	closelog();

	if (strlen(achLFile) > 0)
	{
		// unlock lock file
		unlink(achLFile);
	}

	return 0;
}



/*
* vim:ts=4:sw=4:
*/

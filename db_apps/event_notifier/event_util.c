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

#include "event_rdb_util.h"
#include "event_rdb_names.h"
#include "daemon.h"

extern int rdbfd;
extern int running;

void release_singleton(const char *lockfile)
{
	daemon_fini();
}

void release_resources(const char *lockfile)
{
	release_singleton(lockfile);
}

int isPIDRunning(pid_t pid)
{
	char achProcPID[128];
	char cbFN;
	struct stat statProc;

	cbFN = sprintf(achProcPID, "/proc/%d", pid);
	return stat(achProcPID, &statProc) >= 0;
}

void ensure_singleton(const char *appl_name, const char *lockfile)
{
	char achPID[128];
	int fd;
	int cbRead;

	pid_t pid;
	int cbPID;

	fd = open(lockfile, O_RDWR | O_CREAT | O_EXCL, 0640);
	if (fd < 0)
	{
		if (errno == EEXIST)
		{
			fd = open(lockfile, O_RDONLY);

			// get PID from lock
			cbRead = read(fd, achPID, sizeof(achPID));
			if (cbRead > 0)
				achPID[cbRead] = 0;
			else
				achPID[0] = 0;

			pid = atoi(achPID);
			if (!pid || !isPIDRunning(pid))
			{
				syslog(LOG_ERR, "deleting the lockfile - %s", lockfile);
				close(fd);
				release_singleton(lockfile);
				ensure_singleton(appl_name, lockfile);
				return;
			}
		}

		syslog(LOG_ERR, "another instance of %s already running (because creating lock file %s failed: %s)", appl_name, lockfile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid = getpid();
	cbPID = sprintf(achPID, "%d\n", pid);

	write(fd, achPID, cbPID);
	close(fd);
}

void sig_handler(int signum)
{
	pid_t	pid;
	int stat;

	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notfication via SIGHUP. So we don't whinge
	on SIGHUP. */
	//if (signum != SIGHUP)
	//	syslog(LOG_DEBUG, "caught signal %d", signum);

	switch (signum)
	{
		case SIGUSR1:
			break;

		case SIGHUP:
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			running = 0;
			break;

		case SIGCHLD:
			if ( (pid = waitpid(-1, &stat, WNOHANG)) > 0 )
				syslog(LOG_DEBUG, "Child %d terminated\n", pid);
			break;
	}
}

void fini(const char *lockfile)
{
	rdb_fini();
	closelog();
	release_resources(lockfile);
	syslog(LOG_INFO, "terminated...");
}


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
#include <stdlib.h>
#include <string.h>
//#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
//#include <termios.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include "cdcs_syslog.h"
#include "./pots_rdb_operations.h"
#include "slic/types.h"

//#define MAXLISTLEN 10000
//#define MAXVALUELEN 10000
//static const char *device = "/dev/cdcs_DD";

extern int pots_bridge_running;
char pots_bridge_rdb_prefix[64];
BOOL pots_initialized = FALSE;
BOOL is_modem_exist = FALSE;
int check_slic_event_during_rdb_command_processing = 0;

int rdb_poll_events(int timeout, const char *name, const char *value)
{
	int ret;
	struct pollfd* fds;
	const int fdcount = 1;
	fds = calloc(fdcount, sizeof(struct pollfd));
	fds[0].fd = rdb_get_fd();
	fds[0].events |= POLLIN;
	ret = poll(fds, fdcount, timeout);
	if (ret < 0)
	{
		SYSLOG_ERR("poll() failed: %d (%s)\n", errno, strerror(errno));
	}
	free(fds);
	//log_DEBUG("Events occurred: %d.", ret);
	//log_DEBUG("fd is ready to be read: %d\n", fds[0].revents & POLLIN);
	return ret;
}

int send_rdb_command(const char* name, const char* value, int retry_cnt)
{
	int done = 0;
	int rdb_locked = 1;
	int i;
	char buf[256];
	int max_retry = is_modem_exist? (retry_cnt? retry_cnt:50):1;

	//SYSLOG_ERR( "set '%s'='%s'", name, value );

	if (rdb_get_single(name, buf, sizeof(buf)) == 0 && strcmp(buf, "") == 0)
		done = (rdb_set_single(name, value) == 0);
	//SYSLOG_ERR( "initial done = %d", done );
	if (check_slic_event_during_rdb_command_processing)
	{
		while (max_retry-- > 0 && done == 0)
		{
			fd_set fdr;
			int selected;
			int nfds, max_slic_fd = 0;
			struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
			FD_ZERO(&fdr);
			for (i = 0; i < MAX_CHANNEL_NO; i++)
			{
				if (slic_info[i].slic_fd >= max_slic_fd) max_slic_fd = slic_info[i].slic_fd;
				if (slic_info[i].slic_fd >= 0) FD_SET(slic_info[i].slic_fd, &fdr);
			}
			nfds = 1 + max_slic_fd;
			selected = select(nfds, &fdr, NULL, NULL, &timeout);
			if (!pots_bridge_running)
				return -1;
			if (selected < 0)
			{
				continue;
			}
			else if (selected > 0)
			{
				for (i = 0; i < MAX_CHANNEL_NO; i++)
				{
					if (slic_info[i].slic_fd >= 0 && FD_ISSET(slic_info[i].slic_fd, &fdr))
					{
						SYSLOG_DEBUG("idx [%d] got SLIC event of slic_fd %d during SEND_RDB_COMMAND", i, slic_info[i].slic_fd);
						slic_handle_events(i);
					}
				}
				continue;
			}
			else
			{
				if (rdb_get_single(name, buf, sizeof(buf)) == 0 && strcmp(buf, "") == 0)
				{
					//SYSLOG_ERR( "set '%s'='%s'", name, value );
					done = (rdb_set_single(name, value) == 0);
					//SYSLOG_ERR( "done = %d", done );
				}
				if (!pots_initialized || done)
					break;
			}
		}
	}
	else
	{
		// give at least 5 seconds to simple_at_manager
		i = 0;
		while (i++ < max_retry && !done)
		{
			if (!pots_bridge_running)
				break;
			if (rdb_get_single(name, buf, sizeof(buf)) == 0 &&
			        strcmp(buf, "") == 0)
			{
				//SYSLOG_ERR( "set '%s'='%s'", name, value );
				done = (rdb_set_single(name, value) == 0);
			}
			if (!pots_initialized || done)
			{
				break;
			}
			usleep(100000);
		}
	}

	// we force it to do the job anyway in case simple_at_manager misses the command
	rdb_locked = 0;
	if (!done)
	{
		//SYSLOG_ERR( "set '%s'='%s' again after timeout", name, value );
		done = rdb_set_single(name, value) == 0;
	}

	if (rdb_locked)
	{
		SYSLOG_ERR("cannot set '%s' to '%s' (rdb locked)", name, value);
		return -1;
	}
	if (!done)
	{
		SYSLOG_ERR("cannot set '%s' to '%s' (%s); are the AT and/or CnS managers running?", name, value, strerror(errno));
		return -1;
	}
	return 0;
}

static int get_rdb_command_status(const char* status, int retry_cnt)
{
	int i;
	char buf[256];
	int max_retry = is_modem_exist? (retry_cnt? retry_cnt:5):1;

	//SYSLOG_DEBUG("***** retry cnt = %d", max_retry);
	if (check_slic_event_during_rdb_command_processing)
	{
		while (max_retry-- > 0)
		{
			fd_set fdr;
			int selected;
			int nfds, max_slic_fd = 0;
			struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };

			FD_ZERO(&fdr);
			for (i = 0; i < MAX_CHANNEL_NO; i++)
			{
				if (slic_info[i].slic_fd >= max_slic_fd) max_slic_fd = slic_info[i].slic_fd;
				if (slic_info[i].slic_fd >= 0) FD_SET(slic_info[i].slic_fd, &fdr);
			}
			nfds = 1 + max_slic_fd;
			selected = select(nfds, &fdr, NULL, NULL, &timeout);
			if (!pots_bridge_running)
				break;
			if (selected < 0)
			{
				continue;
			}
			else if (selected > 0)
			{
				for (i = 0; i < MAX_CHANNEL_NO; i++)
				{
					if (slic_info[i].slic_fd >= 0 && FD_ISSET(slic_info[i].slic_fd, &fdr))
					{
						SYSLOG_DEBUG("idx [%d] got SLIC event of slic_fd %d during GET_RDB_COMMAND_STATUS", i, slic_info[i].slic_fd);
						slic_handle_events(i);
					}
				}
			}
			else
			{
				if (rdb_get_single(status, buf, sizeof(buf)) != 0 && pots_initialized)
					continue;
				if (*buf == 0 && pots_initialized)
					continue;
				//SYSLOG_DEBUG( "'%s'='%s' remaining retry cnt %d", status, buf, max_retry );
				if (strcmp(buf, "1") == 0)
				{
					//SYSLOG_ERR("got '%s'='%s' remaining retry cnt %d", status, buf, max_retry);
					return 0;
				}
			}
		}
	}
	else
	{
		for (i = 0; i < max_retry; ++i)
		{
			if (!pots_bridge_running)
				break;

			usleep(100000);
			if (rdb_get_single(status, buf, sizeof(buf)) != 0 && pots_initialized)
			{
				continue;
			}
			if (*buf == 0 && pots_initialized)
			{
				continue;
			}
			//SYSLOG_DEBUG( "'%s'='%s'", status, buf );
			if (strcmp(buf, "1") == 0)
			{
				//SYSLOG_ERR("get '%s'='%s' after %d attempts", status, buf, i);
				return 0;
			}
			else {
				SYSLOG_ERR("rdb command status failure : %s", buf);
				return -1;
			}
		}
	}
	SYSLOG_ERR("failed to get '%s'", status);
	return -1;
}

int send_rdb_command_blocking(const char* path, const char* value, int retry_cnt)
{
	char *name, *status = NULL;
	int i;
	int max_retry = is_modem_exist? (retry_cnt? retry_cnt:5):1;

	for (i = 0; i < max_retry; ++i)
	{
		name = (char *)rdb_variable(path, "", "");
		status = (char *)rdb_variable(path, "", "status");
		if (send_rdb_command(name, value, retry_cnt) != 0)
			return -1;
		if (!get_rdb_command_status(status, retry_cnt))
			return 0;
	}
	return get_rdb_command_status(status, retry_cnt);
}

int rdb_getvnames(char *name, char *value, int *length, int flags)
{
	int ret;
	ioctl_args args;
	args.name = name;
	args.value = value;
	args.len = *length;
	args.perm = 0;
	args.flags = flags;
	ret = ioctl(rdb_get_fd(), GETNAMES, (unsigned long) & args);
	*length = args.len;
	if (ret != 0)
	{
		SYSLOG_ERR("%d (%s)", errno, strerror(errno));
	}
	return ret;
}

const char* rdb_name(const char* prefix, const char* path, const char* instance, const char* name)
{
	enum { size = 10 };
	static unsigned int i = 0;
	static char names[size][256];
	const char* first_dot = path && *path ? "." : "";
	const char* second_dot = instance && *instance ? "." : "";
	const char* third_dot = name && *name ? "." : "";
	char* n = names[i];
	sprintf(n, "%s%s%s%s%s%s%s", prefix, first_dot, path, second_dot, instance, third_dot, name);
	if (++i == size)
	{
		i = 0;
	}
	return n;
}

int rdb_ready_to_send_command(const char* name)
{
	static const int size = 256;
	char* value = alloca(256);
	int result;
	result = rdb_get_single(name, value, size);
	if (strncasecmp(value, "AT+VTS", 6) != 0)
		return 1;
	return((result == 0 && strcmp(value, "") == 0));
}

const char* rdb_variable(const char* path, const char* instance, const char* name)
{
	return rdb_name((const char *)pots_bridge_rdb_prefix, path, instance, name);
}

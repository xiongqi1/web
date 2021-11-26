/*
 * watchdog_main.c
 *	implements RDB watchdog that is a replacement of the busybox watchdog.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "watchdog_logger.h"
#include <rdb_ops.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <limits.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/limits.h>

#ifdef V_RESET_LOGGER_y
#include "reset_logger.h"
#endif

#define DEFAULT_WATCHDOG_DEVICE "/dev/watchdog"
#define DEFAULT_EXPIRE_DURATION 60 /* in seconds */
#define DEFAULT_LOG_LEVEL WATCHDOG_LOG_WARNING_PRIO
#define DEFAULT_RDB_RETRY_TIME 30 /* seconds */
#define SYS_RDB_TIMER_VAR "sys.watchdog.timeToReboot"
#define SYS_RDB_QUEUE_VAR "sys.watchdog.queuedTimers"
#define SYS_RDB_TIMER_DEFAULT "-1"
#define SYS_RDB_QUEUE_DEFAULT "{}"
#define SYS_RDB_QUEUE_ENABLED "{[1]={[\"name\"]=\"BOOT\",[\"remaining\"]=%d,},}"

#ifdef V_STARTUP_WATCHDOG_y
#define SYS_RDB_COMMITMENT_TIMERS_BOOT_ENABLE "sys.watchdog.startup.enable"
#define SYS_RDB_COMMITMENT_TIMERS_BOOT_TIMEOUT "sys.watchdog.startup.timeout"
#define SYS_RDB_COMMITMENT_TIMERS_BOOT_MAX_REBOOTS "sys.watchdog.startup.max_reboots"
#endif /* #ifdef V_STARTUP_WATCHDOG_y */

#define SYS_RDB_REBOOT_CAUSE "sys.watchdog.reboot_cause"
#define SYS_RDB_LAST_REBOOT_CAUSE "sys.watchdog.last_reboot_cause"
#define SYS_RDB_LAST_REBOOT_COUNT "sys.watchdog.last_reboot_count"
#define SYS_RDB_MAX_REBOOT_COUNT "sys.watchdog.max_reboot_count"

// A RDB variable to notify to user space that the system is waiting
// for rebooting by watchdog.
#define SYS_RDB_WAITING_REBOOT "sys.watchdog.waiting_reboot"

// Timeout value to wait until watchdog bites since calling system rebooting
#define DEFAULT_WATCHDOG_REBOOT_WAITING_TIME 30

#define WATCHDOG_MONITOR_RDB_VAR_NUM 5
#define MAX_RDB_VALUE_LEN 128
#define MAX_RDB_NAME_LEN 128
#define MIN_RDB_RETRIES 5
#define MAX_RDB_READY_TIME  600 /* 10 mins */
#define MAX_RDB_OPEN_RETRY_TIME  DEFAULT_RDB_RETRY_TIME /* seconds */
#define MAX_RDB_CREATE_RETRY_TIME  DEFAULT_RDB_RETRY_TIME /* seconds */
#define MAX_RDB_MAIN_RDB_RETRY_TIME  DEFAULT_RDB_RETRY_TIME /* seconds */
#define MAX_RDB_APP_RDB_RETRY_TIME  DEFAULT_RDB_RETRY_TIME /* seconds */

struct rdb_var {
	char *name;
	char value[MAX_RDB_VALUE_LEN];
	int len;
	int active;
	int duration;
	int elapsed_time;
};

struct watchdog_context {
	int dev_fd;
	const char *dev_name;

	/* same as how often the process enters sleep in seconds */
	int kick_duration;
	/* indicates how often the process should kick the watchdog to
	 * avoid the watchdog reset in seconds */
	int expire_duration;
	/* indicates whether or not we should install a commitment timer
	 * at startup and the desired duration */
	int commit_enabled;
	int commit_duration;

	/* indicates whether to close watchdog device with 'V' magic close
	 * when the daemon is terminated.
	 */
	int graceful_closing;

#ifdef V_STARTUP_WATCHDOG_y
	/*
	 * After succesfully reading sys.watchdog.commitment_timers.enable,
	 * Set this variable so that the BOOT commitment timer is overriden only once.
	 */
	int boot_override_applied;
#endif

	int rdb_ready_time;
	/* indicates the RDB operation(e.g. rdb_open) is ready to use */
	int rdb_running;
	struct rdb_session *rdb_session;
	/* the number of RDB variables in the list */
	int nr_rdb_vars;
	struct rdb_var rdb_vars[WATCHDOG_MONITOR_RDB_VAR_NUM];

	int sys_rdb_retry_time;
	int max_sys_rdb_retry_time;
	int app_rdb_retry_time;
	int max_app_rdb_retry_time;
	int rdb_open_retry_time;
	int rdb_create_retry_time;

	int log_level;
	int log_channels;
	const char *rdbvars_filename;
	const char *log_filename;
};

static void watchdog_read_rdb_config(struct watchdog_context *ctx, const char *filename);

static const char *help_message_part1 =
"Usage: rdb_watchdog [OPTION] ...\n"
"   -t  N		Reset the watchdog timer every N seconds (default 30)\n"
"   -T  N		Reboot the system after N seconds if not reset (default 60)\n"
"   -g		 Close watchdog device gracefully with magic 'V' when rdb_watchdog is killed\n"
"   -w  name	 Specify the device name of the watchdog\n"
"   -r  FILE	 Specify a configuration file to register RDB variables\n"
"   -B  N		Install an optional commitment timer of N seconds, named BOOT\n";
static const char *help_message_part2 =
"   -l  FILE	 Specify a log file to record log messages\n"
"   -s		   Send log messages to syslogd\n"
"   -k		   Send log messages to klogd\n"
"   -v		   Increase the log level to produce more log messages\n";

static struct watchdog_context watchdog;

/*
 * kick the watchdog device
 */
static void watchdog_kick(struct watchdog_context *ctx)
{
	WDT_LOG_DEBUG("kicking watchdog\n");
	/* write zero byte */
	if (write(ctx->dev_fd, "", 1) < 0) {
		WDT_LOG_DEBUG("kicking fails\n");
	}
}

/*
 * Catch registered signals and then shut down the daemon.
 */
static void watchdog_shutdown(int sig)
{
	/*
	 * Magic close. Read watchdog-api.txt in kernel.
	 * In at91sam9 driver, 'V' would make driver stay pinging watchdog
	 * timer after the device is closed until watchdog is expired.
	 * This would leave the system some time to deal with some stuff
	 * after watchdog daemon is killed. For watchdog driver not supporting
	 * this value, the error is ignored, since it does no harm.
	*/
	static const char V = 'V';
	const struct watchdog_context *ctx = &watchdog;

	/* If it is a graceful shutdown, close the device with magic 'V'. */
	if (ctx->graceful_closing && write(ctx->dev_fd, &V, 1) < 0) {
		/* Nothing to do */
	}

	WDT_LOG_WARN("shutdown watchdog by signal(%d)\n", sig);
	exit(0);
}

/*
 * Reboot the system, never return.
 */
static void watchdog_reboot(struct watchdog_context *ctx, int rdb_error)
{
	/*
	 * Only use RDB if rdb_error is 0.
	 * If RDB is in an error state, it is not safe to use RDB.
	 */
	if (rdb_error == 0) {

		char cause[MAX_RDB_VALUE_LEN] = "unknown!";
		char last_cause[MAX_RDB_VALUE_LEN];
		int error;

		WDT_LOG_DEBUG("watchdog_reboot\n");

		if (ctx->rdb_session == NULL) {
			WDT_LOG_DEBUG("watchdog_reboot: ctx->rdb_session == NULL\n");
			rdb_open(NULL, &ctx->rdb_session);
		}

		if (ctx->rdb_session != NULL) {

			int last_reboot_count = 0;
			int max_reboot_count = INT_MAX;
			char value[MAX_RDB_VALUE_LEN];

			error = rdb_get_string(ctx->rdb_session, SYS_RDB_REBOOT_CAUSE, cause, sizeof(cause));

			WDT_LOG_DEBUG("watchdog_reboot: rdb_get_string get %s : %s or error = %d\n", SYS_RDB_REBOOT_CAUSE, cause, error);

#ifdef V_RESET_LOGGER_y
			writeResetReasonFormatted( "RDB Watchdog, cause: %s", cause );
#endif

			/*
			 * Read the last value in sys.watchdog.last_reboot_cause.
			 * If it is the same as sys.watchdog.reboot_cause, increment
			 * sys.watchdog.last_reboot_count.
			 * Otherwise, set sys.watchdog.last_reboot_count to 0.
			 */

			if ((rdb_get_string(ctx->rdb_session, SYS_RDB_LAST_REBOOT_CAUSE, last_cause, sizeof(last_cause)) == 0) &&
			    (strcmp(last_cause, cause) == 0) &&
			    (rdb_get_int(ctx->rdb_session, SYS_RDB_LAST_REBOOT_COUNT, &last_reboot_count) == 0)) {
				last_reboot_count++;
			}

			sprintf(value, "%d", last_reboot_count);
			error = rdb_set_string(ctx->rdb_session, SYS_RDB_LAST_REBOOT_COUNT, value);
			WDT_LOG_DEBUG("watchdog_reboot: rdb_set_string set %s : error = %d\n", SYS_RDB_LAST_REBOOT_COUNT, error);

			/*
			 * Copy sys.watchdog.reboot_cause to sys.watchdog.last_reboot_cause.
			 *
			 * This variable is only deleted after several 'good' reboots, so, 
			 * if it is able to be written, the last cause can be examined to
			 * diagnose faults causing watchdog reboots.
			 *
			 * Create sys.watchdog.last_reboot_cause with "unknown!" if
			 * sys.watchdog.reboot_cause cannot be read.
			 */

			error = rdb_set_string(ctx->rdb_session, SYS_RDB_LAST_REBOOT_CAUSE, cause);
			WDT_LOG_DEBUG("watchdog_reboot: rdb_set_string set %s : error = %d\n", SYS_RDB_LAST_REBOOT_CAUSE, error);

			/*
			 * In order to prevent unlimited consecutive reboots with the same cause,
			 * if there is a value in sys.watchdog.max_reboot_count
			 * and sys.watchdog.last_reboot_count is >
			 * sys.watchdog.max_reboot_count,
			 * do not allow the watchdog to expire.
			 */

		    if ((rdb_get_int(ctx->rdb_session, SYS_RDB_MAX_REBOOT_COUNT, &max_reboot_count) == 0) &&
		        (last_reboot_count >= max_reboot_count)) {

				char cmd[PATH_MAX];
				WDT_LOG_CRIT("watchdog_reboot: watchdog reboot due to %s is supressed since %s (%d) is >= %s (%d). \n",
							 cause,
							 SYS_RDB_LAST_REBOOT_COUNT, last_reboot_count,
							 SYS_RDB_MAX_REBOOT_COUNT, max_reboot_count);

				/*
				 * Remove the watchdog from the queue to prevent reboot loops.
				 *
				 * It may still be readded by external code, perhaps at the next system startup.
				 */

				sprintf(cmd, "wdt_commit del %s", cause);

				system(cmd);
				return;
			}

			/*
			 * If the cause is not "SHUTDOWN", try to invoke /sbin/reboot rather than
			 * allowing the system to be (ungracefully) reset.
			 *
			 * This will allow filesystems to be closed nicely.
			 */
			if (strcmp(cause, "SHUTDOWN") != 0) {
				WDT_LOG_DEBUG("watchdog_reboot: attempting to invoke /sbin/reboot\n");
				error = system("/sbin/reboot");
				WDT_LOG_DEBUG("watchdog_reboot: /sbin/reboot returned %d\n", error);

				/*
				* There is a possibility of system hang if system(reboot) call fails though
				* it is very unlikely. To prevent this, wait here for certain time period
				* until the system is rebooted by reboot system call while kicking watchdog.
				* If the system does not reboot due to uncertain error then walk through
				* following codes lines after timeout and wait forever until watchdog bites.
				*/
				{
					int reboot_wait = DEFAULT_WATCHDOG_REBOOT_WAITING_TIME;

					/*
					 * Notify to user space we are waiting for system rebooting or
					 * watchdog reset.
					 */
					if (rdb_create_string(ctx->rdb_session, SYS_RDB_WAITING_REBOOT, "1", CREATE, ALL_PERM) == 0) {
						WDT_LOG_INFO("watchdog_reboot: %s is created and set to 1, notifying user space\n", SYS_RDB_WAITING_REBOOT);
					} else {
						WDT_LOG_INFO("watchdog_reboot: Failed to create %s, can't notify to user space\n", SYS_RDB_WAITING_REBOOT);
					}

					WDT_LOG_INFO("watchdog_reboot: reboot waiting timer set to %d\n", reboot_wait);
					while (reboot_wait > 0) {
						watchdog_kick(ctx);
						sleep(1);
						reboot_wait -= 1;
						WDT_LOG_INFO("watchdog_reboot: remaining reboot waiting time %d\n", reboot_wait);
					}
				}

			}

			rdb_close(&ctx->rdb_session);
			ctx->rdb_session = 0;
		}

		WDT_LOG_CRIT("watchdog_reboot: watchdog reboot due to %s...\n", cause);

		/*
		 * Send SIGTERM to all rdb_managers. This flushes the final writes to flash.
		 */

		WDT_LOG_DEBUG("watchdog_reboot: rdb_manager commiting\n");
		system("/usr/bin/killall -HUP rdb_manager");
		sleep(10);

		WDT_LOG_DEBUG("watchdog_reboot: terminating rdb_manager\n");
		system("/usr/bin/killall rdb_manager");
		sync();

		WDT_LOG_DEBUG("watchdog_reboot: wait for reset from hardware watchdog\n");
	}

	/* keep looping until reboot */
	while (1) {
		sleep(1);
	}
}

/*
 * Register signals should be caught to shut down the daemon.
 */
static void watchdog_init_signals(void)
{
	signal(SIGHUP, watchdog_shutdown);
	signal(SIGINT, watchdog_shutdown);
	signal(SIGTERM, watchdog_shutdown);
	signal(SIGPIPE, watchdog_shutdown);
	signal(SIGQUIT, watchdog_shutdown);
	signal(SIGABRT, watchdog_shutdown);
	signal(SIGALRM, watchdog_shutdown);
	signal(SIGVTALRM, watchdog_shutdown);
	signal(SIGXCPU, watchdog_shutdown);
	signal(SIGXFSZ, watchdog_shutdown);
	signal(SIGUSR1, watchdog_shutdown);
	signal(SIGUSR2, watchdog_shutdown);
}

/*
 * show help messages.
 */
static void usage(void)
{
	WDT_LOG_ERR(help_message_part1);
	WDT_LOG_ERR(help_message_part2);
}

/*
 * intialise the watchdog device.
 */
static void watchdog_init_dev(struct watchdog_context *ctx)
{
	ctx->dev_fd = open(ctx->dev_name, O_WRONLY);
	if (ctx->dev_fd < 0) {
		WDT_LOG_ERR("failed to open the watchdog device\n");
		exit(-1);
	}

#ifndef WDIOC_SETTIMEOUT
# error WDIOC_SETTIMEOUT is not defined, cannot compile watchdog applet
#else
# if defined WDIOC_SETOPTIONS && defined WDIOS_ENABLECARD
	{
		static const int enable = WDIOS_ENABLECARD;
		if (ioctl(ctx->dev_fd, WDIOC_SETOPTIONS, (void*) &enable) < 0) {
			/* Some watchdog drivers may not support this feature */
			WDT_LOG_WARN("failed to enable watchdog\n");
		}
	}
# endif
	if (ioctl(ctx->dev_fd, WDIOC_SETTIMEOUT, &ctx->expire_duration) < 0) {
		/* Some watchdog drivers may not support this feature */
		WDT_LOG_WARN("failed to set timeout on watchdog\n");
	}
#endif
}

/*
 * This function has following responsibilites.
 *  - check whether the RDB works well.
 *  - check whether there is any reboot request.
 *  - Count the number down if the positive value is set to the variable.
 *  - Reboot the system if the value is 0 or the RDB is something wrong.
 */
static void watchdog_monitor_sys_rdb(struct watchdog_context *ctx)
{
	int retry_time;
	int timeout;
	int write_ok;
	int rdb_error = 0;
	char value[MAX_RDB_VALUE_LEN];

	retry_time = 0;
	if (rdb_lock(ctx->rdb_session, 0) == 0) {
		timeout = -1;
		if (rdb_get_int(ctx->rdb_session, SYS_RDB_TIMER_VAR, &timeout) == 0) {
			if (timeout == -1) {
				retry_time = 0;
			} else if (timeout == 0) {
				WDT_LOG_ERR("%s expires\n", SYS_RDB_TIMER_VAR);
				goto reboot_lock;
			} else if (timeout > 0) {
				write_ok = 0;
				/* substract the kick duration from the timeout and write it back */
				timeout = (timeout > ctx->kick_duration) ? (timeout - ctx->kick_duration) : 0;
				sprintf(value, "%d", timeout);
				if (rdb_set_string(ctx->rdb_session, SYS_RDB_TIMER_VAR, value) == 0) {
					int written_timeout = -1;

					/* Ensure the timeout is written to RDB well */
					if (rdb_get_string(ctx->rdb_session, SYS_RDB_TIMER_VAR, value, sizeof(value)) == 0) {
						if (sscanf(value, "%d", &written_timeout) == 1) {
							if (timeout == written_timeout) {
								WDT_LOG_INFO("RDB write succeeds(%d)\n");
								write_ok = 1;
							}
						}
					}
				}

				if (!write_ok) {
					/*
					 * Reboot the system if the write fails, we cannot depend on the RDB
					 * any more for the reboot
					 */
					WDT_LOG_ERR("%s is unable to write\n", SYS_RDB_TIMER_VAR);
					rdb_error = 1;
					goto reboot_lock;
				}
			} else {
				WDT_LOG_ERR("%s is out of range(%d)\n", SYS_RDB_TIMER_VAR, timeout);
				retry_time = ctx->sys_rdb_retry_time +  ctx->kick_duration;
			}
		} else {
			/* getting the value fails */
			WDT_LOG_ERR("%s is unable to read\n", SYS_RDB_TIMER_VAR);
			retry_time = ctx->sys_rdb_retry_time + ctx->kick_duration;
			rdb_error = 1;
		}
		rdb_unlock(ctx->rdb_session);
	} else {
		/* locking fails */
		WDT_LOG_ERR("RDB Lock fails(%d)\n");
		retry_time = ctx->sys_rdb_retry_time + ctx->kick_duration;
		rdb_error = 1;
	}

	if (retry_time > ctx->max_sys_rdb_retry_time) {
		WDT_LOG_ERR("sys.watchdog.timeToReboot is NOT accessible\n");
		goto reboot_end;
	}

	ctx->sys_rdb_retry_time = retry_time;
	WDT_LOG_DEBUG("monitor reboot timeout:%d\n", timeout);

	return;

reboot_lock:
	rdb_unlock(ctx->rdb_session);
reboot_end:
	watchdog_reboot(ctx, rdb_error);
}

/*
 * add a RDB variable that should be monitored whether it changes the value.
 */
static void watchdog_add_rdb_vars(struct watchdog_context *ctx, const char *name, int duration)
{
	int i;

	/* check the variable whether it already exists, update the duration
	 * if it does */
	for (i = 0; i < ctx->nr_rdb_vars; i++) {
		if (!strcmp(ctx->rdb_vars[i].name, name)) {
			WDT_LOG_DEBUG("RDB Var %s changes duration from %d to %d\n",
					name, ctx->rdb_vars[i].duration, duration);
			ctx->rdb_vars[i].duration = duration;
			return;
		}
	}

	if (ctx->nr_rdb_vars < WATCHDOG_MONITOR_RDB_VAR_NUM) {
		ctx->rdb_vars[ctx->nr_rdb_vars].name = malloc(strlen(name) + 1);
		if (ctx->rdb_vars[ctx->nr_rdb_vars].name) {
			strcpy(ctx->rdb_vars[ctx->nr_rdb_vars].name, name);
			ctx->rdb_vars[ctx->nr_rdb_vars].duration = duration;
			ctx->rdb_vars[ctx->nr_rdb_vars].active = 0;
			ctx->nr_rdb_vars++;
		} else {
			WDT_LOG_ERR("RDB Var %s, malloc fails\n", name);
		}
	} else {
		WDT_LOG_WARN("RDB Var %s, too many variables added\n", name);
	}
}

/*
 * check whether new configured variables are available to start monitoring.
 */
static void watchdog_check_rdb_vars(struct watchdog_context *ctx)
{
	int i;
	int rc;
	int alloc, flags, perms;

	for (i = 0; i < ctx->nr_rdb_vars; i++) {
		/* A process is considered active if its variable exists */
		alloc = flags = perms = 0;
		rc = rdb_getinfo(ctx->rdb_session, ctx->rdb_vars[i].name, &alloc, &flags, &perms);
		if (((rc == 0) || (rc == -EOVERFLOW)) && (alloc > 0)) {
			if (!ctx->rdb_vars[i].active) {
				ctx->rdb_vars[i].active = 1;
				WDT_LOG_INFO("RDB Var %s is active now\n", ctx->rdb_vars[i].name);
			}
		} else {
			if (ctx->rdb_vars[i].active) {
				ctx->rdb_vars[i].active = 0;
				ctx->rdb_vars[i].len = 0;
				ctx->rdb_vars[i].elapsed_time = 0;
				WDT_LOG_INFO("RDB Var %s has become inactive\n", ctx->rdb_vars[i].name);
			}
		}
	}
}

/*
 * This function has following responsibilites.
 *  - check whether there are new active variables to be monitored.
 *  - check whether the active variables changes their value.
 *  - reboot the system if there are any active variables that don't change.
 *	their value for their timeout period.
 */
static void watchdog_monitor_app_rdb(struct watchdog_context *ctx)
{
	int i;
	int rc;
	int changed;
	int value_len;
	/*
	 * RDB errors are checked in watchdog_monitor_sys_rdb, so assume no error in this function.
	 */
	int rdb_error = 0;

	char value[MAX_RDB_VALUE_LEN];

	if (!ctx->nr_rdb_vars) {
		/* Nothing to monitor */
		return;
	}

	/* check the configured rdb variables newly appeared */
	watchdog_check_rdb_vars(ctx);

	for (i = 0; i < ctx->nr_rdb_vars; i++) {
		if (!ctx->rdb_vars[i].active) {
			/* ignore the variable if it is configured but not created yet */
			continue;
		}

		WDT_LOG_DEBUG("Monitor RDB Var: %s Duration: %d, Elapsed:%d\n",
				ctx->rdb_vars[i].name, ctx->rdb_vars[i].duration,
				ctx->rdb_vars[i].elapsed_time);

		changed = 0;
		value_len = sizeof(value);
		rc = rdb_get(ctx->rdb_session, ctx->rdb_vars[i].name, value, &value_len);
		if (rc == 0) {
			if ((ctx->rdb_vars[i].len != value_len) ||
				memcmp(ctx->rdb_vars[i].value, value, value_len)) {
				memcpy(ctx->rdb_vars[i].value, value, value_len);
				ctx->rdb_vars[i].len = value_len;
				changed = 1;
			}
		}

		if (changed) {
			ctx->rdb_vars[i].elapsed_time = 0;
		} else {
			if (ctx->rdb_vars[i].elapsed_time > ctx->rdb_vars[i].duration) {
				WDT_LOG_ERR("Reboot RDB Var: %s Elapsed:%d\n", ctx->rdb_vars[i].name,
						ctx->rdb_vars[i].elapsed_time);
				watchdog_reboot(ctx, rdb_error);
			} else {
				ctx->rdb_vars[i].elapsed_time += ctx->kick_duration;
			}
		}
	}
}

#ifdef V_STARTUP_WATCHDOG_y
/*
 * Try to load the commitment timer settings.
 * Initially, RDB will not be populated, so several retries may be required.
 */
static void watchdog_check_boot_override(struct watchdog_context *ctx)
{
	int commit_enabled;

	int error;

	/*
	 * If sys.watchdog.commitment_timers.enable can be read and is 0,
	 * override ctx->commit_enabled.
	 *
	 * If RDB isn't ready, just wait until it can be read.
	 */

	WDT_LOG_DEBUG("RDB Watchdog: watchdog_check_boot_override\n");

	commit_enabled = -1;
	error = rdb_get_int(ctx->rdb_session, SYS_RDB_COMMITMENT_TIMERS_BOOT_ENABLE, &commit_enabled);
	if (error == 0) {

		/* It is possible to succesfully read from RDB, so don't try this again */
		ctx->boot_override_applied = 1;

		WDT_LOG_DEBUG("%s has the value : %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_ENABLE, commit_enabled);

		if (commit_enabled == 0) {
			/*
			 * The sys.watchdog.commitment_timers.enable variable has specified that the BOOT commitment timer be disabled.
			 */

			static const char cmd[] = "wdt_commit del BOOT";
			int result;

			result = system(cmd);
			WDT_LOG_DEBUG("system(%s) returned %d\n", cmd, result);

		} else if (commit_enabled == 1) {

			int commit_duration = -1;
			int commit_max_reboot_count = INT_MAX;
			if (rdb_get_int(ctx->rdb_session, SYS_RDB_COMMITMENT_TIMERS_BOOT_TIMEOUT, &commit_duration) == 0) {

				WDT_LOG_DEBUG("%s has the value : %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_TIMEOUT, commit_duration);

				error = rdb_get_int(ctx->rdb_session, SYS_RDB_COMMITMENT_TIMERS_BOOT_MAX_REBOOTS, &commit_max_reboot_count);
				WDT_LOG_DEBUG("watchdog_check_boot_override: rdb_get_int get %s : %d or error = %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_MAX_REBOOTS, commit_max_reboot_count, error);

				if ((commit_duration >= 30) && (commit_duration <= 3600)) {

					if ((commit_duration != ctx->commit_duration) ||
					    (commit_max_reboot_count != INT_MAX)) {
						/*
						 * The sys.watchdog.commitment_timers.enable variable has specified
						 * that the BOOT commitment timer is enabled: delete and re-add it.
						 */

						char cmd[PATH_MAX];
						int result;

						sprintf(cmd, "wdt_commit readd BOOT %d %d", commit_duration, commit_max_reboot_count);
						result = system(cmd);
						WDT_LOG_DEBUG("system(%s) returned %d\n", cmd, result);
					} else {
						WDT_LOG_DEBUG("commit_duration is the same as the command line value: leaving unchanged.\n");
					}
				} else {
					WDT_LOG_ERR("%s has an invalid value: %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_TIMEOUT, commit_duration);
				}
			}
		} else {
			WDT_LOG_WARN("%s has an invalid value: %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_ENABLE, commit_enabled);
		}

	} /* else can't read the rdb values (yet). Default behaviour specified on the command line is used until the values can be read. */
	else {
		WDT_LOG_DEBUG("cannot read %s: error is %d\n", SYS_RDB_COMMITMENT_TIMERS_BOOT_ENABLE, error);
	}
}
#endif /* #ifdef V_STARTUP_WATCHDOG_y */

/*
 * monitor system and application RDB variables.
 */
static void watchdog_rdb_run(struct watchdog_context *ctx)
{
	int retry_time = 0;
	/*
	 * If a reboot is triggered in this function, it is because there is an
	 * error opening an RDB session.
	 */
	int rdb_error = 1;

	if (rdb_open(NULL, &ctx->rdb_session) == 0) {
		if (ctx->rdb_session) {

#ifdef V_STARTUP_WATCHDOG_y
			if (!ctx->boot_override_applied) {
				watchdog_check_boot_override(ctx);
			}
#endif

			watchdog_monitor_sys_rdb(ctx);
			watchdog_monitor_app_rdb(ctx);
			rdb_close(&ctx->rdb_session);
		} else {
			retry_time = ctx->rdb_open_retry_time + ctx->kick_duration;
		}
	} else {
		retry_time = ctx->rdb_open_retry_time + ctx->kick_duration;
	}

	if (retry_time > MAX_RDB_OPEN_RETRY_TIME) {
		WDT_LOG_ERR("RDB open fails\n");
		watchdog_reboot(ctx, rdb_error);
	}
	ctx->rdb_open_retry_time = retry_time;
}

/*
 * Install a bootup commitment timer if one was specified
 * on the command line, otherwise populate RDB with an
 * empty timer queue.
 */
static int watchdog_init_timer(struct watchdog_context *ctx)
{
	int var_ok = 0;

	if (ctx->commit_enabled) {
		/* Populate RDB with the specified timer. */
		char buf[100] = {0};
		snprintf(buf, sizeof(buf), "%d", ctx->commit_duration);
		if (rdb_create_string(ctx->rdb_session, SYS_RDB_TIMER_VAR, buf, CREATE, ALL_PERM) == 0) {
			WDT_LOG_INFO("%s is created\n", SYS_RDB_TIMER_VAR);
			snprintf(buf, sizeof(buf), SYS_RDB_QUEUE_ENABLED, ctx->commit_duration);
			if (rdb_create_string(ctx->rdb_session, SYS_RDB_QUEUE_VAR, buf, CREATE, ALL_PERM) == 0) {
				WDT_LOG_INFO("%s is created\n", SYS_RDB_QUEUE_VAR);
				var_ok = 1;

				if (rdb_create_string(ctx->rdb_session, SYS_RDB_REBOOT_CAUSE, "BOOT", CREATE, ALL_PERM) != 0) {
					WDT_LOG_ERR("Unable to set reboot cause for inital \"BOOT\" entry in watchdog queue\n");
				}
			}
		}
	} else {
		/* Populate RBD with empty timer queue. */
		if (rdb_create_string(ctx->rdb_session, SYS_RDB_TIMER_VAR, SYS_RDB_TIMER_DEFAULT, CREATE, ALL_PERM) == 0) {
			WDT_LOG_INFO("%s is created\n", SYS_RDB_TIMER_VAR);
			if (rdb_create_string(ctx->rdb_session, SYS_RDB_QUEUE_VAR, SYS_RDB_QUEUE_DEFAULT, CREATE, ALL_PERM) == 0) {
				WDT_LOG_INFO("%s is created\n", SYS_RDB_QUEUE_VAR);
				var_ok = 1;

				if (rdb_delete(ctx->rdb_session, SYS_RDB_REBOOT_CAUSE) != 0) {
					WDT_LOG_ERR("Unable to delete reboot cause for default (empty) watchdog queue\n");
				}
			}
		}
	}

	return var_ok;
}

/*
 * check the RDB is ready after start-up.
 */
static void watchdog_rdb_idle(struct watchdog_context *ctx)
{
	/*
	 * If a reboot is triggered in this function, it is because there is an
	 * error opening an RDB session or creating an RDB variable.
	 */
	int rdb_error = 1;

	if (ctx->rdb_ready_time < MAX_RDB_READY_TIME) {
		if (rdb_open(NULL, &ctx->rdb_session) == 0) {
			int rc;
			int alloc, flags, perms;
			int var_ok = 0;

			/* sys_rdb variable may exist if the daemon restarts */
			rc = rdb_getinfo(ctx->rdb_session, SYS_RDB_TIMER_VAR, &alloc, &flags, &perms);
			if (((rc == 0) || (rc == -EOVERFLOW)) && (alloc > 0)) {
				var_ok = 1;
				WDT_LOG_INFO("%s already exists\n", SYS_RDB_TIMER_VAR);
			} else {
				var_ok = watchdog_init_timer(ctx);
			}
			if (var_ok) {
				if (ctx->rdbvars_filename) {
					watchdog_read_rdb_config(ctx, ctx->rdbvars_filename);
				}
				ctx->rdb_create_retry_time = 0;
				ctx->rdb_running = 1;
			} else {
				if (ctx->rdb_create_retry_time > MAX_RDB_CREATE_RETRY_TIME) {
					WDT_LOG_ERR("RDB create variable fails\n");
					watchdog_reboot(ctx, rdb_error);
				} else {
					WDT_LOG_INFO("RDB create variable fails: %d secs\n", ctx->rdb_create_retry_time);
					ctx->rdb_create_retry_time = ctx->rdb_create_retry_time + ctx->kick_duration;
				}
			}
			rdb_close(&ctx->rdb_session);
		} else {
			WDT_LOG_INFO("RDB open fails: %d secs\n", ctx->rdb_ready_time);
			ctx->rdb_ready_time = ctx->rdb_ready_time + ctx->kick_duration;
		}
	} else {
		WDT_LOG_ERR("RDB open was never successful\n");
		watchdog_reboot(ctx, rdb_error);
	}
}

/*
 * read RDB variables configuration from the specified file and then register them.
 */
static void watchdog_read_rdb_config(struct watchdog_context *ctx, const char *filename)
{
	FILE *file;
	int duration;
	char line[256];
	char fmt_str[16];
	char name[MAX_RDB_NAME_LEN];

	file = fopen(filename, "r");
	if (!file) {
		WDT_LOG_ERR("Cannot open the RDB config file %s\n", filename);
		return;
	}

	snprintf(fmt_str, sizeof(fmt_str), "%%%ds %%d", (MAX_RDB_NAME_LEN - 1));
        while (fgets(line, sizeof(line), file)) {
		if ((line[0] != '#') && line[0]) {
			if (sscanf(line, fmt_str, name, &duration) == 2) {
                                watchdog_add_rdb_vars(ctx, name, duration);
			}
		}
	}

	fclose(file);
}

/*
 * initialise the daemon before getting options.
 */
static void watchdog_pre_init(struct watchdog_context *ctx)
{
	int i;

	ctx->dev_name = DEFAULT_WATCHDOG_DEVICE;

	ctx->kick_duration = 0;
	ctx->expire_duration = DEFAULT_EXPIRE_DURATION;
	ctx->commit_enabled = 0;
	ctx->commit_duration = 0;

#ifdef V_STARTUP_WATCHDOG_y
	ctx->boot_override_applied = 0;
#endif

	ctx->rdb_ready_time = 0;
	ctx->rdb_running = 0;
	ctx->rdb_session = NULL;
	ctx->nr_rdb_vars = 0;
	for (i = 0; i < WATCHDOG_MONITOR_RDB_VAR_NUM; i++) {
		ctx->rdb_vars[i].name = NULL;
		ctx->rdb_vars[i].active = 0;
	}

	ctx->sys_rdb_retry_time = 0;
	ctx->max_sys_rdb_retry_time = MAX_RDB_MAIN_RDB_RETRY_TIME;
	ctx->app_rdb_retry_time = 0;
	ctx->max_app_rdb_retry_time = MAX_RDB_APP_RDB_RETRY_TIME;

	ctx->log_channels = 0;
	ctx->log_level = DEFAULT_LOG_LEVEL;
	ctx->log_filename = NULL;

	ctx->rdbvars_filename = NULL;

	/* initialise the logger to log messages to the kernel */
	logger_init("rdb_watchdog", WATCHDOG_LOG_KMSG, WATCHDOG_LOG_DEBUG_PRIO, NULL);
}

/*
 * show the configured options.
 */
static void watchdog_show_options(struct watchdog_context *ctx)
{
	WDT_LOG_INFO("dev name: %s\n", ctx->dev_name);
	WDT_LOG_INFO("kick duration: %d\n", ctx->kick_duration);
	WDT_LOG_INFO("expire duration: %d\n", ctx->expire_duration);
	WDT_LOG_INFO("commit duration: %d\n", ctx->commit_duration);
	WDT_LOG_INFO("RDB config file: %s\n", ctx->rdbvars_filename);
	WDT_LOG_INFO("log file: %s\n", ctx->log_filename);
	WDT_LOG_INFO("log level: %d\n", ctx->log_level);
	WDT_LOG_INFO("log channels: %d\n", ctx->log_channels);
}

/*
 * initialise the daemon after getting all options.
 */
static void watchdog_post_init(struct watchdog_context *ctx)
{
	/* terminates the kernel logging and then initialises the logger with proper options */
	logger_term();
	logger_init("rdb_watchdog", ctx->log_channels, ctx->log_level, ctx->log_filename);

	watchdog_show_options(ctx);

	/* The expire duration should be larger than 1 secs */
	if (ctx->expire_duration < 2) {
		WDT_LOG_ERR("Invalid watchdog expire duration: %d secs\n", ctx->expire_duration);
		ctx->expire_duration = DEFAULT_EXPIRE_DURATION;
	}

	if (ctx->kick_duration <= 0) {
		/* set the kicking duration if they are not set */
		ctx->kick_duration = ctx->expire_duration / 2;
	} else if (ctx->kick_duration > ctx->expire_duration) {
		WDT_LOG_ERR("Invalid watchdog kicking duration: %d secs\n", ctx->kick_duration);
		ctx->kick_duration = ctx->expire_duration / 2;
	}

	if (ctx->commit_enabled && (ctx->commit_duration < 30 || ctx->commit_duration > 3600)) {
		WDT_LOG_ERR("Invalid commitment timer duration: %d secs\n", ctx->commit_duration);
		ctx->commit_duration = 600;
	}

	/*
	 * ensure that more than 5 times should be checked before it determines
	 * that RDB is something wrong
	 */
	if (ctx->max_sys_rdb_retry_time < (ctx->kick_duration * MIN_RDB_RETRIES)) {
		ctx->max_sys_rdb_retry_time = ctx->kick_duration * MIN_RDB_RETRIES;
	}

	watchdog_init_signals();
	watchdog_init_dev(ctx);
}

int main(int argc, char **argv)
{
	int ch;
	struct watchdog_context *ctx = &watchdog;

	/*
	 * If a reboot is triggered in this function, it is for an unknown reason.
	 */
	int rdb_error = 0;

	watchdog_pre_init(ctx);
	while ((ch = getopt(argc, argv, "hsvgt:T:w:r:l:k:B:")) != -1) {
		switch (ch) {
			case 't':
				ctx->kick_duration = atoi(optarg);
				break;

			case 'T':
				ctx->expire_duration = atoi(optarg);
				break;

			case 'B':
				ctx->commit_enabled = 1;
				ctx->commit_duration = atoi(optarg);
				break;
			case 'g':
				ctx->graceful_closing = 1;
				break;

			case 'w':
				ctx->dev_name = optarg;
				break;

			case 'r':
				ctx->rdbvars_filename = optarg;
				break;

			case 'l':
				ctx->log_filename = optarg;
				ctx->log_channels |= WATCHDOG_LOG_FILE;
				break;

			case 's':
				ctx->log_channels |= WATCHDOG_LOG_SYSLOG;
				break;

			case 'k':
				ctx->log_channels |= WATCHDOG_LOG_KMSG;
				break;

			case 'v':
				ctx->log_level++;
				break;

			default:
				usage();
				break;
		}
	}
	watchdog_post_init(ctx);

	WDT_LOG_ERR("RDB Watchdog: main\n");

	while (1) {
		/* open new session each polling */
		ctx->rdb_session = NULL;

		if (ctx->rdb_running) {
			watchdog_rdb_run(ctx);
		} else {
			watchdog_rdb_idle(ctx);
		}
		watchdog_kick(ctx);

		sleep(ctx->kick_duration);
	}

	/* Unexpected place, but just reboot */
	WDT_LOG_ERR("Nothing to Run\n");
	watchdog_reboot(ctx, rdb_error);

	return 0;
}

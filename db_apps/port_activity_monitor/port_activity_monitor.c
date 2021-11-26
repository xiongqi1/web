/*
 * Daemon to monitor activity on network ports.
 *
 * A list of network ports can be created in initialise_port_activity_monitors
 * naming an rdb variable containing a comma separated list of network ports
 * to monitor.
 *
 * update_port_activity_monitors is regularly called to monitor
 * activity in the sysfs file
 * /sys/class/net/<network_port>/statistics/rx_bytes for
 * the list of network ports.
 *
 * The watchdogs are fed regularly, provided there is activity on
 * the above file for each monitored network port.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "port_activity_monitor.h"
#include "rdb_ops.h"
#include "daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#define SYS_RDB_PACKET_MONITOR_LIST	"sys.watchdog.packet_monitor.network_ports"
#define SYS_RDB_PACKET_MONITOR_ENABLE		"sys.watchdog.packet_monitor.%s.enable"
#define SYS_RDB_PACKET_MONITOR_TIMEOUT		"sys.watchdog.packet_monitor.%s.timeout"
#define SYS_RDB_PACKET_MONITOR_MAX_REBOOTS	"sys.watchdog.packet_monitor.%s.max_reboots"
#define SYS_RDB_PACKET_MONITOR_RX_BYTES		"sys.watchdog.packet_monitor.%s.rx_bytes"
#define SYS_RDB_PACKET_MONITOR_TRIGGER		"sys.watchdog.packet_monitor.trigger"
#define SYS_RDB_IGNITION_OFF				"service.powersave.ignition_off"

#define PACKET_WATCHDOG_NAME				"PACKET_WATCHDOG"
#define DAEMON_NAME       "packet_monitord"

#define log_ERR(m, ...) syslog(LOG_ERR, m, ## __VA_ARGS__)
#define log_INFO(m, ...) {if (glb.verbosity) syslog(LOG_INFO, m, ## __VA_ARGS__);}
#define log_DEBUG(m, ...) {if (glb.verbosity) syslog(LOG_DEBUG, m, ## __VA_ARGS__);}

/*
 * Global variables shared between all functions.
 */
struct _glb {
	int		run;
	int		verbosity;

	/*
	 * RDB Session to ensure thread safe access to RDB functions.
	 */
	struct rdb_session* rdb_session;

	/*
	 * Linked list of port_activity_monitor s containing settings and
	 * state for each port monitored.
	 */
	port_activity_monitor* port_activity_monitor_list_head;

	/*
	 * Saved value of service.powersave.ignition_off.
	 */
	int last_ignition_off;

	/*
	 * Current value of service.powersave.ignition_off.
	 */
	int ignition_off;
} glb;

#define MAX_SIZE 100
#define MAX_RDB_VAR_LEN	256
#define MAX_RDB_VAL_LEN 256

/*
 * Obtain time in seconds since an arbitrary point in time.
 * Not affected by changes in local system time (such as from
 * ntp changes).
 */

static time_t time_monotonic()
{
	struct timespec result;
	result.tv_sec = 0;
	result.tv_nsec = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &result) != 0) {
		/*
		 * Failed, fall back to time().
		 */

		 return time(0);
	}

	return result.tv_sec;
}

char    *optarg;

/*
 * In place remove a new line, spaces or tabs from the end of a string,
 * if such whitespace exists.
 */
static void trim_trailing_whitespace(char* string)
{
	static const char whitespace_chars[] = { '\n', '\r', ' ', '\t' };
	size_t index = 0;

	if (string != 0) {
		for (index = 0; index < sizeof(whitespace_chars); index++) {
			char whitespace_char = whitespace_chars[index];

			int len = strlen(string);
			while (len > 0) {
				if ((string[len - 1] == whitespace_char)) {
					string[len - 1] = '\0';
					len--;
				} else {
					break;
				}
			}
		}
	}
}

/*
 * Invoke the wdt_commit script to refresh the timeout of the
 * PACKET_MONITOR_<network_port> watchdog.
 */
static void commit(port_activity_monitor* monitor)
{
	time_t now = time_monotonic();

	if (now >= monitor->next_recommit_check_time) {
		/*
		 * Adjust the time of watchdog reboot to be
		 * monitor->activity_monitor_timeout_seconds reduced by
		 * the number of seconds since the last activity was detected.
		 */
		int seconds_since_last_activity = now - monitor->oldest_activity_time;
		if (seconds_since_last_activity < monitor->activity_monitor_timeout_seconds) {
			int next_timeout = monitor->activity_monitor_timeout_seconds - seconds_since_last_activity;
			char cmd[NAME_MAX];
			/* sprintf(cmd, "wdt_commit del %s_%s && wdt_commit add %s_%s %d %d",
				    PACKET_WATCHDOG_NAME, monitor->network_port,
				    PACKET_WATCHDOG_NAME, monitor->network_port,
				    next_timeout,
				    monitor->activity_monitor_max_reboots); */
			sprintf(cmd, "wdt_commit readd %s_%s %d %d",
				    PACKET_WATCHDOG_NAME, monitor->network_port,
				    next_timeout,
				    monitor->activity_monitor_max_reboots);
			system(cmd);

			/*
			 * don't invoke wdt_commit too often
			 */
			monitor->next_recommit_check_time = now + next_timeout / 2;

			log_DEBUG("(%d s) recommit %s_%s to %d (next check at = %d s)\n",
					  (int)now,
					  PACKET_WATCHDOG_NAME, monitor->network_port,
					  next_timeout,
					  (int)monitor->next_recommit_check_time);
		}
	}
}

/*
 * Allocate and and populate the structure with the settings for the
 * <network_port> watchdog and return the result.
 * Returns 0 if the memory for the structure cannot be allocated.
 * The memory allocated for the structure can be released with
 * uninitialise_port_activity_monitor.
 */
static port_activity_monitor* initialise_port_activity_monitor(const char* network_port,
															   port_activity_monitor* next)
{
	char rdb_variable_name[NAME_MAX];
	time_t now = time_monotonic();
	int error;

	port_activity_monitor* result = 0;
	port_activity_monitor* next_result;

	size_t network_port_length = (network_port == 0) ? 0 : strlen(network_port);

	if ((network_port_length == 0) ||
	    (strlen(network_port) >= (MAX_SIZE - 1))) {
		log_ERR("initialise_port_activity_monitor: Invalid network_port");
		return 0;
	}

	next_result = (port_activity_monitor*)malloc(sizeof(port_activity_monitor));

	if (next_result == 0) {
		log_ERR("initialise_port_activity_monitor: Memory allocation failure");
		uninitialise_port_activity_monitor_chain(result);
		result = 0;
	} else {
		strcpy(next_result->network_port, network_port);

		next_result->rx_last_activity_time = now;
		next_result->oldest_activity_time = now;

		sprintf(rdb_variable_name, SYS_RDB_PACKET_MONITOR_ENABLE, next_result->network_port);
		error = rdb_get_int(glb.rdb_session, rdb_variable_name, &next_result->activity_monitor_enabled);
		log_DEBUG("%s = %d (error = %d)\n", rdb_variable_name, next_result->activity_monitor_enabled, error);

		sprintf(rdb_variable_name, SYS_RDB_PACKET_MONITOR_TIMEOUT, next_result->network_port);
		error = rdb_get_int(glb.rdb_session, rdb_variable_name, (int*)&next_result->activity_monitor_timeout_seconds);
		log_DEBUG("%s = %d (error = %d)\n", rdb_variable_name, (int)next_result->activity_monitor_timeout_seconds, error);

		sprintf(rdb_variable_name, SYS_RDB_PACKET_MONITOR_MAX_REBOOTS, next_result->network_port);
		error = rdb_get_int(glb.rdb_session, rdb_variable_name, &next_result->activity_monitor_max_reboots);
		log_DEBUG("%s = %d (error = %d)\n", rdb_variable_name, next_result->activity_monitor_max_reboots, error);

		next_result->next_port_activity_monitor = next;

		/*
		 * Check sys.watchdog.packet_monitor.<network_port>.enable.
		 * If set to 1, start committing to the rdb watchdog PACKET_WATCHDOG_<network_port>.
		 */

		if (next_result->activity_monitor_enabled) {
			next_result->next_recommit_check_time = now;
			commit(next_result);
		}

		result = next_result;
	}

	return result;
}

port_activity_monitor* initialise_port_activity_monitors(const char* port_list_rdb_variable)
{
	char network_ports_string[MAX_RDB_VAL_LEN];
	port_activity_monitor* result = 0;

	if (rdb_get_string(glb.rdb_session, port_list_rdb_variable, network_ports_string, sizeof(network_ports_string)) < 0) {
		log_ERR("failed to read %s - %s", port_list_rdb_variable, strerror(errno));
	} else {
		/*
		 * In case initialise_port_activity_monitor uses strtok,
		 * save_ptr keeps this context and we use strtok_r.
		 */
		char* save_ptr = 0;

		char* tok = strtok_r(network_ports_string, ",", &save_ptr);
		while (tok != 0)
		{
			trim_trailing_whitespace(tok);
			result = initialise_port_activity_monitor(tok, result);
			tok = strtok_r(0, ",", &save_ptr);
		}
	}

	return result;
}

/*
 * Read the contents of the file at <file_path> into output_value.
 */

static int get_file_contents(char* file_path,
							 char* output_value,
							 size_t output_value_size) {
	size_t	bytes_read;

	/* Open the file for reading */
	if (file_path && *file_path) {
		FILE	*pf = fopen(file_path, "r");
		if(!pf) {
			log_ERR("Could not open file %s for reading.\n", file_path);
			return -1;
		}

		bytes_read = fread(output_value, 1, output_value_size, pf);
		if (bytes_read == 0) {
			// log_ERR("Read failed or buffer isn't big enough.\n");
			return -1;
		}

		output_value[bytes_read] = '\0';

		if (fclose(pf)) {
			log_INFO("Could not close file %s.\n", file_path);
		}
	}

	return 0;
}

/*
 * Update the rdb variable <rdb_variable_prefix>_<network_port> with
 * the contents of the file at "/sys/class/net/%s/statistics/<variable_name>.
 */
static int update_port_activity_value(port_activity_monitor* monitor,
									  const char* variable_name,
									  const char* rdb_variable_prefix)
{
	char value_path[PATH_MAX];
	char value_content[MAX_SIZE];
	int error = -1;

	sprintf(value_path, "/sys/class/net/%s/statistics/%s", monitor->network_port, variable_name);

	error = get_file_contents(value_path, value_content, sizeof(value_content));
	if (error == 0) {

		char existing_value[MAX_SIZE];
		char rdb_variable_name[MAX_SIZE];

		trim_trailing_whitespace(value_content);
		sprintf(rdb_variable_name, rdb_variable_prefix, monitor->network_port);
		existing_value[0] = '\0';
		error = rdb_get_string(glb.rdb_session, rdb_variable_name, existing_value, sizeof(existing_value));
		if ((error == 0) || (error == -ENOENT)) {

			/*
			 * error == -ENOENT the first time.
			 */
			error = 0;
			if (strcmp(value_content, existing_value) != 0) {
				rdb_update_string(glb.rdb_session, rdb_variable_name, value_content, 0, DEFAULT_PERM);
				error = 0;
			} else {
				// unchanged: don't record any activity timestamp.
				error = -2;
			}
		}
	} else {
		log_ERR("Could not obtain value for %s from sysfs.\n", value_path);
	}

	return error;
}

void update_port_activity_monitor(port_activity_monitor* monitor)
{
	if (monitor == 0) {
		log_ERR("update_port_activity_monitor: invalid port_activity_monitor.\n");
	} else
	{
		if (monitor->activity_monitor_enabled) {

			time_t now = time_monotonic();

			if (update_port_activity_value(monitor, "rx_bytes", SYS_RDB_PACKET_MONITOR_RX_BYTES) == 0) {
				/*
				 * The file contents has changed: set the time of the activity.
				 */
				monitor->rx_last_activity_time = now;
			}

			monitor->oldest_activity_time = monitor->rx_last_activity_time;

			log_DEBUG("(%d s) seconds since oldest activity for %s: %d\n",
					  (int)now,
					  monitor->network_port,
					  (int)(now - monitor->oldest_activity_time));

			commit(monitor);
		}
	}
}

void update_port_activity_monitors(port_activity_monitor* monitor)
{
	while (monitor != 0) {
		update_port_activity_monitor(monitor);
		monitor = monitor->next_port_activity_monitor;
	}
}

void uninitialise_port_activity_monitor_chain(port_activity_monitor* monitor)
{
	log_DEBUG("uninitialise_port_activity_monitor_chain\n");
	while (monitor != 0) {

		port_activity_monitor* next = monitor->next_port_activity_monitor;

		/*
		 * Remove the watchdog from the queue (if any).
		 */
		char cmd[NAME_MAX];
		sprintf(cmd, "wdt_commit del %s_%s", PACKET_WATCHDOG_NAME, monitor->network_port);
		system(cmd);

		free(monitor);
		monitor = next;
	}
}

static void sig_handler(int signum)
{
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
			log_ERR("caught signal %d", signum);
			signal(SIGHUP,  SIG_DFL);
			glb.run = 0;
			break;

		case SIGCHLD:
			break;
	}
}

void process_ignition_changed()
{
	int error;
	error = rdb_get_int(glb.rdb_session,
						SYS_RDB_IGNITION_OFF,
						&glb.ignition_off);

	log_DEBUG("%s = %d (error = %d)\n",
			  SYS_RDB_IGNITION_OFF,
			  glb.ignition_off,
			  error);

	if (glb.ignition_off != glb.last_ignition_off) {

		if (glb.ignition_off == 1) {
			/*
			 * If the ignition was on and is now off, cancel all port activity monitors.
			 */

			log_DEBUG("glb.ignition_off != glb.last_ignition_off - ignition is off");
			uninitialise_port_activity_monitor_chain(glb.port_activity_monitor_list_head);
			glb.port_activity_monitor_list_head = 0;
		} else {
			/*
			 * The ignition was off and is now on.
			 */

			log_DEBUG("glb.ignition_off != glb.last_ignition_off - ignition is on");
			glb.port_activity_monitor_list_head = initialise_port_activity_monitors(SYS_RDB_PACKET_MONITOR_LIST);
		}

		glb.last_ignition_off = glb.ignition_off;
	}
}

/*
 * Remove any existing packet monitors, then,
 * if the ignition is on, load all packet monitors.
 */
void reinitialise_all()
{
	uninitialise_port_activity_monitor_chain(glb.port_activity_monitor_list_head);
	glb.port_activity_monitor_list_head = 0;

	int error = rdb_get_int(glb.rdb_session,
							SYS_RDB_IGNITION_OFF,
							&glb.ignition_off);

	log_DEBUG("%s = %d (error = %d)\n",
			  SYS_RDB_IGNITION_OFF,
			  glb.ignition_off,
			  error);

	if (glb.ignition_off == 0)
	{
		/*
		 * The ignition is on.
		 */

		glb.port_activity_monitor_list_head = initialise_port_activity_monitors(SYS_RDB_PACKET_MONITOR_LIST);
	}

	glb.last_ignition_off = glb.ignition_off;
}

void process_rdb_trigger()
{
	/*
	 * The rdb variable SYS_RDB_IGNITION_OFF is read during reinitialise_all,
	 * which will reset the subscription trigger.
	 */

	/*
	 * Unload and load all variables.
	 */
	 reinitialise_all();
}

void process_triggered_variables()
{
	char names[NAME_MAX * 2];
	int names_len = sizeof(names);

	// get triggered rdbvar names
	if (rdb_getnames(glb.rdb_session, "", names, &names_len, TRIGGERED) < 0) {
		log_ERR("rdb_get_names() failed - %s", strerror(errno));
	} else {
		char* saveptr;
		char* token = NULL;

		names[names_len] = 0;

		log_DEBUG("notify list - %s", names);

		// get changed variable names
		while ((token = strtok_r((token == NULL) ? names : NULL, "&", &saveptr)) != NULL) {

			if (strcmp(token, SYS_RDB_IGNITION_OFF) == 0) {
				process_ignition_changed();
			} else if (strcmp(token, SYS_RDB_PACKET_MONITOR_TRIGGER) == 0) {
				process_rdb_trigger();
			}
		}
	}
}

int main(int argc, char **argv)
{
    int c;

	glb.run = 0;
	glb.rdb_session = 0;
	glb.port_activity_monitor_list_head = 0;
	glb.verbosity = 0;

	fd_set fdr;
	int rdb_file_descriptor;

    while ((c = getopt (argc, argv, "v:h")) != -1)
    {
		//printf("c=%c, optarg=%s\n", c, optarg);
        switch (c)
        {
		case 'v': /*"\t-v verbosity_level, default:0 - Log to syslog only \n"
                    "\t                       if vebosity is non 0 - Log to syslog and stderr \n"
                    "\t                       use -v 8 to enable debug level logging \n"*/
  			glb.verbosity = atoi(optarg);
			break;

        case 'h':
            fprintf(stdout, "%s is a daemon program to monitor network port activity\n"
                    "Usage: [-vh]\n"
                    "\t-v log_level, default:0 - Log to syslog only \n"
                    "\t            if vebosity is non 0 - Log to syslog and stderr \n"
                    "\t            use -v 8 to enable debug level logging \n"
                    "\t-h       -- display this screen\r\n",
                    argv[0]);
                   break;
		}
	}

	if (glb.verbosity == 0)
	{
		daemon_init(DAEMON_NAME, NULL, 1, LOG_NOTICE);
	}

	log_DEBUG("%s Starting\n", argv[0]);
	openlog(DAEMON_NAME, LOG_PID | LOG_PERROR, LOG_LOCAL5);

	glb.run = 1;
    signal(SIGHUP, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGCHLD, sig_handler);

	if ((rdb_open(NULL, &glb.rdb_session) != 0) ||
		(glb.rdb_session == 0)) {

		log_ERR("can't open rdb");
		abort();
		/* abort shouldn't return, but just in case. */
		return -1;
	}

	/*
	 * Subscribe to service.powersave.ignition_off
	 *
	 */

	if (rdb_subscribe(glb.rdb_session, SYS_RDB_IGNITION_OFF) != 0) {
		log_ERR("can't subscribe to %s", SYS_RDB_IGNITION_OFF);
		abort();
		/* abort shouldn't return, but just in case. */
		return -1;
	}

	if (rdb_subscribe(glb.rdb_session, SYS_RDB_PACKET_MONITOR_TRIGGER) != 0) {
		log_ERR("can't subscribe to %s", SYS_RDB_PACKET_MONITOR_TRIGGER);
		abort();
		/* abort shouldn't return, but just in case. */
		return -1;
	}

	rdb_file_descriptor = rdb_fd(glb.rdb_session);

	reinitialise_all();

	while (glb.run)
	{
		int selected;
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

		update_port_activity_monitors(glb.port_activity_monitor_list_head);

		FD_ZERO(&fdr);
		FD_SET(rdb_file_descriptor, &fdr);

		/*
		 * check for changes to subscribed variables or wait for timeout.tv_sec
		 * seconds.
		 */
		selected = select(rdb_file_descriptor + 1, &fdr, NULL, NULL, &timeout);

		if ((selected > 0) && FD_ISSET(rdb_file_descriptor, &fdr))
		{
			log_DEBUG("subscribed variable changed");
			process_triggered_variables();
		}
	}

	daemon_fini();

	log_DEBUG("%s exiting\n", argv[0]);

	rdb_close(&glb.rdb_session);
	uninitialise_port_activity_monitor_chain(glb.port_activity_monitor_list_head);
	glb.port_activity_monitor_list_head = 0;

	closelog();

	return 0;
}

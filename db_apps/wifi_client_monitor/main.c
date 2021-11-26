/*
 * wifi_client_monitor:
 * WiFi client monitor daemon. Communicating with wpa_supplicant to retrieve events
 * and messages related to WiFi client interfaces and update RDB variables.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/types.h>


#include <rdb_ops.h>
#include <wpa_ctrl.h>

#include "event_loop.h"
#include "wpa_supp.h"
#include "wpa_rdb.h"

#define printmsg(...) fprintf(stderr, __VA_ARGS__)

/* to send command */
static struct wpa_ctrl *ctrl_conn = NULL;
/* to monitor */
static struct wpa_ctrl *monitor_conn = NULL;

#define CTRL_IFACE_DIR "/var/run/wpa_supplicant"
static char control_file_path[64];

static void reconnect_wpa_supplicant(void);
static void wpa_monitor_receive(int fd, void *data);
static void close_wpa_supplicant_connection(void);
static int open_wpa_supplicant_connection(void);
static void try_connect_wpa_supplicant(void *data);

#define PING_WPA_SUPPLICANT_INTERVAL_SECS 5
#define RETRY_CONNECT_INTERVAL_SECS 1

/*
 * send a command to wpa_supplicant
 * @cmd: command to send
 *
 * returns 0 on success, -1 on error (send or receive failed), -2 on timeout
 */
static int send_wpa_supplicant_command(const char *cmd)
{
	/* reply buffer; wpa_supplicant defines reply buffer size 4096 bytes */
	char buf[4096];
	size_t len = sizeof(buf) - 1;
	int ret;

	if (!cmd){
		return -EINVAL;
	}

	if (ctrl_conn == NULL) {
		return -1;
	}

	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), buf, &len, NULL);
	if (ret == -2) {
		/* command timed out */
		return -2;
	} else if (ret < 0) {
		/* command failed */
		return -1;
	}

	/* Command is sent successfully and reply message is in buf.
	 * There should be a function to process this reply message
	 * if more commands are supported.
	 * example: print reply message
	 * buf[len] = '\0';
	 * printf("wpa_supplicant replies: %s\n", buf);
	 */
	return 0;
}

/* send periodic ping to wpa_supplicant */
static void send_periodic_ping_command(void *data)
{
	if (!ctrl_conn || (ctrl_conn && send_wpa_supplicant_command("PING") < 0)){
		syslog(LOG_WARNING, "Could not connect to wpa_supplicant: %s - re-trying\n", control_file_path);
		reconnect_wpa_supplicant();
	}
	else {
		event_loop_add_timer(PING_WPA_SUPPLICANT_INTERVAL_SECS, 0, send_periodic_ping_command, NULL);
	}
}

/*
 * Try to reconnect to wpa_supplicant.
 * If failed, setup a timer to try to connect every 1 second.
 */
static void reconnect_wpa_supplicant(void)
{
	close_wpa_supplicant_connection();

	try_connect_wpa_supplicant(NULL);
}

/*
 * handler function to read wpa_supplicant messages
 * @fd: file descriptor used by the wpa_supplicant monitor interface
 * @data: data passed to this handler
 */
static void wpa_monitor_receive(int fd, void *data)
{
	char buf[256];
	size_t buf_len = sizeof(buf) - 1;
	wpa_event_parse_t parse_ret;

	if (ctrl_conn == NULL) {
		reconnect_wpa_supplicant();
		return;
	}
	/* read, parse wpa_supplicant messages, and update rdb variables */
	while (wpa_ctrl_pending(monitor_conn) > 0) {
		if (wpa_ctrl_recv(monitor_conn, buf, &buf_len) == 0) {
			buf[buf_len] = '\0';
			parse_ret = parse_wpa_supplicant_msg(buf);
			update_rdb(parse_ret);
			if (parse_ret == PARSE_WPA_SUPP_TERMINATING){
				syslog(LOG_WARNING, "wpa_supplicant is terminating!\n");
				return;
			}
		}
		else {
			syslog(LOG_DEBUG, "Could not read pending message.\n");
			break;
		}
	}

	if (wpa_ctrl_pending(monitor_conn) < 0) {
		syslog(LOG_WARNING, "Connection to wpa_supplicant lost - trying to reconnect\n");
		/* reset rdb variables */
		reset_rdb();
		/* try to reconnect to wpa_supplicant */
		reconnect_wpa_supplicant();
	}
}

/* close connection to wpa_supplicant */
static void close_wpa_supplicant_connection(void)
{
	if (ctrl_conn == NULL)
		return;
	/* detach monitored control interface */
	wpa_ctrl_detach(monitor_conn);
	/* delete monitor fd from event loop */
	if (monitor_conn) {
		event_loop_del_fd(wpa_ctrl_get_fd(monitor_conn));
		wpa_ctrl_close(monitor_conn);
		monitor_conn = NULL;
	}
	/* delete ping timer */
	event_loop_del_timer(send_periodic_ping_command, NULL);
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
}

/*
 * Open connection to wpa_supplicant.
 * Returns 0 if success, -1 otherwise.
 */
static int open_wpa_supplicant_connection(void)
{
	/* control interface to send command to wpa_supplicant */
	ctrl_conn = wpa_ctrl_open(control_file_path);
	if (!ctrl_conn) {
		return -1;
	}
	/* ping wpa_supplicant every 5 seconds */
	event_loop_add_timer(PING_WPA_SUPPLICANT_INTERVAL_SECS, 0, send_periodic_ping_command, NULL);
	/* control interface to monitor messages and events */
	monitor_conn = wpa_ctrl_open(control_file_path);
	if (!monitor_conn) {
		close_wpa_supplicant_connection();
		return -1;
	}
	else {
		/* attach and add monitored control interface to event loop */
		if (wpa_ctrl_attach(monitor_conn) == 0) {
			event_loop_add_fd(wpa_ctrl_get_fd(monitor_conn), wpa_monitor_receive, NULL);
		} else {
			close_wpa_supplicant_connection();
			return -1;
		}
	}

	return 0;
}

/*
 * Try to connect to wpa_supplicant.
 * If failed, setup a timer to try to connect every 1 second.
 */
static void try_connect_wpa_supplicant(void *data)
{
	if (ctrl_conn)
		return;

	if (open_wpa_supplicant_connection()) {
		event_loop_add_timer(RETRY_CONNECT_INTERVAL_SECS, 0, try_connect_wpa_supplicant, NULL);
		return;
	}
	/* Connection is established here */
}

/*
 * show usage
 * @prog_name: program name
 */
static void show_usage (const char *prog_name)
{
	printmsg("Usage: %s <INTERFACE NAME> <INDEX>\n", prog_name);
	printmsg("Start a daemon to monitor WiFi client status on wireless network interface INTERFACE NAME\n");
	printmsg("INDEX: index in RDB structure (e.g index in wlan_sta.{INDEX}.extra_status for extra status)\n");
}

/*
 * main function
 * - Setup RDB variables
 * - Attach to wpa_supplicant
 * - Listen to wpa_supplicant, parse events and messages, and update RDB variables
 */
int main(int argc, char *argv[])
{
	int rval;

	if (argc != 3) {
		show_usage(argv[0]);
		return -EINVAL;
	}
	/* build control interface path */
	rval = snprintf(control_file_path, sizeof(control_file_path),"%s/%s", CTRL_IFACE_DIR, argv[1]);
	if (rval >= sizeof(control_file_path)){
		return -ENOMEM;
	}
	else if (rval < 0){
		return rval;
	}

	openlog("wifi_client_monitor", LOG_PID | LOG_PERROR, LOG_DEBUG);

	/* open rdb */
	if (init_rdb(argv[2])) {
		deinit_rdb();
		syslog(LOG_ERR, "Unable to open rdb");
		closelog();
		return 1;
	}

	/* initialise event loop */
	rval = event_loop_init();
	if (rval){
		syslog(LOG_ERR, "Failed to initialise event loop. Error: %s\n", strerror(rval));
		goto error;
	}
	/* try open connection to wpa_supplicant */
	if (open_wpa_supplicant_connection()) {
		syslog(LOG_WARNING, "Could not connect to wpa_supplicant: "
				"%s - re-trying\n", control_file_path);
		event_loop_add_timer(1, 0, try_connect_wpa_supplicant, NULL);
	}
	/* run event loop */
	event_loop_run();
	/* close wpa_supplicant connection */
	close_wpa_supplicant_connection();
	/* free event loop */
	event_loop_free();

	deinit_rdb();
	closelog();

	return 0;

	error:
	event_loop_free();
	deinit_rdb();
	closelog();

	return rval;
}

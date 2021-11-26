/*
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
 *
 * W1_monitor monitors 1-wire devices information and updates RDB variables.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include "daemon.h"
#include "rdb_ops.h"
#include "gem_api.h"
#include "w1_monitor.h"

w1_sl_list_type w1_sl_list[] =
{
	{ W1_FAMILY_DEFAULT,    "default",              "default family", NULL },
	{ W1_FAMILY_SMEM_01,    "iButton 1990A",        "Serial ID button (1990A)", NULL },
	{ W1_FAMILY_SMEM_81,    "iButton 1420",         "Serial ID button (1420)", NULL },
	{ W1_THERM_DS18S20,     "iButton 1990A",        "Parasite-Power Digital Thermometer (DS18S20)", process_w1_sl_ds18xxx },
	{ W1_FAMILY_DS28E04,    "EEPROM DS28E04",       "4096-Bit Addressable EEPROM with PIO (DS28E04-100)", NULL },
	{ W1_COUNTER_DS2423,    "Counter DS2423",       "Counter device (DS2423)", NULL },
	{ W1_THERM_DS1822,      "Thermometer DS1822",   "Econo Digital Thermometer (DS1822)", process_w1_sl_ds18xxx },
	{ W1_EEPROM_DS2433,     "EEPROM DS2433",        "4kb EEPROM family (DS2433)", NULL },
	{ W1_THERM_DS18B20,     "Thermometer DS18B20",  "Programmable Resolution Digital Thermometer (DS18B20)", process_w1_sl_ds18xxx },
	{ W1_FAMILY_DS2408,     "IO Expander DS2408",   "8-Channel Addressable Switch (IO Expander) family (DS2408)", NULL },
	{ W1_EEPROM_DS2431,     "EEPROM DS2431",        "1kb EEPROM family (DS2431)", NULL },
	{ W1_FAMILY_DS2760,     "Dallas 2760",          "Dallas 2760 battery monitor chip (HP iPAQ & others)", NULL },
	{ W1_FAMILY_DS2780,     "Dallas 2780",          "Dallas 2780 battery monitor chip", NULL },
	{ W1_THERM_DS1825,      "Thermometer DS1825",   "Programmable Resolution Digital Thermometer (DS1825)", process_w1_sl_ds18xxx },
	{ W1_FAMILY_DS2781,     "Dallas 2781",          "Dallas 2781 battery monitor chip", NULL },
	{ W1_THERM_DS28EA00,    "Thermometer DS28EA00", "Digital Thermometer with Sequence Detect and PIO (DS28EA00)", process_w1_sl_ds18xxx },
};

w1_dev_type w1_dev[MAX_SLAVE_DEVICES];

int running = 1;
int be_daemon = 1;

struct rdb_session* rdb;

static void print_buf(char* msg, int len)
{
	unsigned char* buf = (unsigned char *)msg;
	unsigned char buf2[256] = {0x0,};
	int i, j = len/16, k = len % 16;
	D("     < buffer contents : %d bytes >", len);
	for (i = 0; i < j; i++)	{
		D("     %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
	}
	if (k == 0)
		return;
	j = i;
	for (i = 0; i < k; i++)
		sprintf((char *)buf2, "%s%02x ", buf2, buf[j*16+i]);
	D("     %s", buf2);
}

static void print_str(char* buf, int len)
{
	char *msg = strdup(buf);
	char *token;
	D("     < buffer contents : %d bytes >", len);
	for(token = strtok(msg, "\n"); token; token = strtok(NULL, "\n"))
		D("     %s", token);
	free(msg);
}

static void print_devInfo(int idx)
{
	D("     < device info : index %d >", idx);
	D("     name       : %s", w1_dev[idx].name);
	D("     sName      : %s", w1_dev[idx].sName);
	D("     id         : %s", w1_dev[idx].id);
	D("     path       : %s", w1_dev[idx].path);
	D("     data       : %s", w1_dev[idx].data);
	D("     flag       : 0x%02x", w1_dev[idx].flag);
}

/* If 1-wire device disappears then obsolute RDB variables are left so
 * it is necessary to remove these RDB variables.
 */
static void cleanup_w1_rdb_variables(int idx)
{
	char rdb_name[MAX_NAME_LEN];
	char rdb_root[MAX_NAME_LEN];
	char buf[MAX_DATA_LEN];
	int i, len = MAX_DATA_LEN;
	int rdb_fd = rdb_open_db();
	D("     %s(%d)...", __func__, idx);
	if (rdb_fd < 0) {
		syslog(LOG_ERR, "Could not open RDB");
		return;
	}
	for (i = idx; i < MAX_SLAVE_DEVICES; i++) {
		sprintf(rdb_name, "sensors.w1.%d.name", i);
		if (rdb_get(rdb, rdb_name, buf, &len) >= 0) {
			/* delete RDB root */
			sprintf(rdb_root, "sensors.w1.%d", i);
			D("     deleting rdb root %s", rdb_root);
			gem_delete_rdb_root(rdb_root, TRUE, FALSE);
			memset((char *)&w1_dev[idx], 0x00, sizeof(w1_dev_type));
		} else {
			break;
		}
	}
	rdb_close_db();
}

/* 1-wire RDB variable index starts from 0 so sensors.w1.last_index
 * variables indicates next 1-wire index value.
 * ex) If there are 2 devices then sensors.w1.last_index = 2.
 */
static void update_w1_rdb_last_index(int idx)
{
	static int last_index = -1;
	char *rdb_name = "sensors.w1.last_index";
	if (idx != last_index) {
		char val[3];
		sprintf(val, "%d", idx);
		(void) rdb_update_string(rdb, rdb_name, val, CREATE, 0);
		D("     rdb_update(%s, %s)", rdb_name, val);
		last_index = idx;
	}
}

/* Update RDB variables only when the value is changed */
static void update_w1_rdb_variable(int idx, int bit, char *var, char *value)
{
	char rdb_name[MAX_NAME_LEN];
	if (w1_dev[idx].flag & bit) {
		sprintf(rdb_name, "sensors.w1.%d.%s", idx, var);
		(void) rdb_update_string(rdb, rdb_name, value, CREATE, 0);
		D("     rdb_update(%s, %s)", rdb_name, value);
	}
}

static void update_w1_dev_info(int idx)
{
	print_devInfo(idx);
	/* update chaned RDB variables only */
	D("     < update RDB variables : index %d >", idx);
	update_w1_rdb_variable(idx, NAME_BIT, "name", w1_dev[idx].name);
	update_w1_rdb_variable(idx, SNAME_BIT, "sname", w1_dev[idx].sName);
	update_w1_rdb_variable(idx, DID_BIT, "id", w1_dev[idx].id);
	update_w1_rdb_variable(idx, PATH_BIT, "path", w1_dev[idx].path);
	update_w1_rdb_variable(idx, DATA_BIT, "data", w1_dev[idx].data);
}


/* Maxim DS18XXX digital thermometer
 *
 * Contents of /sys/bus/w1/devices/[w1 id]/w1_slave
 *
 *   81 01 4b 46 7f ff 0f 10 71 : crc=71 YES
 *   81 01 4b 46 7f ff 0f 10 71 t=24062
 */
int process_w1_sl_ds18xxx(int dbIdx, int idx)
{
	int fd, err, len, numLen = 0, ret = 0;
	float tempC;
	char path[MAX_NAME_LEN+8];
	char buf[MAX_DATA_LEN];
	char *val;

	D("%s(%d, %d)...", __func__, dbIdx, idx);

	/* assemble path to w1 device data file */
	sprintf(path, "%s/w1_slave", w1_dev[dbIdx].path);

	fd = open(path, O_RDONLY);
	if(fd == -1) {
		syslog(LOG_ERR, "Couldn't open the w1 device : %s", w1_dev[dbIdx].id);
		ret = 0;
		goto ret_exit;
	}
	while(running) {
		  len = read(fd, buf+numLen, MAX_DATA_LEN - numLen);
		  if (len <= 0)
			  break;
		  else
			  numLen += len;
		  if (numLen > MAX_DATA_LEN) {
			  numLen = MAX_DATA_LEN;
			  break;
		  }
	}
	buf[numLen] = 0;
	print_str(buf, numLen);

	// CRC check
	val = strstr(buf, "crc=");
	if (!val) {
		D("    - parsing error, keyword 'crc=' not found.");
		goto ret_exit;
	}
	if (strlen(val) >= 7) {
		val += 7;
	} else {
		D("    - invalid length");
		goto ret_exit;
	}
	if (strncmp(val, "NO", 2) == 0) {
		D("    - CRC error");
		goto ret_exit;
	} else if (strncmp(val, "YES", 3) != 0) {
		D("    - unknown CRC value");
		goto ret_exit;
	}

	val = strstr(buf, "t=");
	if (!val) {
		D("    - parsing error, keyword 't=' not found.");
		goto ret_exit;
	}
	if (strlen(val) >= 2) {
		val += 2;
	} else {
		D("    - invalid length");
		goto ret_exit;
	}
	errno = 0;
	tempC = strtof(val, NULL);
	err = errno;
	if (err) {
		D("    - error(%s)", strerror(err));
	} else {
		sprintf(buf, "%.3f \u2103, %.3f \u2109", tempC / 1000, (tempC / 1000) * 9 / 5 + 32);
		UPDATE_FIELD(w1_dev[dbIdx].data, buf, w1_dev[dbIdx].flag, DATA_BIT);
	}
ret_exit:
	close(fd);
	return ret;
}

static int find_w1_sl_by_dev_name(const char *dev_name)
{
	int i;
	for (i = 0;i < (sizeof(w1_sl_list)/sizeof(w1_sl_list[0]));i++) {
		/* compare first 2 bytes (FID) */
		if (strncmp(w1_sl_list[i].fid, dev_name, 2) == 0)
			return i;
	}
	return -1;
}

/* Polling all 1-wire devices at /sys/bus/w1 folder and
 * updating device information and RDB variables when their
 * values are changed.
 */
static void polling_w1_devices(void)
{
	int idx, dbIdx = 0;
	DIR *dir;
	struct dirent *dirent;
	char path[MAX_NAME_LEN];	// Path to device

	dir = opendir (W1_SYSDEV_PATH);
	if (dir != NULL) {
		while ((dirent = readdir (dir)) && running) {
			/* find device index in slave device list */
			idx = find_w1_sl_by_dev_name(dirent->d_name);
			if (dirent->d_type == DT_LNK && idx >= 0)
			{
				D("==========================================================================================");
				D("[%s] 1-wire %s", dirent->d_name, w1_sl_list[idx].name);
				sprintf(path, "%s/%s", W1_SYSDEV_PATH, dirent->d_name);

				/* update w1 device info db */
				w1_dev[dbIdx].flag = 0;
				UPDATE_FIELD(w1_dev[dbIdx].name, w1_sl_list[idx].name, w1_dev[dbIdx].flag, NAME_BIT);
				UPDATE_FIELD(w1_dev[dbIdx].sName, w1_sl_list[idx].sname, w1_dev[dbIdx].flag, SNAME_BIT);
				UPDATE_FIELD(w1_dev[dbIdx].id, dirent->d_name, w1_dev[dbIdx].flag, DID_BIT);
				UPDATE_FIELD(w1_dev[dbIdx].path, path, w1_dev[dbIdx].flag, PATH_BIT);

				/* if device name changed, that means old device is disconnected or
				 * new device is connected so refresh all fields together.
				 */
				if (w1_dev[dbIdx].flag & NAME_BIT)
					w1_dev[dbIdx].flag = MASKALL;

				if (w1_sl_list[idx].p_sl_process) {
					if (w1_sl_list[idx].p_sl_process(dbIdx, idx) >= 0) {
						update_w1_dev_info(dbIdx);
						dbIdx++;
						if (dbIdx >= MAX_SLAVE_DEVICES)
							break;
					}
				} else {
					syslog(LOG_ERR, "No processing function is implemented for this device");
					continue;
				}
			}
		}
		(void) closedir(dir);
		update_w1_rdb_last_index(dbIdx);
		cleanup_w1_rdb_variables(dbIdx);
		D("==========================================================================================");
	}
	else
	{
		syslog(LOG_ERR, "Couldn't open the w1 devices directory");
	}
}

/* Obsolute RDB function, rdb_open_db() and  rdb_close_db are used
 * in this function to call gem_delete_rdb_root() which is written
 * based on these two obsolute functions.
 */
static int rdb_init(void)
{
	int rdb_fd = rdb_open_db();

	if (rdb_fd < 0) {
	    syslog(LOG_ERR, "Could not open RDB");
	    return -1;
	}

	/* delete RDB root */
	syslog(LOG_INFO, "deleting rdb root %s", W1_RBD_ROOT);
	gem_delete_rdb_root(W1_RBD_ROOT, TRUE, FALSE);
	rdb_close_db();

	/* open rdb database again */
	if( rdb_open(NULL,&rdb)<0 ) {
		syslog(LOG_ERR,"failed to open rdb driver - %s",strerror(errno));
		return -1;
	}

	return 0;
}

static void rdb_fini(void)
{
	if(rdb)
		rdb_close(&rdb);
}

static void release_singleton(const char *lockfile)
{
	daemon_fini();
}

static void release_resources(const char *lockfile)
{
	release_singleton(lockfile);
}

static void sig_handler(int signum)
{
	pid_t	pid;
	int stat;

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

static void fini(const char *lockfile)
{
	rdb_fini();
	closelog();
	release_resources(lockfile);
	syslog(LOG_INFO, "terminated");
}

static int init(int be_daemon)
{
	if (be_daemon) {
		daemon_init(APPLICATION_NAME, NULL, 0, LOG_INFO);
		syslog(LOG_INFO, "daemonized");
	} else {
		openlog(APPLICATION_NAME ,LOG_PID | LOG_PERROR, LOG_LOCAL5);
		setlogmask(LOG_UPTO(LOG_INFO));
	}

	syslog(LOG_INFO, "start...");

	/* signal handler set */
	signal(SIGHUP, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGCHLD, sig_handler);

	if(rdb_init()<0) {
		syslog(LOG_ERR,"rdb_init() failed");
		goto err;
	}

	syslog(LOG_INFO, "initialized");
	return 0;
err:
	if (be_daemon) {
		release_resources(LOCK_FILE);
	}
	return -1;
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: "APPLICATION_NAME" [-v] [-d]\n"

		"\n"
		"Options:\n"

		"\t-v Display version information\n"
		"\t-d don't detach from controlling terminal (don't daemonise)\n"
		"\n"
		"The default behavior of %s is browsing 1-wire devices and read data.\n"
		"\n"
		, APPLICATION_NAME
	);
}

static void parse_options(int argc, char *argv[])
{
	int ret;
	/* Parse Options */
	while ((ret = getopt(argc, argv, "hvd")) != EOF) {
		switch (ret) {
			case 'v':
				fprintf(stderr, "%s: build date / %s %s\n",APPLICATION_NAME, __TIME__,__DATE__);
				exit(0);
			case 'd':
				be_daemon = 0;
				break;
			case 'h':
			case '?':
				usage();
				exit(0);
		}
	}
}

int main(int argc, char *argv[])
{
	parse_options(argc, argv);

	if (init(be_daemon) < 0) {
		goto exit_ret;
	}

	/* select loop */
	while(running) {
		/* Polling time is 2 seconds which is fast enough for 1-wire devices scanning.
		 * 1-wire devices are mostly slow devices at so even 5 seconds polling time could be better.
		 * Most realistic cases 1-wire devices are connected before power-up and
		 * the device data changes slowly.
		 */
		struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
		int retval;

		retval = select( 1, NULL, NULL, NULL, &timeout );

		/* call again if interrupted by signal, or exit if error condition */
		if ((retval < 0) && (errno != EINTR)) {
			syslog(LOG_ERR,"select() failed with %d - %s", retval, strerror(errno));
			break;
		}
		/* polling 1-wire devices */
		else if (retval == 0) {
			polling_w1_devices();
		}
	}

exit_ret:
	fini(LOCK_FILE);
	return 1;
}

/*
 * watchdog_logger.c
 *    implements the logger that provides capabilities for application to write
 *    log messages to STDIO, syslogd, klogd and a text file
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

#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct watchdog_logger {
	int initialised;
	int channels;
	char name[32];
	char filename[256];
	int priority;
};

static struct watchdog_logger logger;

void logger_init(const char *name, int channels, int priority, const char *filename)
{
	if (name) {
		strncpy(logger.name, name, sizeof(logger.name));
	} else {
		strcpy(logger.name, "logger");
	}
	logger.priority = priority;
	logger.channels = channels;

	if (logger.channels & WATCHDOG_LOG_FILE) {
		if (filename) {
			strncpy(logger.filename, filename, sizeof(logger.filename));
		} else {
			logger.channels &= ~WATCHDOG_LOG_FILE;
		}
	}

	logger.initialised = 1;
}

static void logger_file(int priority, const char *fmt, va_list ap)
{
	FILE *file;
	time_t now;
	char *timestamp;

	if ((file = fopen(logger.filename, "a")) == NULL) {
		return;
	}

	now = time(NULL);
	timestamp = ctime(&now) + 4; /* skip day of week */
	timestamp[15] = '\0'; /* remove new line */

	fprintf(file, "%s %s ", timestamp, logger.name);
	vfprintf(file, fmt, ap);
	fclose(file);
}

static void logger_syslog(int priority, const char *fmt, va_list ap)
{
	openlog(logger.name, 0, LOG_DAEMON);
	vsyslog(priority, fmt, ap);
	closelog();
}

static void logger_kmsg(int priority, const char *fmt, va_list ap)
{
	FILE *file;

	if ((file = fopen("/dev/kmsg", "r+")) == NULL) {
		return;
	}

	fprintf(file, "%s: ", logger.name);
	vfprintf(file, fmt, ap);
	fclose(file);
}

static void logger_stdio(int priority, const char *fmt, va_list ap)
{
	time_t now;
	char *timestamp;

	now = time(NULL);
	timestamp = ctime(&now) + 4; /* skip day of week */
	timestamp[15] = '\0'; /* remove new line */

	fprintf(stderr, "%s %s ", timestamp, logger.name);
	vfprintf(stderr, fmt, ap);
}

void logger_log(int priority, const char *fmt, ...)
{
	va_list ap;

	if (!logger.initialised) {
		return;
	}

	if (priority > logger.priority) {
		return;
	}

	if (logger.channels & WATCHDOG_LOG_FILE) {
		va_start(ap, fmt);
		logger_file(priority, fmt, ap);
		va_end(ap);
	}

	if (logger.channels & WATCHDOG_LOG_SYSLOG) {
		va_start(ap, fmt);
		logger_syslog(priority, fmt, ap);
		va_end(ap);
	}

	if (logger.channels & WATCHDOG_LOG_KMSG) {
		va_start(ap, fmt);
		logger_kmsg(priority, fmt, ap);
		va_end(ap);
	}

	if (logger.channels & WATCHDOG_LOG_STDIO) {
		va_start(ap, fmt);
		logger_stdio(priority, fmt, ap);
		va_end(ap);
	}
}

void logger_term(void)
{
	if (!logger.initialised) {
		return;
	}

	logger.initialised = 0;
}


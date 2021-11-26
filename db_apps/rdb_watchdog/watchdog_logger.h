/*
 * watchdog_logger.h
 *    provides public interfaces for the watchdog logger
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
#ifndef _WATCHDOG_LOGGER_H_
#define _WATCHDOG_LOGGER_H_

enum {
	WATCHDOG_LOG_STDIO = (1 << 0),
	WATCHDOG_LOG_FILE = (1 << 1),
	WATCHDOG_LOG_SYSLOG = (1 << 2),
	WATCHDOG_LOG_KMSG = (1 << 3)
};

#define WATCHDOG_LOG_EMER_PRIO 0
#define WATCHDOG_LOG_ALERT_PRIO 1
#define WATCHDOG_LOG_CRIT_PRIO 2
#define WATCHDOG_LOG_ERR_PRIO 3
#define WATCHDOG_LOG_WARNING_PRIO 4
#define WATCHDOG_LOG_NOTICE_PRIO 5
#define WATCHDOG_LOG_INFO_PRIO 6
#define WATCHDOG_LOG_DEBUG_PRIO 7

/*
 * initialises the logger
 * \param name the owner's name of the logger
 * \param channels logical logging channels to send log messages
 * \param priority the priority of the logger
 * \param filename the name of the text file to save log messages,
 *        the filename is ignored if WATCHDOG_LOG_FILE channel is off
 */
void logger_init(const char *name, int channels, int priority, const char *filename);

/*
 * terminates the logger
 */
void logger_term(void);

/*
 * logs the message
 * \param priority the priority of the message
 * \param fmt the message itself or message format
 * \param ... variable arguements to add more details on the message
 */
void logger_log(int priority, const char *fmt, ...);

#define WDT_LOG_EMER(...) logger_log(WATCHDOG_LOG_EMER_PRIO, __VA_ARGS__)
#define WDT_LOG_ALERT(...) logger_log(WATCHDOG_LOG_ALERT_PRIO, __VA_ARGS__)
#define WDT_LOG_CRIT(...) logger_log(WATCHDOG_LOG_CRIT_PRIO, __VA_ARGS__)
#define WDT_LOG_ERR(...) logger_log(WATCHDOG_LOG_ERR_PRIO, __VA_ARGS__)
#define WDT_LOG_WARN(...) logger_log(WATCHDOG_LOG_WARNING_PRIO, __VA_ARGS__)
#define WDT_LOG_NOTICE(...) logger_log(WATCHDOG_LOG_NOTICE_PRIO, __VA_ARGS__)
#define WDT_LOG_INFO(...) logger_log(WATCHDOG_LOG_INFO_PRIO, __VA_ARGS__)
#define WDT_LOG_DEBUG(...) logger_log(WATCHDOG_LOG_DEBUG_PRIO, __VA_ARGS__)

#endif /* _WATCHDOG_LOGGER_H_ */

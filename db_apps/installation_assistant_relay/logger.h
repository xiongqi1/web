#ifndef LOGGER_H_14580311122015
#define LOGGER_H_14580311122015
/**
 * @file logger.h
 * @brief Provides public interfaces for the logger
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

enum {
	LOGGER_STDIO = (1 << 0),
	LOGGER_FILE = (1 << 1),
	LOGGER_SYSLOG = (1 << 2),
	LOGGER_KMSG = (1 << 3)
};

#define LOG_EMER_PRIO 0
#define LOG_ALERT_PRIO 1
#define LOG_CRIT_PRIO 2
#define LOG_ERR_PRIO 3
#define LOG_WARNING_PRIO 4
#define LOG_NOTICE_PRIO 5
#define LOG_INFO_PRIO 6
#define LOG_DEBUG_PRIO 7

/*
 * initialises the logger
 * \param name the owner's name of the logger
 * \param channels logical logging channels to send log messages
 * \param priority the priority of the logger
 * \param filename the name of the text file to save log messages,
 *        the filename is ignored if LOG_FILE channel is off
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

#define LOG_EMER(...) logger_log(LOG_EMER_PRIO, __VA_ARGS__)
#define LOG_ALERT(...) logger_log(LOG_ALERT_PRIO, __VA_ARGS__)
#define LOG_CRIT(...) logger_log(LOG_CRIT_PRIO, __VA_ARGS__)
#define LOG_ERR(...) logger_log(LOG_ERR_PRIO, __VA_ARGS__)
#define LOG_WARN(...) logger_log(LOG_WARNING_PRIO, __VA_ARGS__)
#define LOG_NOTICE(...) logger_log(LOG_NOTICE_PRIO, __VA_ARGS__)
#define LOG_INFO(...) logger_log(LOG_INFO_PRIO, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(LOG_DEBUG_PRIO, __VA_ARGS__)
#define LOG_MUTE(...)

#define LOG_EMER_AT() logger_log(LOG_EMER_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_ALERT_AT() logger_log(LOG_ALERT_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_CRIT_AT() logger_log(LOG_CRIT_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_ERR_AT() logger_log(LOG_ERR_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_WARN_AT() logger_log(LOG_WARNING_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_NOTICE_AT() logger_log(LOG_NOTICE_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_INFO_AT() logger_log(LOG_INFO_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_DEBUG_AT() logger_log(LOG_DEBUG_PRIO, "%s, %d\n", __func__, __LINE__)
#define LOG_MUTE_AT()

#if defined(NO_CHECK_RETURN)
	#define CHK_RET(func,ret) func
#else
	#define CHK_RET(func,ret) { \
		if ((ret != func)) { \
			LOG_WARN("Check Return %s, %d\r\n", __FILE__, __LINE__); \
		} \
	}
#endif

#endif

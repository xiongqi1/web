#ifndef LOGGER_H_10405501082018
#define LOGGER_H_10405501082018
/**
 * @file logger.h
 *
 * Header file for logging functionality.
 *
 *//*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdlib.h>
#include <syslog.h>

/*
 * Macros
 */
#define logError(...) logger(LOG_ERR, __VA_ARGS__)
#define logErr(...) logger(LOG_ERR, __VA_ARGS__)
#define logWarning(...) logger(LOG_WARNING, __VA_ARGS__)
#define logWarn(...) logger(LOG_WARNING, __VA_ARGS__)
#define logNotice(...) logger(LOG_NOTICE, __VA_ARGS__)
#define logInfo(...) logger(LOG_INFO, __VA_ARGS__)
#ifndef NDEBUG
#define logDebug(...) logger(LOG_DEBUG, __VA_ARGS__)
#define loggerHasDebug true
#else
#define logDebug(...)
#define loggerHasDebug false
#endif

/**
 * Select logging output mode
 */
typedef enum {
    LOG_OUTPUT_SYSLOG,              ///< Log to syslog only
    LOG_OUTPUT_STDOUT,              ///< Log to stdout only
    LOG_OUTPUT_BOTH                 ///< Log to both syslog and stdout
} logOutput_e;

/**
 * Allows configuration of output for logger calls
 *
 * @param output Output mode for logging
 * @param ident Identity string under which the log messages will be logged (syslog modes only)
 * @param option Options for output as per openlog command (syslog modes only).
 * @param facility Default facility for log messages (syslog modes only).
 */
void loggerConfig(logOutput_e output, const char *ident = NULL, int option = 0, int facility = LOG_USER);

/**
 * Logs messages either to syslog or to stdout
 *
 * @param priority Combined priority/facility value for the message
 * @param format Message or format for following variable arguments
 * @param ... Additional arguments
 */
void logger(int priority, const char *format, ...);

#endif  // LOGGER_H_10405501082018

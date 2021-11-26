/**
 * @file logger.c
 *
 * Implementation file for logging functionality.
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
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

/*
 * Statics
 */
static logOutput_e s_output = LOG_OUTPUT_SYSLOG;

/**
 * Get logging priority (level/facility) as a string
 *
 * @param priority Logging priority value.
 * @return Constant string describing the logging level, or "UNKOWN".
 */
static const char *getPriorityString(int priority)
{
    int level = priority & LOG_PRIMASK;
    if (level == LOG_ERR) return "ERROR";
    else if (level == LOG_WARNING) return "WARN";
    else if (level == LOG_NOTICE) return "NOTICE";
    else if (level == LOG_INFO) return "INFO";
    else if (level == LOG_DEBUG) return "DEBUG";
    return "UNKNOWN";
}

/*
 * loggerConfig
 */
void loggerConfig(logOutput_e output, const char *ident, int option, int facility)
{
    if (output == LOG_OUTPUT_STDOUT) {
        closelog();
        s_output = output;
    }
    else if ((output == LOG_OUTPUT_SYSLOG) || (output == LOG_OUTPUT_BOTH)) {
        closelog();
        openlog(ident, option, facility);
        s_output = output;
    }
    else {
        logError("loggerConfig unknown output mode %d", output);
    }
}


/*
 * logger
 */
void logger(int priority, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    if (s_output != LOG_OUTPUT_SYSLOG) {
        printf("%s: ", getPriorityString(priority));
        vprintf(format, vargs);
        printf("\n");
    }
    if (s_output != LOG_OUTPUT_STDOUT) {
        vsyslog(priority, format, vargs);
    }
    va_end(vargs);
}


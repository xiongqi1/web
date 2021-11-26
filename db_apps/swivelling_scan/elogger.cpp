/*
 * Copied from enhanced syslog
 *
 * Copyright Notice:
 * Copyright (C) 2021 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "elogger.hpp"

#include <stdarg.h>

#include <exception>
#include <string>

#include "estd.hpp"

namespace swscan
{

void Logger::syslog(int logLevel, const char *file, int line, const char *strFmt, ...) const noexcept
{
    va_list ap;

    std::string msg;

    va_start(ap, strFmt);

    try {
        msg = estd::vformat(strFmt, ap);

        for (const auto &lmsg : estd::split(msg)) {
            ::syslog(logLevel, estd::format("%s:%d: %s", file, line, lmsg.c_str()).c_str());
        }
    }
    catch (const std::bad_alloc &e) {
        ::syslog(LOG_ERR, "memory allocation failure for syslog");
    }

    ::va_end(ap);
}

void Logger::setup(int maxLogLevel_)
{
    syslog(LOG_NOTICE, __FILE__, __LINE__, "change log level from %d to %d", maxLogLevel, maxLogLevel_);

    maxLogLevel = maxLogLevel_;

    ::setlogmask(LOG_UPTO(maxLogLevel));

    for (int i = LOG_EMERG; i <= LOG_DEBUG; ++i)
        syslog(i, __FILE__, __LINE__, "loglevel checkpoint (level=%d)", i);
}

Logger::Logger() : name(program_invocation_short_name), maxLogLevel(LOG_DEBUG)
{
    ::openlog(name.c_str(), LOG_PID, LOG_DAEMON);
    ::setlogmask(LOG_UPTO(maxLogLevel));

    syslog(LOG_NOTICE, __FILE__, __LINE__, "start...");
}

Logger::~Logger()
{
    syslog(LOG_NOTICE, __FILE__, __LINE__, "finished.");
    ::closelog();
}
} // namespace swscan

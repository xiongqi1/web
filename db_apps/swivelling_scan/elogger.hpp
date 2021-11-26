#ifndef __SWSCAN_ELOGGER_HPP__
#define __SWSCAN_ELOGGER_HPP__

/*
 * enhanced syslog
 * Copied from estd logger, of which macro "log" conflicts with some std headers.
 * Renaming that macro "log" here.
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

#include <syslog.h>

#include "estd.hpp"

#define swscan_log(logLevel, ...) swscan::Logger::getInstance().syslog(logLevel, __FILE__, __LINE__, __VA_ARGS__)

namespace swscan
{

/**
 * @brief syslog class
 *
 */
class Logger : public estd::Singleton<Logger>
{
  public:
    Logger();
    ~Logger();

    void setup(int maxLogLevel_);
    void syslog(int logLevel, const char *file, int line, const char *strFmt, ...) const noexcept __attribute__((format(printf, 5, 6)));

  private:
    std::string name;
    int maxLogLevel;
};
}

#endif

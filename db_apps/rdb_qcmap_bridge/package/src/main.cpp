/*
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
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

#include <execinfo.h>
#include <signal.h>
#include <syslog.h>

#include <exception>

#include "esignal.hpp"
#include "estring.hpp"
#include "rdbqcmapbridge.hpp"
#include <elogger.hpp>
#include <estd.hpp>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // change log level
    estd::Logger::getInstance().setup(LOG_DEBUG);

    log(LOG_INFO, "initiate global signal");
    estd::Signal &signal = estd::Signal::getInstance();

    log(LOG_INFO, "set SIGSEGV action");
    signal.setSigAction(SIGSEGV, 0, [](int sigNo, siginfo_t *sigInfo, void *uContext) {
        estd::CCallStack callStack(0);

        void *callerAddr;
        const ucontext_t &uc = *static_cast<ucontext_t *>(uContext);

        callerAddr = (void *)uc.uc_mcontext.arm_pc;

        std::string logMsg;

        if (sigNo == SIGSEGV) {
            logMsg = estd::format("[signal] %s, address is %p from %p\n", ::strsignal(sigNo), sigInfo->si_addr, (void *)callerAddr);
        } else {
            logMsg = estd::format("[signal] %s\n", ::strsignal(sigNo));
        }

        logMsg += callStack.to_string();

        estd::fputs_newline(logMsg, stderr);
        log(LOG_ERR, logMsg.c_str());

        std::abort();
    });

    log(LOG_INFO, "set assert handler");
    signal.setAssertHandler([](const char *progname, const char *assertion, const char *file, unsigned int line, const char *function) {
        estd::CCallStack callStack(4);

        std::string logMsg = estd::format("[assert] %s: %s:%d: %s %s\n", program_invocation_short_name, file, line, function, assertion);

        logMsg += callStack.to_string();

        estd::fputs_newline(logMsg, stderr);
        log(LOG_CRIT, logMsg.c_str());
    });

    // scope for rdb session
    try {
        log(LOG_INFO, "start RdbQcMapBridge");
        auto &bridge = eqmi::RdbQcMapBridge::getInstance();

        auto termHandle = [&bridge](int sigNo, siginfo_t *sigInfo, void *uContext) mutable {
            log(LOG_DEBUG, "terminate rdb qc map (signal=%s)\n", strsignal(sigNo));
            bridge.terminate();
        };

        log(LOG_INFO, "set signal actions");
        signal.setSigAction(SIGTERM, 0, termHandle);
        signal.setSigAction(SIGHUP, 0, termHandle);

        log(LOG_INFO, "run RdbQcMapBridge");
        bridge.run();
    }
    catch (std::exception &e) {
        std::string what = e.what();

        estd::fputs_newline(what, stderr);
        log(LOG_ERR, what.c_str());
    }
    catch (...) {
        log(LOG_CRIT, "unknown exception detected.");
    }
}

/*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include "sysrepo_client.h"
#include "timer.h"
#include "Sysrepo.h"

#include <cdcs_fileselector.h>
#include <cdcs_rdb.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <signal.h>

using namespace std;

RdbSession rdbSession;
static volatile bool running = true;

void sigHandler(int signo)
{
    DBG(LOG_DEBUG,"signal %d received",signo);
    running=false;
}

// Periodic timer
void TimerFd::onTimeOut()
{
    DBG(LOG_DEBUG,"Periodic timer");
}

int main(int argc, char *argv[])
{
    DBG(LOG_INFO,"started v 0.92");
    FileSelector fileSelector(1000);
    Sysrepo sysrepo;
    //TimerFd timer(1000,fileSelector);
    /* start rdb */
    if(!rdbSession.init() ) {
        DBG(LOG_ERR,"failed to init rdb");
        return -1;
    }

    rdbSession.registerWith(fileSelector);

    if(!sysrepo.init(fileSelector)) {
        DBG(LOG_ERR,"failed to init sysrepo");
        return -1;
    }

    /* override signals */
    signal(SIGINT,sigHandler);
    signal(SIGTERM,sigHandler);
    signal(SIGPIPE,sigHandler);
    signal(SIGUSR2,sigHandler);

    sysrepo.provideData();

    RdbVar rdbDaemonReady(rdbSession, "sys.sysrepo.client.ready");
    rdbDaemonReady.setVal("1");
    while(running) {
        fileSelector.processReadyFiles();
    }
    rdbDaemonReady.setVal("0");

    rdbSession.unregister();

    DBG(LOG_INFO,"stopped");
    return 0;
}

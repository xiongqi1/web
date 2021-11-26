/*
 * User interface implementation
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

#include <cstring>
#include <cstdio>
#include <exception>
#include <functional>

#include <jsoncons/json.hpp>
#include <erdbsession.hpp>

#include "user_interface.hpp"
#include "elogger.hpp"

namespace swscan
{
    UserInterface::UserInterface(asio::io_context &io, ManagerCommand &mng)
        :
        manager(mng),
        rpcServer("SwivellingScan"),
        stRdb(io, erdb::Session::getInstance().getHandle())
    {
        rpcServer.addCommand("startSwivellingScan", {"param"}, *this);
        rpcServer.addCommand("stopSwivellingScan", {}, *this);
        rpcServer.addCommand("moveToHomePosition", {}, *this);

        rpcServer.serverRun(erdb::Session::getInstance().getSession());

        stRdb.async_wait(asio::posix::descriptor_base::wait_type::wait_read,
            std::bind(&UserInterface::rdbNotified, this, std::placeholders::_1));
    }

    void UserInterface::rdbNotified(const asio::error_code& e)
    {
        if (e) {
            if (e != asio::error::operation_aborted) {
                swscan_log(LOG_ERR, "rdbNotified: handle error: %s", e.message().c_str());
            }
            return;
        }
        stRdb.async_wait(asio::posix::descriptor_base::wait_type::wait_read,
            std::bind(&UserInterface::rdbNotified, this, std::placeholders::_1));
        auto rdbNames = erdb::Session::getInstance().getnames_triggered("");
        swscan_log(LOG_NOTICE, "rdb notified: %s", rdbNames.c_str());
        rpcServer.processCommands(rdbNames);
    }

    int UserInterface::writeCommandResult(CommandResult &cmdResult, char *result, int *resultLen)
    {
        int rval = 0;
        std::ostringstream ss;
        jsoncons::encode_json(cmdResult, ss);
        int written = snprintf(result, *resultLen, "%s", ss.str().c_str());
        if (written < 0 || *resultLen <= written) {
            rval = -1;
        } else {
            *resultLen = written + 1;
        }
        return rval;
    }

    int UserInterface::handle(const std::string cmdStr,
        rdb::rdb_rpc_cmd_param_t *params, int paramsLen,
        char *result, int *resultLen)
    {
        int rval = 0;
        if (cmdStr == "startSwivellingScan") {
            if (paramsLen == 1 && strcmp(params[0].name, "param") == 0) {
                try {
                    std::string paramStr(params[0].value);
                    auto param = jsoncons::decode_json<StartSwivellingScanParam>(paramStr);
                    CommandResult cmdResult = manager.startSwivellingScan(param);
                    rval = writeCommandResult(cmdResult, result, resultLen);
                } catch (std::exception &e) {
                    rval = -1;
                }
            }
        } else if (cmdStr == "stopSwivellingScan") {
            CommandResult cmdResult = manager.stopSwivellingScan();
            rval = writeCommandResult(cmdResult, result, resultLen);
        } else if (cmdStr == "moveToHomePosition") {
            CommandResult cmdResult = manager.moveToHomePosition();
            rval = writeCommandResult(cmdResult, result, resultLen);
        } else {
            rval = -1;
        }
        return rval;
    }
}

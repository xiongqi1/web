/*
 * Manager command interface
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
#ifndef _SWSCAN_MANAGER_COMMAND_
#define _SWSCAN_MANAGER_COMMAND_

#include <string>

#include <jsoncons/json.hpp>
#include <jsoncons/json_type_traits.hpp>

namespace swscan
{
    struct CommandResult
    {
        bool success;
        std::string error;
        CommandResult(bool succ, const std::string &err): success(succ), error(err) { }
    };

    struct StartSwivellingScanParam
    {
        bool force = true;
    };

    class ManagerCommand
    {
        public:
            virtual ~ManagerCommand() = default;
            virtual CommandResult startSwivellingScan(StartSwivellingScanParam &param) = 0;
            virtual CommandResult stopSwivellingScan() = 0;
            virtual CommandResult moveToHomePosition() = 0;
    };
}

JSONCONS_ALL_MEMBER_TRAITS(swscan::CommandResult, success, error)
JSONCONS_ALL_MEMBER_TRAITS(swscan::StartSwivellingScanParam, force)

#endif

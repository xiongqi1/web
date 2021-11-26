/*
 * Status
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
#ifndef _SWSCAN_STATUS_
#define _SWSCAN_STATUS_

#include <string>
#include <map>

#include <jsoncons/json_type_traits.hpp>
#include <erdb.hpp>

namespace swscan
{
    class Status
    {
        public:
            enum class Mode
            {
                Initialising,
                CheckingMotor,
                SwivellingScan,
                SwivellingScanMoving,
                SwivellingScanAtPosition,
                MovingToBestPosition,
                MovingDueToFailedScan,
                SuccessFinal,
                Failed,
                None,
                Busy,
                Stopped
            };

            const std::string& getStatusStr() const { return modeToStr[status]; }
            Mode getStatus() const { return status; }
            const std::string& getErrorStr() const { return errorStr; }
            int getCurrentPositionIndex() const { return currentPositionIndex; }
            int getMovingToPositionIndex() const { return movingToPositionIndex; }
            int getCurrentSwivellingScanStep() const { return currentSwivellingScanStep; }
            int getMaxSwivellingScanStep() const { return maxSwivellingScanStep; }

            void setStatus(Mode st);
            void setErrorStr(const std::string &err) { errorStr = err; }
            void setCurrentPositionIndex(int idx) { currentPositionIndex = idx; }
            void setMovingToPositionIndex(int idx) { movingToPositionIndex = idx; }
            void setCurrentSwivellingScanStep(int idx) { currentSwivellingScanStep = idx; }
            void setMaxSwivellingScanStep(int idx) { maxSwivellingScanStep = idx; }

            void report();
        private:
            static inline std::map<Mode, const std::string> modeToStr{
                {Mode::Initialising, "initialising"},
                {Mode::CheckingMotor, "checkingMotor"},
                {Mode::SwivellingScan, "swivellingScan"},
                {Mode::SwivellingScanMoving, "swivellingScanMoving"},
                {Mode::SwivellingScanAtPosition, "swivellingScanAtPosition"},
                {Mode::MovingToBestPosition, "movingToBestPosition"},
                {Mode::MovingDueToFailedScan, "movingDueToFailedScan"},
                {Mode::SuccessFinal, "successFinal"},
                {Mode::Failed, "failed"},
                {Mode::None, "none"},
                {Mode::Busy, "busy"},
                {Mode::Stopped, "stopped"}
            };
            Mode status = Mode::None;
            std::string errorStr;
            int currentPositionIndex = 0;
            int movingToPositionIndex = -1;
            int currentSwivellingScanStep = 0;
            int maxSwivellingScanStep = 0;
            erdb::Rdb rdb{"swivelling_scan.status"};
    };
}

namespace jsoncons
{
    template <class Json>
    struct json_type_traits<Json, swscan::Status>
    {
        using allocator_type = typename Json::allocator_type;
        static Json to_json(const swscan::Status& val,
                const allocator_type& alloc = allocator_type())
        {
            Json j(json_object_arg, semantic_tag::none, alloc);
            j.try_emplace("status", val.getStatusStr());
            j.try_emplace("error", val.getErrorStr());
            j.try_emplace("currentPositionIndex", val.getCurrentPositionIndex());
            j.try_emplace("movingToPositionIndex", val.getMovingToPositionIndex());
            j.try_emplace("currentSwivellingScanStep", val.getCurrentSwivellingScanStep());
            j.try_emplace("maxSwivellingScanStep", val.getMaxSwivellingScanStep());
            return j;
        }
    };
}

#endif

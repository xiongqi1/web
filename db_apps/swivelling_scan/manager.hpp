/*
 * Manager
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
#ifndef _SWSCAN_MANAGER_
#define _SWSCAN_MANAGER_

#include <string>
#include <thread>
#include <future>
#include <utility>
#include <map>

#include <asio.hpp>

#include "scan_serving.hpp"
#include "manager_command.hpp"
#include "scan_controller.hpp"
#include "configuration.hpp"
#include "motor_driver.hpp"
#include "swivelling_safety_check.hpp"
#include "status.hpp"


namespace swscan
{
    class Manager: public ScanServing, public ManagerCommand
    {
        public:
            enum class PendingCmd
            {
                None,
                StartSwivellingScan,
                StopSwivellingScan,
                MoveToHomePosition
            };
            Manager(asio::io_context &io, ScanController &sc, MotorDriver &motorDriver,
                SwivellingSafetyCheck &safetyCheck);
            virtual CommandResult startSwivellingScan(StartSwivellingScanParam &param) override;
            virtual CommandResult stopSwivellingScan() override;
            virtual CommandResult moveToHomePosition() override;
            virtual bool swivelNextStep(int stepsDone) override;
            virtual void swivellingScanDone(int bestPositionStep) override;
            virtual void swivellingScanStopped(int currentStep,
                const std::string message) override;
        private:
            Configuration config;
            ScanController &scanController;
            MotorDriver &motorDriver;
            SwivellingSafetyCheck &safetyCheck;
            Status status;
            asio::steady_timer stateHandleTimer;
            PendingCmd pendingCmd = PendingCmd::None;

            bool checkedMotor = false;

            virtual void stateHandle(const asio::error_code& e);
            std::map<int, int> scanPositionList;
            int currentSwivellingScanStep;
            virtual void generateScanPositionlist();

            // throw std::runtime_error on unexpected logic error
            virtual void orderSwivelling(int position);
            struct SwivellingResult
            {
                // true for done, false for in-progress
                bool done;
                // 0 or error code
                int returnCode;
                // message for error case
                std::string errMsg;
            };
            std::thread swivellingThread;
            std::future<SwivellingResult> swivellingFuture;
            virtual SwivellingResult isSwivellingDone();
            virtual SwivellingResult swivel(int position);

            virtual void scheduleStateHandle(int delaySec = 0);
            virtual void doSwivellingScanMoving();

            struct CheckingMotorResult
            {
                // true for done, false for in-progress
                bool done;
                // 0 or error code
                int returnCode;
                // current coordinate if success
                int currCoord;
                // message for error case
                std::string errMsg;
            };
            std::thread checkingMotorThread;
            std::future<CheckingMotorResult> checkingMotorFuture;
            virtual void orderCheckingMotor();
            virtual CheckingMotorResult checkMotor();
            // throw std::runtime_error on unexpected logic error
            virtual CheckingMotorResult isCheckingMotorDone();
            virtual void doCheckingMotor();
    };
}

#endif

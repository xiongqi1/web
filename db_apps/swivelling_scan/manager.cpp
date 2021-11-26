/*
 * Implementing Manager
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

#include <stdexcept>
#include <string>
#include <sstream>

#include "elogger.hpp"
#include "manager.hpp"

namespace swscan
{
    Manager::Manager(asio::io_context &io, ScanController &sc, MotorDriver &md,
        SwivellingSafetyCheck &sCheck)
        :
        config{},
        scanController(sc),
        motorDriver(md),
        safetyCheck(sCheck),
        status{},
        stateHandleTimer{io}
    {
        scanController.setScanServing(*this);

        status.setStatus(Status::Mode::Initialising);
        status.report();

        scheduleStateHandle();
    }

    CommandResult Manager::startSwivellingScan(StartSwivellingScanParam &param)
    {
        auto st = status.getStatus();
        switch (st) {
            case Status::Mode::SwivellingScan:
                return CommandResult(true, "");

            case Status::Mode::SwivellingScanMoving:
            case Status::Mode::SwivellingScanAtPosition:
            case Status::Mode::MovingToBestPosition:
            {
                if (!param.force) {
                    return CommandResult(false, "swivelling scan is in progress");
                }
                break;
            }
            default:
                break;
        }

        pendingCmd = Manager::PendingCmd::StartSwivellingScan;
        scheduleStateHandle();
        return CommandResult(true, "");
    }

    CommandResult Manager::stopSwivellingScan()
    {
        pendingCmd = Manager::PendingCmd::StopSwivellingScan;
        scheduleStateHandle();
        return CommandResult(true, "");
    }

    CommandResult Manager::moveToHomePosition()
    {
        pendingCmd = Manager::PendingCmd::MoveToHomePosition;
        scheduleStateHandle();
        return CommandResult(true, "");
    }

    void Manager::orderSwivelling(int position)
    {
        swscan_log(LOG_NOTICE, "Swivelling to position index %d", position);
        if (checkingMotorThread.joinable() || swivellingThread.joinable()) {
            throw std::runtime_error("Swivelling related threads should not be joinable at this step!");
        }
        std::packaged_task<SwivellingResult()> task(std::bind(&Manager::swivel, this, position));
        swivellingFuture = task.get_future();
        swivellingThread = std::thread(std::move(task));
    }

    Manager::SwivellingResult Manager::isSwivellingDone()
    {
        swscan_log(LOG_NOTICE, "Checking if swivelling is done");
        if (!swivellingThread.joinable() || !swivellingFuture.valid()) {
            swscan_log(LOG_NOTICE, "Either swivelling thread or future is invalid;"
                " considered as not swivelling");
            return SwivellingResult{true, 0, ""};
        }
        if (swivellingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = swivellingFuture.get();
            swivellingThread.join();
            swscan_log(LOG_NOTICE, "swivelling is done, either successful or failed");
            if (result.returnCode == 0) {
                swscan_log(LOG_NOTICE, "swivelling is successful");
            } else {
                swscan_log(LOG_NOTICE, "swivelling failed");
            }
            return result;
        }

        return SwivellingResult{false, 0, ""};
    }

    Manager::SwivellingResult Manager::swivel(int position)
    {
        swscan_log(LOG_NOTICE, "Doing swivelling to position %d", position);
        if (!config.getNoSwivellingSafetyCheck() && !safetyCheck.isSafeForSwivelling()) {
            swscan_log(LOG_NOTICE, "It is unsafe to swivel.");
            return SwivellingResult{true, -1, "It is unsafe to swivel."};
        }
        int rval = motorDriver.goToCoordinate(config.getCoordinateAtPositionIndex(position));
        if (rval != 0) {
            swscan_log(LOG_ERR, "Motor driver failed to swivel.");
            return {true, rval, "Motor driver failed to swivel."};
        }

        return SwivellingResult{true, 0, ""};
    }

    void Manager::orderCheckingMotor()
    {
        swscan_log(LOG_NOTICE, "Checking motor");
        if (checkingMotorThread.joinable() || swivellingThread.joinable()) {
            throw std::runtime_error("Swivelling related threads should not be joinable at this step!");
        }
        std::packaged_task<CheckingMotorResult()> task(std::bind(&Manager::checkMotor, this));
        checkingMotorFuture = task.get_future();
        checkingMotorThread = std::thread(std::move(task));
    }

    Manager::CheckingMotorResult Manager::isCheckingMotorDone()
    {
        swscan_log(LOG_NOTICE, "Checking if checking motor is done");
        if (!checkingMotorThread.joinable() || !checkingMotorFuture.valid()) {
            swscan_log(LOG_NOTICE, "Either checking motor thread or future is invalid");
            throw std::runtime_error("Invalid calling isCheckingMotorDone");
        }
        if (checkingMotorFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = checkingMotorFuture.get();
            checkingMotorThread.join();
            swscan_log(LOG_NOTICE, "checking motor is done, either successful or failed");
            if (result.returnCode == 0) {
                swscan_log(LOG_NOTICE, "checking motor is successful");
            } else {
                swscan_log(LOG_NOTICE, "checking motor failed");
            }
            return result;
        }

        return CheckingMotorResult{false, 0, 0, ""};
    }

    Manager::CheckingMotorResult Manager::checkMotor()
    {
        swscan_log(LOG_NOTICE, "Doing checking motor");

        if (!config.getNoSwivellingSafetyCheck() && !safetyCheck.isSafeForSwivelling()) {
            swscan_log(LOG_NOTICE, "It is unsafe to swivel.");
            return CheckingMotorResult{true, -1,
                config.getCoordinateAtPositionIndex(status.getCurrentPositionIndex()),
                "It is unsafe to swivel."};
        }

        auto res = motorDriver.checkMotor();
        if (res.returnCode != 0) {
            swscan_log(LOG_ERR, "Motor driver failed to check motor.");
            return CheckingMotorResult{true, res.returnCode,
                res.currCoord,
                "Motor driver failed to check motor."};
        }
        return CheckingMotorResult{true, res.returnCode, res.currCoord, ""};
    }

    void Manager::doCheckingMotor()
    {
        orderCheckingMotor();
        scheduleStateHandle(1);

        status.setStatus(Status::Mode::CheckingMotor);
        status.report();
    }

    void Manager::scheduleStateHandle(int delaySec)
    {
        stateHandleTimer.expires_after(std::chrono::seconds(delaySec));
        stateHandleTimer.async_wait(std::bind(&Manager::stateHandle, this,
            std::placeholders::_1));
    }

    void Manager::generateScanPositionlist()
    {
        int currentPos = status.getCurrentPositionIndex();
        if (currentPos < 0) {
            swscan_log(LOG_NOTICE, "Current position is unknown. Starting from home.");
            currentPos = config.getHomePositionIndex();
        }
        int maxPos = config.getPositionNumber() - 1;
        scanPositionList = std::map<int, int>{};
        swscan_log(LOG_NOTICE, "Generating scanPositionList: currentPos=%d, maxPos=%d", currentPos, maxPos);
        // first position is always current position
        int step = 1;
        scanPositionList[step++] = currentPos;
        if ( maxPos - currentPos >= currentPos) {
            for (int pos = currentPos - 1; pos >= 0; pos--) {
                scanPositionList[step++] = pos;
            }
            for (int pos = currentPos + 1; pos <= maxPos; pos++) {
                scanPositionList[step++] = pos;
            }
        } else {
            for (int pos = currentPos + 1; pos <= maxPos; pos++) {
                scanPositionList[step++] = pos;
            }
            for (int pos = currentPos - 1; pos >= 0; pos--) {
                scanPositionList[step++] = pos;
            }
        }
        std::stringstream ss;
        for (const auto& posEl : scanPositionList) {
            ss << "["<< posEl.first << "] = " << posEl.second << "; ";
        }
        swscan_log(LOG_NOTICE, "scanPositionList: %s", ss.str().c_str());
    }

    bool Manager::swivelNextStep(int stepsDone)
    {
        if (pendingCmd == Manager::PendingCmd::None
                && status.getStatus() == Status::Mode::SwivellingScanAtPosition
                && stepsDone == currentSwivellingScanStep
                && currentSwivellingScanStep < config.getPositionNumber()) {
            currentSwivellingScanStep++;
            doSwivellingScanMoving();
            return true;
        }
        swscan_log(LOG_NOTICE, "swivelNextStep is ignored!");
        return false;
    }

    void Manager::doSwivellingScanMoving()
    {
        auto moveToPos = scanPositionList[currentSwivellingScanStep];
        orderSwivelling(moveToPos);
        scheduleStateHandle(1);

        status.setStatus(Status::Mode::SwivellingScanMoving);
        status.setMovingToPositionIndex(moveToPos);
        status.setCurrentSwivellingScanStep(currentSwivellingScanStep);
        status.report();
    }

    void Manager::swivellingScanStopped(int currentStep, const std::string message)
    {
        swscan_log(LOG_NOTICE, "swivellingScanStopped!");
        auto currentSt = status.getStatus();
        if (currentSt == Status::Mode::SwivellingScan
                || currentSt == Status::Mode::SwivellingScanMoving
                || currentSt == Status::Mode::SwivellingScanAtPosition) {
            scanController.stopScan();
            status.setStatus(Status::Mode::Failed);
            status.setErrorStr(message);
            status.report();
        }
    }

    void Manager::swivellingScanDone(int bestPositionStep)
    {
        swscan_log(LOG_NOTICE, "swivellingScanDone bestPositionStep=%d", bestPositionStep);
        if (pendingCmd == Manager::PendingCmd::None
                && status.getStatus() == Status::Mode::SwivellingScanAtPosition
                && bestPositionStep > 0 && bestPositionStep <= config.getPositionNumber()) {
            auto moveToPos = scanPositionList[bestPositionStep];
            config.setBestPositionIndex(moveToPos);
            orderSwivelling(moveToPos);
            scheduleStateHandle(1);

            status.setStatus(Status::Mode::MovingToBestPosition);
            status.setMovingToPositionIndex(moveToPos);
            status.report();
        } else {
            swscan_log(LOG_NOTICE, "swivellingScanDone is ignored; status=%s, bestPositionStep=%d",
                status.getStatusStr().c_str(), bestPositionStep);
            if (bestPositionStep == 0) {
                swscan_log(LOG_ERR, "Scanning failed at all positions.");
                status.setErrorStr("Scanning failed at all positions.");
                // move back home
                orderSwivelling(config.getHomePositionIndex());
                scheduleStateHandle(1);

                status.setStatus(Status::Mode::MovingDueToFailedScan);
                status.setMovingToPositionIndex(config.getHomePositionIndex());
                status.report();
                return;
            } else if (pendingCmd != Manager::PendingCmd::None) {
                swscan_log(LOG_ERR, "There is a pending command.");
                status.setErrorStr("Cancelled due to pending command.");
            } else if (status.getStatus() != Status::Mode::SwivellingScanAtPosition) {
                swscan_log(LOG_ERR, "Invalid state");
                status.setErrorStr("Invalid state");
            } else {
                swscan_log(LOG_ERR, "Unexpected error");
                status.setErrorStr("Unexpected error");
            }
            status.setStatus(Status::Mode::Failed);
            status.report();
            scheduleStateHandle();
        }
    }

    void Manager::stateHandle(const asio::error_code& e)
    {
        if (e) {
            if (e != asio::error::operation_aborted) {
                swscan_log(LOG_ERR, "Error in state handling: %s", e.message().c_str());
            }
            return;
        }
        auto currentSt = status.getStatus();
        switch (currentSt) {
            case Status::Mode::Initialising:
            {
                swscan_log(LOG_NOTICE, "Current state is Initialising");
                int motorInitCode = motorDriver.init(config.getMotorInitialDelayUs(),
                    config.getMotorMaxStepsPerSec());
                if (motorInitCode == 0) {
                    status.setCurrentPositionIndex(config.getBestPositionIndex());
                    if (status.getCurrentPositionIndex() < 0) {
                        swscan_log(LOG_NOTICE, "Current position is unknown");
                    }
                    status.setStatus(Status::Mode::None);
                    status.report();
                    scheduleStateHandle(1);
                } else {
                    swscan_log(LOG_WARNING, "Failed to initialise motor. Code=%d.", motorInitCode);
                    scheduleStateHandle(3);
                }
                break;
            }
            case Status::Mode::CheckingMotor:
            {
                swscan_log(LOG_NOTICE, "In-progress of checking motor");
                auto res = isCheckingMotorDone();
                if (res.done && res.returnCode == 0) {
                    swscan_log(LOG_NOTICE, "Checking motor is complete. Coord = %d", res.currCoord);
                    int posIndex = config.getIndexFromCoordinate(res.currCoord);
                    swscan_log(LOG_NOTICE, "Current position index: %d", posIndex);
                    checkedMotor = true;
                    status.setCurrentPositionIndex(posIndex);
                    status.setStatus(Status::Mode::None);
                    status.report();
                    scheduleStateHandle();
                } else if (res.done && res.returnCode != 0) {
                    swscan_log(LOG_NOTICE, "Errors in checking motor. Coord = %d", res.currCoord);
                    int posIndex = config.getIndexFromCoordinate(res.currCoord);
                    swscan_log(LOG_NOTICE, "Current position index: %d", posIndex);
                    status.setCurrentPositionIndex(posIndex);
                    status.setStatus(Status::Mode::Failed);
                    status.setErrorStr("Error code: " + std::to_string(res.returnCode)
                        + ". " + res.errMsg);
                    status.report();
                    // could not process any pending command if checking motor failed
                    pendingCmd = Manager::PendingCmd::None;
                    scheduleStateHandle();
                } else {
                    scheduleStateHandle(1);
                }
                break;
            }
            case Status::Mode::SwivellingScanMoving:
            case Status::Mode::MovingToBestPosition:
            case Status::Mode::MovingDueToFailedScan:
            case Status::Mode::Busy:
            {
                swscan_log(LOG_NOTICE, "In one of moving states");
                // check moving is complete
                auto movRes = isSwivellingDone();
                if (movRes.done && movRes.returnCode == 0) {
                    // complete
                    swscan_log(LOG_NOTICE, "Moving is complete");
                    if (currentSt == Status::Mode::Busy) {
                        status.setStatus(Status::Mode::None);
                    } else if (currentSt == Status::Mode::SwivellingScanMoving) {
                        status.setStatus(Status::Mode::SwivellingScanAtPosition);
                    } else if (currentSt == Status::Mode::MovingToBestPosition) {
                        status.setStatus(Status::Mode::SuccessFinal);
                    } else if (currentSt == Status::Mode::MovingDueToFailedScan) {
                        status.setStatus(Status::Mode::Failed);
                    }
                    status.setCurrentPositionIndex(status.getMovingToPositionIndex());
                    status.setMovingToPositionIndex(-1);
                    status.report();

                    scheduleStateHandle();
                } else if (movRes.done && movRes.returnCode != 0) {
                    // error
                    swscan_log(LOG_NOTICE, "Errors in moving");
                    if (currentSt == Status::Mode::SwivellingScanMoving) {
                        scanController.stopScan();
                    }
                    status.setStatus(Status::Mode::Failed);
                    status.setErrorStr("Error code: " + std::to_string(movRes.returnCode)
                        + ". " + movRes.errMsg);
                    status.report();
                    scheduleStateHandle();
                } else {
                    // moving is in progress, schedule to check with some delay
                    scheduleStateHandle(1);
                }
                break;
            }
            case Status::Mode::None:
            case Status::Mode::SuccessFinal:
            case Status::Mode::Failed:
            case Status::Mode::Stopped:
            case Status::Mode::SwivellingScan:
            case Status::Mode::SwivellingScanAtPosition:
            {
                swscan_log(LOG_NOTICE, "In one of stationary states");
                switch (pendingCmd) {
                    case Manager::PendingCmd::StartSwivellingScan:
                    {
                        swscan_log(LOG_NOTICE, "Processing command StartSwivellingScan");

                        try {
                            scanController.checkScanConditions();
                        } catch (swscan::ScanController::invalid_scan_conditions &e) {
                            swscan_log(LOG_NOTICE, e.what());
                            pendingCmd = Manager::PendingCmd::None;
                            status.setStatus(Status::Mode::Failed);
                            status.setErrorStr(e.what());
                            status.report();
                            break;
                        }

                        if (!checkedMotor) {
                            swscan_log(LOG_NOTICE, "Need checking motor");
                            doCheckingMotor();
                            break;
                        }
                        swscan_log(LOG_NOTICE, "Checking motor's already done");

                        pendingCmd = Manager::PendingCmd::None;

                        if (currentSt == Status::Mode::SwivellingScanAtPosition) {
                            scanController.stopScan();
                        }

                        currentSwivellingScanStep = 0;
                        generateScanPositionlist();
                        scanController.initialiseScan(config.getPositionNumber());

                        status.setStatus(Status::Mode::SwivellingScan);
                        status.setCurrentSwivellingScanStep(0);
                        status.setMaxSwivellingScanStep(config.getPositionNumber());
                        status.report();

                        scheduleStateHandle();
                        break;
                    }
                    case Manager::PendingCmd::StopSwivellingScan:
                    {
                        swscan_log(LOG_NOTICE, "Processing command StopSwivellingScan");
                        if (currentSt == Status::Mode::SwivellingScanAtPosition) {
                            scanController.stopScan();
                            status.setStatus(Status::Mode::Stopped);
                            status.report();
                        }
                        pendingCmd = Manager::PendingCmd::None;
                        break;
                    }
                    case Manager::PendingCmd::MoveToHomePosition:
                    {
                        swscan_log(LOG_NOTICE, "Processing command MoveToHomePosition");
                        if (currentSt == Status::Mode::SwivellingScanAtPosition) {
                            scanController.stopScan();
                        }

                        if (!checkedMotor) {
                            swscan_log(LOG_NOTICE, "Need checking motor");
                            doCheckingMotor();
                            break;
                        }
                        swscan_log(LOG_NOTICE, "Checking motor's already done");

                        orderSwivelling(config.getHomePositionIndex());
                        scheduleStateHandle(1);

                        status.setStatus(Status::Mode::Busy);
                        status.setMovingToPositionIndex(config.getHomePositionIndex());
                        status.report();
                        pendingCmd = Manager::PendingCmd::None;
                        break;
                    }
                    case Manager::PendingCmd::None:
                    {
                        if (currentSt == Status::Mode::SwivellingScan) {
                            // go to first position in the generated position list
                            // set state to SwivellingScanMoving
                            currentSwivellingScanStep = 1;
                            doSwivellingScanMoving();
                        } else if (currentSt == Status::Mode::SwivellingScanAtPosition) {
                            if (!scanController.isScanningForStep(currentSwivellingScanStep)) {
                                scanController.scanNextStep();
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
}

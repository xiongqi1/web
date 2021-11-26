/*
 * Mock motor driver
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
#ifndef _SWSCAN_MOCK_MOTOR_DRIVER_
#define _SWSCAN_MOCK_MOTOR_DRIVER_

#include <iostream>
#include <chrono>
#include <thread>

#include "motor_driver.hpp"

namespace swscan
{
    class MockMotorDriver: public MotorDriver
    {
        public:
            virtual int init(int initialDelayUs, int maxStepPerSecond) override
            {
                std::cout << "INIT initialDelayUs: " << initialDelayUs
                    << ", maxStepPerSecond: " << maxStepPerSecond << std::endl;
                return 0;
            }
            virtual int goToCoordinate(int coordinate) override
            {
                std::cout << "GOTO coordinate: " << coordinate << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::cout << "REACHED coordinate: " << coordinate << std::endl;
                return 0;
            }
            virtual MotorDriver::CheckMotorResult checkMotor() override
            {
                std::cout << "CHECK MOTOR" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(10));
                std::cout << "CHECK MOTOR: DONE" << std::endl;
                return MotorDriver::CheckMotorResult{0, 0};
            }
            ~MockMotorDriver()
            {
                std::cout << "Releasing resources ..." << std::endl;
            }
    };
}
#endif

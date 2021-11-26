/*
 * STM32 motor driver
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
#include "stm32_motor_driver.hpp"

namespace swscan
{
    int Stm32MotorDriver::init(int initialDelayUs, int maxStepPerSecond)
    {
        swscan_log(LOG_NOTICE, "Calling init");
        int rval = motorImpl.init(const_cast<char*>(""));
        if (rval != EXIT_SUCCESS) {
            swscan_log(LOG_ERR, "Failed in init");
            return -1;
        }
        swscan_log(LOG_NOTICE, "Calling set_initial_delay");
        rval = motorImpl.set_initial_delay(initialDelayUs/1000);
        if (rval != EXIT_SUCCESS) {
            swscan_log(LOG_ERR, "Failed in set_initial_delay");
            return -1;
        }
        swscan_log(LOG_NOTICE, "Calling set_top_speed");
        rval = motorImpl.set_top_speed(maxStepPerSecond);
        if (rval != EXIT_SUCCESS) {
            swscan_log(LOG_ERR, "Failed in set_top_speed");
            return -1;
        }

        return 0;
    }

    int Stm32MotorDriver::goToCoordinate(int coordinate)
    {
        swscan_log(LOG_NOTICE, "Calling goto_position");
        int rval = motorImpl.goto_position(static_cast<float>(coordinate));
        if (rval != EXIT_SUCCESS) {
            swscan_log(LOG_ERR, "Failed in goto_position");
            return -1;
        }
        return 0;
    }

    MotorDriver::CheckMotorResult Stm32MotorDriver::checkMotor()
    {
        swscan_log(LOG_NOTICE, "Calling set_home_position(0)");
        int rval = motorImpl.set_home_position(0);
        if (rval != EXIT_SUCCESS) {
            swscan_log(LOG_ERR, "Failed in set_home_position(0)");
            // the stm32 driver does not indicate current position;
            // it would be at home in most cases
            return MotorDriver::CheckMotorResult{-1, 0};
        }
        // when success, it is always at home
        return MotorDriver::CheckMotorResult{0, 0};
    }
}
